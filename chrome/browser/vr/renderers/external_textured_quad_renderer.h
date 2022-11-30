// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_

#include "chrome/browser/vr/renderers/textured_quad_renderer.h"

namespace vr {

class ExternalTexturedQuadRenderer : public TexturedQuadRenderer {
 public:
  ExternalTexturedQuadRenderer();

  ExternalTexturedQuadRenderer(const ExternalTexturedQuadRenderer&) = delete;
  ExternalTexturedQuadRenderer& operator=(const ExternalTexturedQuadRenderer&) =
      delete;

  ~ExternalTexturedQuadRenderer() override;

 private:
  GLenum TextureType() const override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_
