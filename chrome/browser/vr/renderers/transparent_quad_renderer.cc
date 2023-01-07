// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/transparent_quad_renderer.h"

#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  uniform sampler2D u_Texture;
  uniform sampler2D u_OverlayTexture;
  uniform vec4 u_CopyRect;
  varying vec2 v_TexCoordinate;
  varying vec2 v_CornerPosition;
  uniform mediump float u_Opacity;
  uniform mediump float u_OverlayOpacity;
  void main() {
    if (length(v_CornerPosition) > 1.0)
        discard;
    gl_FragColor = vec4(0, 0, 0, 0);
  }
);
// clang-format on

}  // namespace

TransparentQuadRenderer::TransparentQuadRenderer()
    : TexturedQuadRenderer(TexturedQuadRenderer::VertexShader(),
                           kFragmentShader) {}

TransparentQuadRenderer::~TransparentQuadRenderer() = default;

}  // namespace vr
