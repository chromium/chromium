// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/shadow.h"

#include "base/numerics/ranges.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/animation/tween.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec4 a_Position;
  varying vec2 v_Position;

  void main() {
    v_Position = vec2(0.5 + a_Position.x, 0.5 - a_Position.y);
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  uniform mediump float u_XPadding;
  uniform mediump float u_YPadding;
  uniform vec4 u_Color;
  uniform mediump float u_Opacity;
  uniform mediump float u_YOffset;
  uniform mediump float u_XCornerRadius;
  uniform mediump float u_YCornerRadius;
  varying vec2 v_Position;

  void main() {
    // By using abs here, we can treat all four quadrants of the quad in the
    // same way.
    vec2 pos = vec2(0.5 - abs(v_Position.x - 0.5),
                    0.5 - abs(v_Position.y - 0.5 - u_YOffset));
    float x_ramp = u_XPadding + u_XCornerRadius;
    float y_ramp = u_YPadding + u_YCornerRadius;

    // Compute distance from inner rect.
    float x_pad_distance = (pos.x - x_ramp) / x_ramp;
    float y_pad_distance = (pos.y - y_ramp) / y_ramp;

    // These booleans drive some subsequent conditionals. Essentially, we need
    // to determine which of the following situations applies:
    //  1. We're in the corner (where the shadow is elliptical),
    //  2. In a linear gradient off one of the sides of the rounded rect.
    //  3. We're fully within the rounded rect.
    bool inside_x_pad = x_pad_distance < 0.0;
    bool inside_y_pad = y_pad_distance < 0.0;

    // Computing this mask (i.e., the shadow gradient) is the point of this fn.
    float mask = 1.0;

    float x_offset = u_XCornerRadius / (u_XPadding + u_XCornerRadius);
    float y_offset = u_YCornerRadius / (u_YPadding + u_YCornerRadius);
    float x_clamped = 1.0 + x_pad_distance;
    float y_clamped = 1.0 + y_pad_distance;

    if (inside_x_pad && inside_y_pad) {
      vec2 v = vec2(x_pad_distance, y_pad_distance);
      vec2 n = normalize(v);
      // Sadly, there is no M_PI_2 constant to use within glsl.
      float t = acos(-n.x) / 1.57079632679489661923;
      float offset = mix(x_offset, y_offset, t);
      mask = clamp((1.0 - clamp(length(v), 0.0, 1.0)) / (1.0 - offset),
                   0.0, 1.0);
    } else if (inside_x_pad) {
      mask = clamp(x_clamped / (1.0 - x_offset), 0.0, 1.0);
    } else if (inside_y_pad) {
      mask = clamp(y_clamped / (1.0 - y_offset), 0.0, 1.0);
    }

    // This is an arbitrary function that takes mask (which was previously a
    // linear ramp from the edge of the rrect), and causes it to fall off more
    // rapidly.
    mask = mask * mask * mask * mask;

    vec4 color = u_Color;
    color = vec4(color.xyz * color.w, color.w);

    // Add some noise to prevent banding artifacts in the gradient.
    float n =
        (fract(dot(v_Position.xy, vec2(12345.67, 456.7))) - 0.5) / 255.0;
    gl_FragColor = (color + n) * mask * u_Opacity;
  }
);
// clang-format on

// Beyond this depth, things start to look too blurry. This is a bit arbitrary.
static constexpr float kMaximumChildDepth = 0.15f;
static constexpr float kShadowOpacity = 0.4f;

// Adjusting these will change the min/max gradient sizes along the x/y axes. By
// adjusting these ranges independently, we can be blurrier in one direction
// than another, simulating different light shapes.
static constexpr float kXMinShadowGradientFactor = 0.01f;
static constexpr float kXMaxShadowGradientFactor = 0.3f;
static constexpr float kYMinShadowGradientFactor = 0.01f;
static constexpr float kYMaxShadowGradientFactor = 0.24f;

// This amount controls how much we increase the padding due to depth changes.
// I.e, this causes the shadow to encroach into the rounded rect which is
// crucial for softness. Increasing this number makes the shadows softer.
static constexpr float kPaddingScaleFactor = 0.2f;

// This value adjust how the shadow translates due to depth changes. Increasing
// this number effectively makes the faked light sounce appear higher (the
// shadow descends more quickly).
static constexpr float kYShadowOffset = 0.03f;

}  // namespace

Shadow::Shadow() {
  set_bounds_contain_children(true);
  set_bounds_contain_padding(false);
}

Shadow::~Shadow() {}

void Shadow::Render(UiElementRenderer* renderer,
                    const CameraModel& camera_model) const {
  DCHECK_EQ(left_padding(), right_padding());
  DCHECK_EQ(top_padding(), bottom_padding());
  renderer->DrawShadow(
      camera_model.view_proj_matrix * world_space_transform(), size(),
      left_padding(), right_padding(),
      gfx::Tween::FloatValueBetween(depth_, 0.0f, 1.0f), SK_ColorBLACK,
      computed_opacity() * kShadowOpacity * intensity_, corner_radius());
}

void Shadow::LayOutContributingChildren() {
  DCHECK(shadow_caster_ || !children().empty());
  UiElement* shadow_caster =
      shadow_caster_ ? shadow_caster_ : children().back().get();
  gfx::Point3F p;
  shadow_caster->LocalTransform().TransformPoint(&p);
  DCHECK_GE(kMaximumChildDepth, p.z());
  depth_ = base::ClampToRange(p.z() / kMaximumChildDepth, 0.0f, 1.0f);
  // This is an arbitrary function that quickly accelerates from 0 toward 1.
  set_padding(gfx::Tween::FloatValueBetween(depth_, kXMinShadowGradientFactor,
                                            kXMaxShadowGradientFactor),
              gfx::Tween::FloatValueBetween(depth_, kYMinShadowGradientFactor,
                                            kYMaxShadowGradientFactor));
  if (shadow_caster_ || children().size() == 1u)
    SetCornerRadius(shadow_caster->corner_radii().MaxRadius());
}

Shadow::Renderer::Renderer()
    : BaseQuadRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  x_padding_handle_ = glGetUniformLocation(program_handle_, "u_XPadding");
  y_padding_handle_ = glGetUniformLocation(program_handle_, "u_YPadding");
  y_offset_handle_ = glGetUniformLocation(program_handle_, "u_YOffset");
  color_handle_ = glGetUniformLocation(program_handle_, "u_Color");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  x_corner_radius_handle_ =
      glGetUniformLocation(program_handle_, "u_XCornerRadius");
  y_corner_radius_handle_ =
      glGetUniformLocation(program_handle_, "u_YCornerRadius");
}

Shadow::Renderer::~Renderer() = default;

void Shadow::Renderer::Draw(const gfx::Transform& model_view_proj_matrix,
                            const gfx::SizeF& element_size,
                            float x_padding,
                            float y_padding,
                            float y_offset,
                            SkColor color,
                            float opacity,
                            float corner_radius) {
  PrepareToDraw(model_view_proj_matrix_handle_, model_view_proj_matrix);

  float scale = 1.0 + (y_offset * kPaddingScaleFactor);
  glUniform1f(x_padding_handle_, x_padding * scale / element_size.width());
  glUniform1f(y_padding_handle_, y_padding * scale / element_size.height());
  glUniform1f(y_offset_handle_, y_offset * kYShadowOffset);
  glUniform1f(opacity_handle_, opacity);
  glUniform1f(x_corner_radius_handle_, corner_radius / element_size.width());
  glUniform1f(y_corner_radius_handle_, corner_radius / element_size.height());

  SetColorUniform(color_handle_, color);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
