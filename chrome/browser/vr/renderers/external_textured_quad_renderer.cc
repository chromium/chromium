// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/external_textured_quad_renderer.h"

#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kFragmentShader = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  uniform samplerExternalOES u_OverlayTexture;
  varying vec2 v_TexCoordinate;
  varying vec2 v_CornerPosition;
  uniform mediump float u_Opacity;
  uniform mediump float u_OverlayOpacity;
  uniform bool u_UsesOverlay;

  void main() {
    lowp vec4 color = texture2D(u_Texture, v_TexCoordinate);
    if (u_UsesOverlay) {
      lowp vec4 overlay_color = texture2D(u_OverlayTexture, v_TexCoordinate);
      overlay_color = overlay_color * u_OverlayOpacity;
      color = mix(color, overlay_color, overlay_color.a);
    }
    float mask = 1.0 - step(1.0, length(v_CornerPosition));
    gl_FragColor = color * u_Opacity * mask;
  }
);
// clang-format on

}  // namespace

ExternalTexturedQuadRenderer::ExternalTexturedQuadRenderer()
    : TexturedQuadRenderer(TexturedQuadRenderer::VertexShader(),
                           kFragmentShader) {}

ExternalTexturedQuadRenderer::~ExternalTexturedQuadRenderer() = default;

GLenum ExternalTexturedQuadRenderer::TextureType() const {
  return GL_TEXTURE_EXTERNAL_OES;
}

}  // namespace vr
