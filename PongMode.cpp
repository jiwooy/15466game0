#include "PongMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
using namespace std;

PongMode::PongMode() {

	Ball *b = new Ball();
	b->ball = glm::vec2(0.0f, 0.0f);
	b->ball_velocity = glm::vec2(-1.0f, 0.0f);
	b->ball_radius = glm::vec2(0.2f, 0.2f);
	b->alive = 0.0;
	b->trail_color = (glm::u8vec4((0x000000ff >> 24) & 0xff, (0x000000ff >> 16) & 0xff, (0x000000ff >> 8) & 0xff, (0x000000ff) & 0xff ));
	balls.push_back(b);

	//set up trail as if ball has been here for 'forever':
	balls[0]->ball_trail.clear();
	balls[0]->ball_trail.emplace_back(balls[0]->ball, trail_length);
	balls[0]->ball_trail.emplace_back(balls[0]->ball, 0.0f);
	
	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PongMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

PongMode::~PongMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool PongMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
		);
		left_paddle.y = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y;
	}

	return false;
}

void PongMode::newBall() {
	float lo = 0.03f;
	float hi = 0.1f;
	float r = lo + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/(hi-lo)));
	Ball *b = new Ball();
	if (rand() % 2 == 1) {
		b->ball_radius = glm::vec2(0.2f + r, 0.2f + r);
	} else {
		b->ball_radius = glm::vec2(0.2f - r, 0.2f - r);
	}
	int rint = rand();
	if (rint % 2 == 1) {
		b->ball_velocity = glm::vec2(-1.0f, 0.0f);
		b->trail_color = player1_trail;
	} else {
		b->ball_velocity = glm::vec2(1.0f, 0.0f);
		b->trail_color = player2_trail;
	}
	b->ball = glm::vec2(0.0f, 0.0f);
	b->ball_trail.clear();
	b->ball_trail.emplace_back(b->ball, trail_length);
	b->ball_trail.emplace_back(b->ball, 0.0f);
	
	balls.push_back(b);
}

