// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/radial_gradient_quad_renderer.h"

#include "chrome/browser/vr/elements/corner_radii.h"
#include "chrome/browser/vr/renderers/textured_quad_renderer.h"
#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

static constexpr int kPositionDataSize = 2;
static constexpr size_t kPositionDataOffset = 0;
static constexpr int kOffsetScaleDataSize = 2;
static constexpr size_t kOffsetScaleDataOffset = 2 * sizeof(float);
static constexpr int kCornerPositionDataSize = 2;
static constexpr size_t kCornerPositionDataOffset = 4 * sizeof(float);
static constexpr size_t kDataStride = 6 * sizeof(float);

static constexpr size_t kInnerRectOffset = 6 * sizeof(GLushort);

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform vec2 u_ULCornerOffset;
  uniform vec2 u_URCornerOffset;
  uniform vec2 u_LRCornerOffset;
  uniform vec2 u_LLCornerOffset;
  uniform mediump float u_AspectRatio;
  attribute vec4 a_Position;
  attribute vec2 a_CornerPosition;
  attribute vec2 a_OffsetScale;
  varying vec2 v_CornerPosition;
  varying vec2 v_TexCoordinate;
  varying vec2 v_Position;

  void main() {
    v_CornerPosition = a_CornerPosition;
    vec2 corner_offset;
    if (a_Position[0] < 0.0 && a_Position[1] > 0.0) {
      corner_offset = u_ULCornerOffset;
    } else if (a_Position[0] > 0.0 && a_Position[1] > 0.0) {
      corner_offset = u_URCornerOffset;
    } else if (a_Position[0] > 0.0 && a_Position[1] < 0.0) {
      corner_offset = u_LRCornerOffset;
    } else if (a_Position[0] < 0.0 && a_Position[1] < 0.0) {
      corner_offset = u_LLCornerOffset;
    }
    vec4 position = vec4(
        a_Position[0] + corner_offset[0] * a_OffsetScale[0],
        a_Position[1] + corner_offset[1] * a_OffsetScale[1],
        a_Position[2],
        a_Position[3]);
    if (u_AspectRatio > 1.0) {
      v_Position = vec2(position.x, position.y / u_AspectRatio);
    } else {
      v_Position = vec2(position.x * u_AspectRatio, position.y);
    }
    v_TexCoordinate = vec2(0.5 + position[0], 0.5 - position[1]);
    gl_Position = u_ModelViewProjMatrix * position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  varying vec2 v_CornerPosition;
  varying vec2 v_Position;
  varying vec2 v_TexCoordinate;
  uniform mediump float u_Opacity;
  uniform vec2 u_ClipRect[2];
  uniform vec4 u_CenterColor;
  uniform vec4 u_EdgeColor;

  void main() {
    vec2 s = step(u_ClipRect[0], v_TexCoordinate)
        - step(u_ClipRect[1], v_TexCoordinate);
    float insideClip = s.x * s.y;

    vec2 position = v_Position;
    float edge_color_weight = clamp(2.0 * length(position), 0.0, 1.0);
    float center_color_weight = 1.0 - edge_color_weight;
    vec4 color = u_CenterColor * center_color_weight + u_EdgeColor *
        edge_color_weight;
    float mask = 1.0 - step(1.0, length(v_CornerPosition));
    // Add some noise to prevent banding artifacts in the gradient.
    float n = (fract(dot(v_Position.xy, vec2(12345.67, 456.7))) - 0.5) / 255.0;

    color = color + n;
    color = vec4(color.rgb * color.a, color.a);
    gl_FragColor = insideClip * color * u_Opacity * mask;
  }
);
// clang-format on

void SetCornerOffset(GLuint handle, float radius, const gfx::SizeF& size) {
  if (radius == 0.0f)
    glUniform2f(handle, 0.0, 0.0);
  else
    glUniform2f(handle, radius / size.width(), radius / size.height());
}

}  // namespace

RadialGradientQuadRenderer::RadialGradientQuadRenderer()
    : BaseRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  ul_corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_ULCornerOffset");
  ur_corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_URCornerOffset");
  lr_corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_LRCornerOffset");
  ll_corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_LLCornerOffset");
  corner_position_handle_ =
      glGetAttribLocation(program_handle_, "a_CornerPosition");
  offset_scale_handle_ = glGetAttribLocation(program_handle_, "a_OffsetScale");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  center_color_handle_ = glGetUniformLocation(program_handle_, "u_CenterColor");
  edge_color_handle_ = glGetUniformLocation(program_handle_, "u_EdgeColor");
  aspect_ratio_handle_ = glGetUniformLocation(program_handle_, "u_AspectRatio");
}

RadialGradientQuadRenderer::~RadialGradientQuadRenderer() = default;

void RadialGradientQuadRenderer::Draw(
    const gfx::Transform& model_view_proj_matrix,
    SkColor edge_color,
    SkColor center_color,
    const gfx::RectF& clip_rect,
    float opacity,
    const gfx::SizeF& element_size,
    const CornerRadii& radii) {
  DCHECK(opacity > 0.f);
  if (SkColorGetA(edge_color) == SK_AlphaTRANSPARENT &&
      SkColorGetA(center_color) == SK_AlphaTRANSPARENT) {
    return;
  }
  if (!clip_rect.Intersects(gfx::RectF(1.0f, 1.0f)))
    return;

  glUseProgram(program_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, TexturedQuadRenderer::VertexBuffer());
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TexturedQuadRenderer::IndexBuffer());

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kPositionDataSize, GL_FLOAT, false,
                        kDataStride, VOID_OFFSET(kPositionDataOffset));
  glEnableVertexAttribArray(position_handle_);

  // Set up offset scale attribute.
  glVertexAttribPointer(offset_scale_handle_, kOffsetScaleDataSize, GL_FLOAT,
                        false, kDataStride,
                        VOID_OFFSET(kOffsetScaleDataOffset));
  glEnableVertexAttribArray(offset_scale_handle_);

  // Set up corner position attribute.
  glVertexAttribPointer(corner_position_handle_, kCornerPositionDataSize,
                        GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(kCornerPositionDataOffset));
  glEnableVertexAttribArray(corner_position_handle_);

  SetCornerOffset(ul_corner_offset_handle_, radii.upper_left, element_size);
  SetCornerOffset(ur_corner_offset_handle_, radii.upper_right, element_size);
  SetCornerOffset(lr_corner_offset_handle_, radii.lower_right, element_size);
  SetCornerOffset(ll_corner_offset_handle_, radii.lower_left, element_size);

  // Set the edge color to the fog color so that it seems to fade out.
  SetColorUniform(edge_color_handle_, edge_color);
  SetColorUniform(center_color_handle_, center_color);
  glUniform1f(opacity_handle_, opacity);
  glUniform1f(aspect_ratio_handle_,
              element_size.width() / element_size.height());

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(model_view_proj_matrix).data());

  const GLfloat clip_rect_data[4] = {clip_rect.x(), clip_rect.y(),
                                     clip_rect.right(), clip_rect.bottom()};
  glUniform2fv(clip_rect_handle_, 2, clip_rect_data);

  if (radii.IsZero()) {
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                   VOID_OFFSET(kInnerRectOffset));
  } else {
    glDrawElements(GL_TRIANGLES, TexturedQuadRenderer::NumQuadIndices(),
                   GL_UNSIGNED_SHORT, 0);
  }

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(offset_scale_handle_);
  glDisableVertexAttribArray(corner_position_handle_);
}

}  // namespace vr
