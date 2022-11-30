// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_

#include "chrome/browser/vr/renderers/textured_quad_renderer.h"

namespace vr {

class TransparentQuadRenderer : public TexturedQuadRenderer {
 public:
  TransparentQuadRenderer();

  TransparentQuadRenderer(const TransparentQuadRenderer&) = delete;
  TransparentQuadRenderer& operator=(const TransparentQuadRenderer&) = delete;

  ~TransparentQuadRenderer() override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_