void PongMode::update(float elapsed) {

	static std::mt19937 mt; //mersenne twister pseudo-random number generator

	time += elapsed;
	if (time > threshold && balls.size() < 6) {
		newBall();
		threshold += 6.0f;
	}

	//----- paddle update -----

	{ //right player ai:
		ai_offset_update -= elapsed;
		if (ai_offset_update < elapsed) {
			//update again in [0.5,1.0) seconds:
			ai_offset_update = (mt() / float(mt.max())) * 0.5f + 0.5f;
			ai_offset = (mt() / float(mt.max())) * 2.5f - 1.25f;
		}
		int closest = 0;
		double dist = INT_MAX;
		for(int i = 0; i < balls.size(); i++) {
			if (balls[i]->ball_velocity.x > 0 && balls[i]->trail_color == player1_trail) {
				double newDist = sqrt(std::pow(right_paddle.x - balls[i]->ball.x, 2) + std::pow(right_paddle.y - balls[i]->ball.y, 2) * 1.0);
				if (newDist < dist) {
					dist = newDist;
					closest = i;
				}
			}
		}
		if (right_paddle.y < balls[closest]->ball.y + ai_offset) {
				right_paddle.y = std::min(balls[closest]->ball.y + ai_offset, right_paddle.y + 10.0f * elapsed);
			} else {
				right_paddle.y = std::max(balls[closest]->ball.y + ai_offset, right_paddle.y - 10.0f * elapsed);
		}
	}

	//clamp paddles to court:
	right_paddle.y = std::max(right_paddle.y, -court_radius.y + paddle_radius.y);
	right_paddle.y = std::min(right_paddle.y,  court_radius.y - paddle_radius.y);

	left_paddle.y = std::max(left_paddle.y, -court_radius.y + paddle_radius.y);
	left_paddle.y = std::min(left_paddle.y,  court_radius.y - paddle_radius.y);

	//----- ball update -----
	float speed_mult;
	for (int i = 0; i < balls.size(); i++) {
		balls[i]->alive += elapsed;
		speed_mult = 4.0f * std::pow(2.0f, balls[i]->alive / 5.0f);
		speed_mult = std::min(speed_mult, 10.0f);
		balls[i]->ball += elapsed * speed_mult * balls[i]->ball_velocity;
	}

	//---- collision handling ----

	//paddles:
	auto paddle_vs_ball = [this](glm::vec2 const &paddle, Ball *ball) {
		//compute area of overlap:
		glm::u8vec4 new_color;
		if (paddle.x == -court_radius.x + 0.5f) {
			new_color = player1_trail;
		} else if (paddle.x == court_radius.x - 0.5f) {
			new_color = player2_trail;
		}
		glm::vec2 min = glm::max(paddle - paddle_radius, ball->ball - ball->ball_radius);
		glm::vec2 max = glm::min(paddle + paddle_radius, ball->ball + ball->ball_radius);
		//if no overlap, no collision:
		if (min.x > max.x || min.y > max.y)  {
			return;
		}

		if (max.x - min.x > max.y - min.y) {
			//wider overlap in x => bounce in y direction:
			if (ball->ball.y > paddle.y) {
				ball->ball.y = paddle.y + paddle_radius.y + ball->ball_radius.y;
				ball->ball_velocity.y = std::abs(ball->ball_velocity.y);
			} else {
				ball->ball.y = paddle.y - paddle_radius.y - ball->ball_radius.y;
				ball->ball_velocity.y = -std::abs(ball->ball_velocity.y);
			}
			ball->trail_color = new_color;
		} else {
			//wider overlap in y => bounce in x direction:
			if (ball->ball.x > paddle.x) {
				ball->ball.x = paddle.x + paddle_radius.x + ball->ball_radius.x;
				ball->ball_velocity.x = std::abs(ball->ball_velocity.x);
			} else {
				ball->ball.x = paddle.x - paddle_radius.x - ball->ball_radius.x;
				ball->ball_velocity.x = -std::abs(ball->ball_velocity.x);
			}
			//warp y velocity based on offset from paddle center:
			float vel = (ball->ball.y - paddle.y) / (paddle_radius.y + ball->ball_radius.y);
			ball->ball_velocity.y = glm::mix(ball->ball_velocity.y, vel, 0.75f);
			ball->trail_color = new_color;
		}
		
	};
	
	for (int i = 0; i < balls.size(); i ++) {
		paddle_vs_ball(left_paddle, balls[i]);
		paddle_vs_ball(right_paddle, balls[i]);
	}
	
	//court walls:
	for (int i = 0; i < balls.size(); i++) {
		if (balls[i]->ball.y > court_radius.y - balls[i]->ball_radius.y) {
			balls[i]->ball.y = court_radius.y - balls[i]->ball_radius.y;
			if (balls[i]->ball_velocity.y > 0.0f) {
				balls[i]->ball_velocity.y = -balls[i]->ball_velocity.y;
			}
		}
		if (balls[i]->ball.y < -court_radius.y + balls[i]->ball_radius.y) {
			balls[i]->ball.y = -court_radius.y + balls[i]->ball_radius.y;
			if (balls[i]->ball_velocity.y < 0.0f) {
				balls[i]->ball_velocity.y = -balls[i]->ball_velocity.y;
			}
		}

		if (balls[i]->ball.x > court_radius.x - balls[i]->ball_radius.x) {
			balls[i]->ball.x = court_radius.x - balls[i]->ball_radius.x;
			if (balls[i]->ball_velocity.x > 0.0f) {
				balls[i]->ball_velocity.x = -balls[i]->ball_velocity.x;
			}
		}
		if (balls[i]->ball.x < -court_radius.x + balls[i]->ball_radius.x) {
			balls[i]->ball.x = -court_radius.x + balls[i]->ball_radius.x;
			if (balls[i]->ball_velocity.x < 0.0f) {
				balls[i]->ball_velocity.x = -balls[i]->ball_velocity.x;
			}
		}
	}

	//----- rainbow trails -----

	//age up all locations in ball trail:
	for (int i = 0; i < balls.size(); i++) {
		for (auto &t : balls[i]->ball_trail) {
			t.z += elapsed;
		}
		//store fresh location at back of ball trail:
		balls[i]->ball_trail.emplace_back(balls[i]->ball, 0.0f);

		//trim any too-old locations from back of trail:
		//NOTE: since trail drawing interpolates between points, only removes back element if second-to-back element is too old:
		while (balls[i]->ball_trail.size() >= 2 && balls[i]->ball_trail[1].z > trail_length) {
			balls[i]->ball_trail.pop_front();
		}
	}
}

void PongMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x171714ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xffffffff);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0x604d29ff);
	const glm::u8vec4 player1_color = HEX_TO_U8VEC4(0x008DECff);
	const glm::u8vec4 player2_color = HEX_TO_U8VEC4(0xEC0040ff);
	const std::vector< glm::u8vec4 > rainbow_colors = {
		HEX_TO_U8VEC4(0x604d29ff), HEX_TO_U8VEC4(0x624f29fc), HEX_TO_U8VEC4(0x69542df2),
		HEX_TO_U8VEC4(0x6a552df1), HEX_TO_U8VEC4(0x6b562ef0), HEX_TO_U8VEC4(0x6b562ef0),
		HEX_TO_U8VEC4(0x6d572eed), HEX_TO_U8VEC4(0x6f592feb), HEX_TO_U8VEC4(0x725b31e7),
		HEX_TO_U8VEC4(0x745d31e3), HEX_TO_U8VEC4(0x755e32e0), HEX_TO_U8VEC4(0x765f33de),
		HEX_TO_U8VEC4(0x7a6234d8), HEX_TO_U8VEC4(0x826838ca), HEX_TO_U8VEC4(0x977840a4),
		HEX_TO_U8VEC4(0x96773fa5), HEX_TO_U8VEC4(0xa07f4493), HEX_TO_U8VEC4(0xa1814590),
		HEX_TO_U8VEC4(0x9e7e4496), HEX_TO_U8VEC4(0xa6844887), HEX_TO_U8VEC4(0xa9864884),
		HEX_TO_U8VEC4(0xad8a4a7c),
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	//shadows for everything (except the trail):

	glm::vec2 s = glm::vec2(0.0f,-shadow_offset);

	/*
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(left_paddle+s, paddle_radius, shadow_color);
	draw_rectangle(right_paddle+s, paddle_radius, shadow_color);
	draw_rectangle(ball+s, ball_radius, shadow_color);
	*/
	
	//ball's trail:
	for (int j = 0; j < balls.size(); j++) {
		if (balls[j]->ball_trail.size() >= 2) {
			//start ti at second element so there is always something before it to interpolate from:
			std::deque< glm::vec3 >::iterator ti = balls[j]->ball_trail.begin() + 1;
			//draw trail from oldest-to-newest:
			for (uint32_t i = uint32_t(rainbow_colors.size())-1; i < rainbow_colors.size(); --i) {
				//time at which to draw the trail element:
				float t = (i + 1) / float(rainbow_colors.size()) * trail_length;
				//advance ti until 'just before' t:
				while (ti != balls[j]->ball_trail.end() && ti->z > t) ++ti;
				//if we ran out of tail, stop drawing:
				if (ti == balls[j]->ball_trail.end()) break;
				//interpolate between previous and current trail point to the correct time:
				glm::vec3 a = *(ti-1);
				glm::vec3 b = *(ti);
				glm::vec2 at = (t - a.z) / (b.z - a.z) * (glm::vec2(b) - glm::vec2(a)) + glm::vec2(a);
				//draw:
				draw_rectangle(at, balls[j]->ball_radius, balls[j]->trail_color);
				//draw_rectangle(at, ball_radius, rainbow_colors[7]);
			}
		}
	}
	//solid objects:

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//paddles:
	draw_rectangle(left_paddle, paddle_radius, player1_color);
	draw_rectangle(right_paddle, paddle_radius, player2_color);
	

	//ball:
	for (int i = 0; i < balls.size(); i++) {
		draw_rectangle(balls[i]->ball, balls[i]->ball_radius, fg_color);
	}

	//scores:
	glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	for (uint32_t i = 0; i < left_score; ++i) {
		draw_rectangle(glm::vec2( -court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
	}
	for (uint32_t i = 0; i < right_score; ++i) {
		draw_rectangle(glm::vec2( court_radius.x - (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
	}



	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	/*
	GL.Enable (EnableCap.ScissorTest);
	GL.Scissor (-court_radius.x + 0.5f, court_radius.x - 0.5f, court_radius.x, court_radius.y);
	GL.Clear (ClearBufferMask.ColorBufferBit);
	GL.Disable (EnableCap.ScissorTest);
	*/

	float rightx = 597 / 640;
	float paddleScale = 20 / 640;
	float leftx = 30 / 640;
	float ySize = 200 / 480;
	//cout << drawable_size.x << " " << drawable_size.y;
	//right paddle area clear
	glEnable(GL_SCISSOR_TEST);
	glScissor((GLint)597, (GLint)-court_radius.y,
			(GLsizei)20, (GLsizei)(court_radius.y * 200));
	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);

	//left paddle area clear
	glEnable(GL_SCISSOR_TEST);
	glScissor((GLint)-court_radius.x + 30, (GLint)-court_radius.y,
			(GLsizei)(20), (GLsizei)court_radius.y * 200);
	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}
