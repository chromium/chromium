// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/environment/stars.h"

#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/animation/tween.h"

namespace vr {

namespace {
constexpr size_t kNumStars = 500lu;

// Position & opacity.
constexpr size_t kFloatsPerStarVertex = 5lu;
constexpr size_t kDataStride = kFloatsPerStarVertex * sizeof(float);
constexpr size_t kOpacityDataOffset = 3 * sizeof(float);
constexpr size_t kPhaseDataOffset = 4 * sizeof(float);
constexpr size_t kVerticesPerStar = 6lu;
constexpr size_t kTrianglesPerStar = kVerticesPerStar - 1lu;

constexpr float kMinStarWidth = 0.002f;
constexpr float kMaxStarWidth = 0.007f;

constexpr float kMaxStarDegrees = 70.0f;
constexpr float kOpacityNoiseScale = 5.0f;

float g_vertices[kNumStars * kFloatsPerStarVertex * kVerticesPerStar];
GLushort g_indices[kNumStars * kTrianglesPerStar * 3lu];

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform float u_Time;
  attribute vec4 a_Position;
  attribute float a_Opacity;
  attribute float a_Phase;
  varying mediump float v_Opacity;

  float Twinkle(float t) {
    return sin(t + 5.0 * cos(t + 3.0)) * 0.25 + 0.75;
  }

  void main() {
    float twinkle = 1.0;
    v_Opacity = a_Opacity * Twinkle(u_Time + a_Phase);
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  varying mediump float v_Opacity;
  void main() {
    mediump vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
    // This is somewhat arbitrary, but makes the bright part of the star larger.
    gl_FragColor = color * v_Opacity * v_Opacity;
  }
);
// clang-format on

}  // namespace

Stars::Stars() = default;
Stars::~Stars() = default;

void Stars::Render(UiElementRenderer* renderer,
                   const CameraModel& camera) const {
  renderer->DrawStars((last_frame_time() - start_time_).InSecondsF(),
                      camera.view_proj_matrix * world_space_transform());
}

void Stars::Initialize(SkiaSurfaceProvider* provider) {
  start_time_ = base::TimeTicks::Now();
}

Stars::Renderer::Renderer() : BaseRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  time_handle_ = glGetUniformLocation(program_handle_, "u_Time");
  opacity_handle_ = glGetAttribLocation(program_handle_, "a_Opacity");
  phase_handle_ = glGetAttribLocation(program_handle_, "a_Phase");
}

Stars::Renderer::~Renderer() {}

// TODO(vollick): use time |t| to animate the stars.
void Stars::Renderer::Draw(float t, const gfx::Transform& view_proj_matrix) {
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  glUniform1f(time_handle_, t * 0.5);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, 3, GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(0));
  glEnableVertexAttribArray(position_handle_);

  // Set up opacity attribute.
  glVertexAttribPointer(opacity_handle_, 1, GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(kOpacityDataOffset));
  glEnableVertexAttribArray(opacity_handle_);

  // Set up phase attribute
  glVertexAttribPointer(phase_handle_, 1, GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(kPhaseDataOffset));
  glEnableVertexAttribArray(phase_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);

  glDrawElements(GL_TRIANGLES, base::size(g_indices), GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(opacity_handle_);
  glDisableVertexAttribArray(phase_handle_);
}

GLuint Stars::Renderer::vertex_buffer_ = 0;
GLuint Stars::Renderer::index_buffer_ = 0;

void Stars::Renderer::CreateBuffers() {
  gfx::Point3F local_star_geometry[kVerticesPerStar];
  for (size_t i = 1; i < kVerticesPerStar; ++i) {
    float k = 2.0f / (kVerticesPerStar - 1.0f);
    k *= i - 1;
    local_star_geometry[i].set_x(cos(base::kPiFloat * k));
    local_star_geometry[i].set_y(sin(base::kPiFloat * k));
  }
  size_t cur_vertex = 0;
  size_t cur_index = 0;
  for (size_t i = 0; i < kNumStars; ++i) {
    float x_rot = gfx::Tween::FloatValueBetween(
        base::RandDouble(), -kMaxStarDegrees, kMaxStarDegrees);
    float y_rot = gfx::Tween::FloatValueBetween(
        base::RandDouble(), -kMaxStarDegrees, kMaxStarDegrees);
    float size = gfx::Tween::FloatValueBetween(base::RandDouble(),
                                               kMinStarWidth, kMaxStarWidth);
    float phase = gfx::Tween::FloatValueBetween(base::RandDouble(), 0.0f,
                                                2.0f * base::kPiFloat);
    float opacity =
        1.0 - gfx::Vector2dF(x_rot, y_rot).Length() / kMaxStarDegrees;

    float opacity_noise = (base::RandDouble() - 0.5);
    opacity_noise *= opacity_noise * opacity_noise * kOpacityNoiseScale;
    opacity = base::ClampToRange(opacity + opacity_noise, 0.0f, 1.0f);

    gfx::Transform local;
    local.RotateAboutYAxis(x_rot);
    local.RotateAboutXAxis(y_rot);
    local.Translate3d({0, 0, -kSkyDistance});
    local.Scale3d(size * kSkyDistance, size * kSkyDistance, 1.0f);
    for (size_t j = 0; j < kVerticesPerStar - 1; j++, cur_index += 3) {
      size_t j_next = (j + 1) % (kVerticesPerStar - 1);
      g_indices[cur_index] = cur_vertex;
      g_indices[cur_index + 1] = cur_vertex + j + 1;
      g_indices[cur_index + 2] = cur_vertex + j_next + 1;
    }
    for (size_t j = 0; j < kVerticesPerStar; j++, cur_vertex++) {
      gfx::Point3F p = local_star_geometry[j];
      local.TransformPoint(&p);
      g_vertices[cur_vertex * kFloatsPerStarVertex] = p.x();
      g_vertices[cur_vertex * kFloatsPerStarVertex + 1] = p.y();
      g_vertices[cur_vertex * kFloatsPerStarVertex + 2] = p.z();
      g_vertices[cur_vertex * kFloatsPerStarVertex + 3] = j ? 0 : opacity;
      g_vertices[cur_vertex * kFloatsPerStarVertex + 4] = phase;
    }
  }

  GLuint buffers[2];
  glGenBuffers(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, base::size(g_vertices) * sizeof(float),
               g_vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               base::size(g_indices) * sizeof(GLushort), g_indices,
               GL_STATIC_DRAW);
}

}  // namespace vr
