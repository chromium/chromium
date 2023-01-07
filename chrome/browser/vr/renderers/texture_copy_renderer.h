// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_TEXTURE_COPY_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_TEXTURE_COPY_RENDERER_H_

#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Renders a page-generated stereo VR view.
class VR_UI_EXPORT TextureCopyRenderer : public BaseQuadRenderer {
 public:
  TextureCopyRenderer();

  TextureCopyRenderer(const TextureCopyRenderer&) = delete;
  TextureCopyRenderer& operator=(const TextureCopyRenderer&) = delete;

  ~TextureCopyRenderer() override;

  void Draw(int texture_handle,
            const float (&uv_transform)[16],
            float xborder,
            float yborder);

 private:
  GLuint texture_handle_;
  GLuint uv_transform_;
  GLuint x_border_handle_;
  GLuint y_border_handle_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_TEXTURE_COPY_RENDERER_H_
