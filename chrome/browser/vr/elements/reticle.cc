// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/reticle.h"

#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/browser/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec4 a_Position;
  varying vec2 v_TexCoordinate;

  void main() {
    v_TexCoordinate = vec2(0.5 + a_Position[0], 0.5 - a_Position[1]);
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision mediump float;
  varying mediump vec2 v_TexCoordinate;
  uniform lowp vec4 color;
  uniform mediump float ring_diameter;
  uniform mediump float inner_hole;
  uniform mediump float inner_ring_end;
  uniform mediump float inner_ring_thickness;
  uniform mediump float mid_ring_end;
  uniform mediump float mid_ring_opacity;
  uniform mediump float opacity;

  void main() {
    mediump float r = length(v_TexCoordinate - vec2(0.5, 0.5));
    mediump float color_radius = inner_ring_end * ring_diameter;
    mediump float color_feather_radius = inner_ring_thickness * ring_diameter;
    mediump float hole_radius =
        inner_hole * ring_diameter - color_feather_radius;
    mediump float color1 = clamp(
        1.0 - (r - color_radius) / color_feather_radius, 0.0, 1.0);
    mediump float hole_alpha =
        clamp((r - hole_radius) / color_feather_radius, 0.0, 1.0);

    mediump float black_radius = mid_ring_end * ring_diameter;
    mediump float black_feather =
        1.0 / (ring_diameter * 0.5 - black_radius);
    mediump float black_alpha_factor =
        mid_ring_opacity * (1.0 - (r - black_radius) * black_feather);
    mediump float alpha = clamp(
        min(hole_alpha, max(color1, black_alpha_factor)) * opacity, 0.0, 1.0);
    lowp vec3 color_rgb = color1 * color.xyz;
    gl_FragColor = vec4(color_rgb * color.w * alpha, color.w * alpha);
  }
);
// clang-format on

constexpr float kRingDiameter = 1.0f;
constexpr float kInnerHole = 0.0f;
constexpr float kInnerRingEnd = 0.177f;
constexpr float kInnerRingThickness = 0.14f;
constexpr float kMidRingEnd = 0.177f;
constexpr float kMidRingOpacity = 0.22f;
constexpr float kReticleColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

}  // namespace

Reticle::Reticle(UiScene* scene, Model* model) : scene_(scene), model_(model) {
  SetName(kReticle);
  SetDrawPhase(kPhaseForeground);
}

Reticle::~Reticle() = default;

UiElement* Reticle::TargetElement() const {
  return scene_->GetUiElementById(model_->reticle.target_element_id);
}

void Reticle::Render(UiElementRenderer* renderer,
                     const CameraModel& model) const {
  if (model_->reticle.cursor_type != kCursorDefault)
    return;
  renderer->DrawReticle(computed_opacity(),
                        model.view_proj_matrix * world_space_transform());
}

Reticle::Renderer::Renderer()
    : BaseQuadRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  color_handle_ = glGetUniformLocation(program_handle_, "color");
  ring_diameter_handle_ =
      glGetUniformLocation(program_handle_, "ring_diameter");
  inner_hole_handle_ = glGetUniformLocation(program_handle_, "inner_hole");
  inner_ring_end_handle_ =
      glGetUniformLocation(program_handle_, "inner_ring_end");
  inner_ring_thickness_handle_ =
      glGetUniformLocation(program_handle_, "inner_ring_thickness");
  mid_ring_end_handle_ = glGetUniformLocation(program_handle_, "mid_ring_end");
  mid_ring_opacity_handle_ =
      glGetUniformLocation(program_handle_, "mid_ring_opacity");
  opacity_handle_ = glGetUniformLocation(program_handle_, "opacity");
}

Reticle::Renderer::~Renderer() = default;

void Reticle::Renderer::Draw(float opacity,
                             const gfx::Transform& view_proj_matrix) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  glUniform4f(color_handle_, kReticleColor[0], kReticleColor[1],
              kReticleColor[2], kReticleColor[3]);
  glUniform1f(ring_diameter_handle_, kRingDiameter);
  glUniform1f(inner_hole_handle_, kInnerHole);
  glUniform1f(inner_ring_end_handle_, kInnerRingEnd);
  glUniform1f(inner_ring_thickness_handle_, kInnerRingThickness);
  glUniform1f(mid_ring_end_handle_, kMidRingEnd);
  glUniform1f(mid_ring_opacity_handle_, kMidRingOpacity);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

const char* Reticle::Renderer::VertexShader() {
  return kVertexShader;
}

gfx::Transform Reticle::LocalTransform() const {
  // Scale the reticle to have a fixed FOV size at any distance.
  const float eye_to_target =
      std::sqrt(model_->reticle.target_point.SquaredDistanceTo(kOrigin));

  if (eye_to_target == 0.0f)
    return gfx::Transform();

  gfx::Transform mat;
  mat.Scale3d(kReticleWidth * eye_to_target, kReticleHeight * eye_to_target, 1);

  // This will make the reticle planar to the target element (if there is one),
  // and directly face the user, otherwise.
  UiElement* target = TargetElement();
  gfx::Transform rotation_mat(gfx::Quaternion(
      gfx::Vector3dF(0.0f, 0.0f, -1.0f),
      target ? -target->GetNormal() : model_->reticle.target_point - kOrigin));
  mat = rotation_mat * mat;

  // We now need to correct the roll so that cursors point upward.
  gfx::Vector3dF up = {0.0f, 1.0f, 0.0f};
  gfx::Vector3dF forward = {0.0f, 0.0f, -1.0f};
  mat.TransformVector(&forward);

  gfx::Vector3dF right = {1.0f, 0.0f, 0.0f};
  mat.TransformVector(&right);

  gfx::Vector3dF expected_right = gfx::CrossProduct(forward, up);
  gfx::Quaternion rotate_to_expected_right(right, expected_right);
  mat.ConcatTransform(gfx::Transform(rotate_to_expected_right));

  mat.matrix().postTranslate(model_->reticle.target_point.x(),
                             model_->reticle.target_point.y(),
                             model_->reticle.target_point.z());
  return mat;
}

gfx::Transform Reticle::GetTargetLocalTransform() const {
  return LocalTransform();
}

bool Reticle::ShouldUpdateWorldSpaceTransform(
    bool parent_transform_changed) const {
  return true;
}

}  // namespace vr
