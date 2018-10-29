// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/vr/elements/controller.h"
#include "chrome/browser/vr/elements/environment/background.h"
#include "chrome/browser/vr/elements/environment/grid.h"
#include "chrome/browser/vr/elements/environment/stars.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/laser.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/shadow.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace gfx {
class RectF;
class SizeF;
class Transform;
}  // namespace gfx

namespace vr {

class BaseRenderer;
class ExternalTexturedQuadRenderer;
class RadialGradientQuadRenderer;
class TextureCopyRenderer;
class TexturedQuadRenderer;
class TransparentQuadRenderer;

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
  VIRTUAL_FOR_MOCKS ~UiElementRenderer();

  VIRTUAL_FOR_MOCKS void DrawTexturedQuad(
      int texture_data_handle,
      int overlay_texture_data_handle,
      GlTextureLocation texture_location,
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

  VIRTUAL_FOR_MOCKS void DrawController(
      float opacity,
      const gfx::Transform& model_view_proj_matrix);

  VIRTUAL_FOR_MOCKS void DrawLaser(
      float opacity,
      const gfx::Transform& model_view_proj_matrix);

  VIRTUAL_FOR_MOCKS void DrawReticle(
      float opacity,
      const gfx::Transform& model_view_proj_matrix);

  VIRTUAL_FOR_MOCKS void DrawTextureCopy(int texture_data_handle,
                                         const float (&uv_transform)[16],
                                         float xborder,
                                         float yborder);

  VIRTUAL_FOR_MOCKS void DrawShadow(
      const gfx::Transform& model_view_proj_matrix,
      const gfx::SizeF& element_size,
      float x_padding,
      float y_padding,
      float y_offset,
      SkColor color,
      float opacity,
      float corner_radius);

  VIRTUAL_FOR_MOCKS void DrawStars(
      float t,
      const gfx::Transform& model_view_proj_matrix);

  VIRTUAL_FOR_MOCKS void DrawBackground(
      const gfx::Transform& model_view_proj_matrix,
      int texture_data_handle,
      int normal_gradient_texture_data_handle,
      int incognito_gradient_texture_data_handle,
      int fullscreen_gradient_texture_data_handle,
      float normal_factor,
      float incognito_factor,
      float fullscreen_factor);

  VIRTUAL_FOR_MOCKS void DrawKeyboard(const CameraModel& camera_model,
                                      KeyboardDelegate* delegate);

  void Flush();

 protected:
  explicit UiElementRenderer(bool use_gl);

 private:
  void Init();
  void FlushIfNecessary(BaseRenderer* renderer);

  BaseRenderer* last_renderer_ = nullptr;

  std::unique_ptr<ExternalTexturedQuadRenderer>
      external_textured_quad_renderer_;
  std::unique_ptr<TransparentQuadRenderer> transparent_quad_renderer_;
  std::unique_ptr<TexturedQuadRenderer> textured_quad_renderer_;
  std::unique_ptr<RadialGradientQuadRenderer> radial_gradient_quad_renderer_;
  std::unique_ptr<TextureCopyRenderer> texture_copy_renderer_;
  std::unique_ptr<Reticle::Renderer> reticle_renderer_;
  std::unique_ptr<Laser::Renderer> laser_renderer_;
  std::unique_ptr<Controller::Renderer> controller_renderer_;
  std::unique_ptr<Grid::Renderer> gradient_grid_renderer_;
  std::unique_ptr<Shadow::Renderer> shadow_renderer_;
  std::unique_ptr<Stars::Renderer> stars_renderer_;
  std::unique_ptr<Background::Renderer> background_renderer_;
  std::unique_ptr<Keyboard::Renderer> keyboard_renderer_;

  DISALLOW_COPY_AND_ASSIGN(UiElementRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
