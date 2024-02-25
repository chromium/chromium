// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_GRID_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_GRID_RENDERER_H_

#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class GridRenderer : public BaseQuadRenderer {
 public:
  GridRenderer();

  GridRenderer(const GridRenderer&) = delete;
  GridRenderer& operator=(const GridRenderer&) = delete;

  ~GridRenderer() override;

  void Draw(const gfx::Transform& model_view_proj_matrix,
            SkColor grid_color,
            int gridline_count,
            float opacity);

  static void CreateBuffers();

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint grid_color_handle_;
  GLuint opacity_handle_;
  GLuint lines_count_handle_;
};
}  // namespace vr
#endif  // CHROME_BROWSER_VR_RENDERERS_GRID_RENDERER_H_
