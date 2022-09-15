// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/laser.h"

#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "device/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kFragmentShader = SHADER(
  varying mediump vec2 v_TexCoordinate;
  uniform sampler2D texture_unit;
  uniform lowp vec4 color;
  uniform mediump float fade_point;
  uniform mediump float fade_end;
  uniform mediump float u_Opacity;

  void main() {
    mediump vec2 uv = v_TexCoordinate;
    mediump float front_fade_factor = 1.0 -
        clamp(1.0 - (uv.y - fade_point) / (1.0 - fade_point), 0.0, 1.0);
    mediump float back_fade_factor =
        clamp((uv.y - fade_point) / (fade_end - fade_point), 0.0, 1.0);
    mediump float total_fade = front_fade_factor * back_fade_factor;
    lowp vec4 texture_color = texture2D(texture_unit, uv);
    lowp vec4 final_color = color * texture_color;
    lowp float final_opacity = final_color.w * total_fade * u_Opacity;
    gl_FragColor = vec4(final_color.xyz * final_opacity, final_opacity);
  }
);
// clang-format on

// Laser texture data, 48x1 RGBA (not premultiplied alpha).
// TODO(mthiesse): As we add more resources for VR Shell, we should put them
// in Chrome's resource files.
static const unsigned char kLaserData[] =
    "\xff\xff\xff\x01\xff\xff\xff\x02\xbf\xbf\xbf\x04\xcc\xcc\xcc\x05\xdb\xdb"
    "\xdb\x07\xcc\xcc\xcc\x0a\xd8\xd8\xd8\x0d\xd2\xd2\xd2\x11\xce\xce\xce\x15"
    "\xce\xce\xce\x1a\xce\xce\xce\x1f\xcd\xcd\xcd\x24\xc8\xc8\xc8\x2a\xc9\xc9"
    "\xc9\x2f\xc9\xc9\xc9\x34\xc9\xc9\xc9\x39\xc9\xc9\xc9\x3d\xc8\xc8\xc8\x41"
    "\xcb\xcb\xcb\x44\xee\xee\xee\x87\xfa\xfa\xfa\xc8\xf9\xf9\xf9\xc9\xf9\xf9"
    "\xf9\xc9\xfa\xfa\xfa\xc9\xfa\xfa\xfa\xc9\xf9\xf9\xf9\xc9\xf9\xf9\xf9\xc9"
    "\xfa\xfa\xfa\xc8\xee\xee\xee\x87\xcb\xcb\xcb\x44\xc8\xc8\xc8\x41\xc9\xc9"
    "\xc9\x3d\xc9\xc9\xc9\x39\xc9\xc9\xc9\x34\xc9\xc9\xc9\x2f\xc8\xc8\xc8\x2a"
    "\xcd\xcd\xcd\x24\xce\xce\xce\x1f\xce\xce\xce\x1a\xce\xce\xce\x15\xd2\xd2"
    "\xd2\x11\xd8\xd8\xd8\x0d\xcc\xcc\xcc\x0a\xdb\xdb\xdb\x07\xcc\xcc\xcc\x05"
    "\xbf\xbf\xbf\x04\xff\xff\xff\x02\xff\xff\xff\x01";

static constexpr float kFadeEnd = 0.535f;
static constexpr float kFadePoint = 0.5335f;
static constexpr float kLaserColor[] = {1.0f, 1.0f, 1.0f, 0.5f};
static constexpr int kLaserDataWidth = 48;
static constexpr int kLaserDataHeight = 1;

}  // namespace

Laser::Laser(Model* model) : model_(model) {
  SetName(kLaser);
}

Laser::~Laser() = default;

void Laser::Render(UiElementRenderer* renderer,
                   const CameraModel& model) const {
  // Find the length of the beam (from hand to target).
  const float laser_length =
      std::sqrt(model_->primary_controller().laser_origin.SquaredDistanceTo(
          model_->reticle.target_point));

  // Build a beam, originating from the origin.
  gfx::Transform mat;

  // Move the beam half its height so that its end sits on the origin.
  mat.PostTranslate3d(0.0f, 0.5f, 0.0f);
  mat.PostScale3d(kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  const gfx::Quaternion quat(gfx::Vector3dF(1.0f, 0.0f, 0.0f),
                             -base::kPiDouble / 2);
  gfx::Transform rotation_mat(quat);
  mat = rotation_mat * mat;

  const gfx::Vector3dF beam_direction =
      model_->reticle.target_point - model_->primary_controller().laser_origin;

  gfx::Transform beam_direction_mat(
      gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f), beam_direction));

  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  gfx::Transform face_transform;
  gfx::Transform transform;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = base::kPiFloat * 2 * i / faces;
    const gfx::Quaternion rot({0.0f, 0.0f, 1.0f}, angle);
    face_transform = beam_direction_mat * gfx::Transform(rot) * mat;

    // Move the beam origin to the hand.
    face_transform.PostTranslate3d(
        model_->primary_controller().laser_origin.OffsetFromOrigin());
    transform =
        model.view_proj_matrix * world_space_transform() * face_transform;
    renderer->DrawLaser(computed_opacity(), transform);
  }
}

Laser::Renderer::Renderer()
    : BaseQuadRenderer(Reticle::Renderer::VertexShader(), kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  texture_unit_handle_ = glGetUniformLocation(program_handle_, "texture_unit");
  color_handle_ = glGetUniformLocation(program_handle_, "color");
  fade_point_handle_ = glGetUniformLocation(program_handle_, "fade_point");
  fade_end_handle_ = glGetUniformLocation(program_handle_, "fade_end");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");

  glGenTextures(1, &texture_data_handle_);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle_);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kLaserDataWidth, kLaserDataHeight, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, kLaserData);
  SetTexParameters(GL_TEXTURE_2D);
}

Laser::Renderer::~Renderer() = default;

void Laser::Renderer::Draw(float opacity,
                           const gfx::Transform& view_proj_matrix) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  // Link texture data with texture unit.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle_);

  glUniform1i(texture_unit_handle_, 0);
  glUniform4f(color_handle_, kLaserColor[0], kLaserColor[1], kLaserColor[2],
              kLaserColor[3]);
  glUniform1f(fade_point_handle_, kFadePoint);
  glUniform1f(fade_end_handle_, kFadeEnd);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
