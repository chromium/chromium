// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/corner_radii.h"
#include "chrome/browser/vr/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {
class RectF;
class SizeF;
class Transform;
}  // namespace gfx

namespace vr {

class BaseRenderer;
class GridRenderer;
class RadialGradientQuadRenderer;
class TexturedQuadRenderer;

// An instance of this class is passed to UiElements by the UiRenderer in order
// to issue the GL commands for drawing the frame. In some ways, this class is a
// bit of unnecessary abstraction, but by having all the renderers owned by a
// single class, we gain several benefits. For one, we know when the shader
// programs are being changed and this lets us do batching (see the textured
// quad renderer). It also is a central point of contact that can let all
// renderers know to recreate their state in the event of a GL context
// loss/recreation.
class UiElementRenderer {
 public:
  UiElementRenderer();

  UiElementRenderer(const UiElementRenderer&) = delete;
  UiElementRenderer& operator=(const UiElementRenderer&) = delete;

  VIRTUAL_FOR_MOCKS ~UiElementRenderer();

  VIRTUAL_FOR_MOCKS void DrawTexturedQuad(
      int texture_data_handle,
      int overlay_texture_data_handle,
      const gfx::Transform& model_view_proj_matrix,
      const gfx::RectF& clip_rect,
      float opacity,
      const gfx::SizeF& element_size,
      float corner_radius,
      bool blend);
  VIRTUAL_FOR_MOCKS void DrawRadialGradientQuad(
      const gfx::Transform& model_view_proj_matrix,
      const SkColor edge_color,
      const SkColor center_color,
      const gfx::RectF& clip_rect,
      float opacity,
      const gfx::SizeF& element_size,
      const CornerRadii& radii);
  VIRTUAL_FOR_MOCKS void DrawGradientGridQuad(
      const gfx::Transform& model_view_proj_matrix,
      const SkColor grid_color,
      int gridline_count,
      float opacity);

  void Flush();

 protected:
  explicit UiElementRenderer(bool use_gl);

 private:
  void Init();
  void FlushIfNecessary(BaseRenderer* renderer);

  raw_ptr<BaseRenderer> last_renderer_ = nullptr;

  std::unique_ptr<TexturedQuadRenderer> textured_quad_renderer_;
  std::unique_ptr<RadialGradientQuadRenderer> radial_gradient_quad_renderer_;
  std::unique_ptr<GridRenderer> gradient_grid_renderer_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
