// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/texture_copy_renderer.h"

#include "device/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kWebVrVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_UvTransform;
  attribute vec4 a_Position;
  varying vec2 v_TexCoordinate;
  uniform float u_XBorder;
  uniform float u_YBorder;

  void main() {
    // The quad vertex coordinate range is [-0.5, 0.5]. Transform to [0, 1],
    // scale to cause the borders to wrap the texture, then apply the supplied
    // affine transform matrix to get the final UV.
    float xposition = a_Position[0] + 0.5;
    xposition = xposition * (2.0 * u_XBorder + 1.0) - u_XBorder;
    float yposition = a_Position[1] + 0.5;
    yposition = yposition * (2.0 * u_YBorder + 1.0) - u_YBorder;
    vec4 uv_in = vec4(xposition, yposition, 0.0, 1.0);
    vec4 uv_out = u_UvTransform * uv_in;
    v_TexCoordinate = vec2(uv_out.x, uv_out.y);
    gl_Position = vec4(a_Position.xyz * 2.0, 1.0);
  }
);

static constexpr char const* kWebVrFragmentShader = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  varying vec2 v_TexCoordinate;

  void main() {
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
  }
);
// clang-format on

}  // namespace

TextureCopyRenderer::TextureCopyRenderer()
    : BaseQuadRenderer(kWebVrVertexShader, kWebVrFragmentShader) {
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  uv_transform_ = glGetUniformLocation(program_handle_, "u_UvTransform");
  x_border_handle_ = glGetUniformLocation(program_handle_, "u_XBorder");
  y_border_handle_ = glGetUniformLocation(program_handle_, "u_YBorder");
}

// Draw the stereo WebVR frame
void TextureCopyRenderer::Draw(int texture_handle,
                               const float (&uv_transform)[16],
                               float xborder,
                               float yborder) {
  glUseProgram(program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, 2, GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  // Bind texture. This is a 1:1 pixel copy since the source surface
  // and renderbuffer destination size are resized to match, so use
  // GL_NEAREST.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_handle);
  SetTexParameters(GL_TEXTURE_EXTERNAL_OES);
  glUniform1i(texture_handle_, 0);

  glUniform1f(x_border_handle_, xborder);
  glUniform1f(y_border_handle_, yborder);

  glUniformMatrix4fv(uv_transform_, 1, GL_FALSE, &uv_transform[0]);

  // Blit texture to buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

// Note that we don't explicitly delete gl objects here, they're deleted
// automatically when we call ShutdownGL, and deleting them here leads to
// segfaults.
TextureCopyRenderer::~TextureCopyRenderer() = default;

}  // namespace vr
