// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_
#define CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/gl_bindings.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkBitmap;
class SkSurface;

namespace vr {

class Background : public UiElement {
 public:
  Background();
  ~Background() override;

  // UiElement:
  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;
  void Initialize(SkiaSurfaceProvider* provider) override;

  void SetBackgroundImage(std::unique_ptr<SkBitmap> background);
  void SetGradientImages(std::unique_ptr<SkBitmap> normal_gradient,
                         std::unique_ptr<SkBitmap> incognito_gradient,
                         std::unique_ptr<SkBitmap> fullscreen_gradient);

  // These factors control the relative contribution of the the mode gradients
  // to the final background color.
  void SetNormalFactor(float factor);
  void SetIncognitoFactor(float factor);
  void SetFullscreenFactor(float factor);

  class Renderer : public BaseRenderer {
   public:
    Renderer();
    ~Renderer() final;

    void Draw(const gfx::Transform& view_proj_matrix,
              int texture_data_handle,
              int normal_gradient_data_handle,
              int incognito_gradient_data_handle,
              int fullscreen_gradient_data_handle,
              float normal_factor,
              float incognito_factor,
              float fullscreen_factork);

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint tex_uniform_handle_;
    GLuint normal_gradient_tex_uniform_handle_;
    GLuint incognito_gradient_tex_uniform_handle_;
    GLuint fullscreen_gradient_tex_uniform_handle_;
    GLuint normal_factor_handle_;
    GLuint incognito_factor_handle_;
    GLuint fullscreen_factor_handle_;

    GLuint vertex_buffer_;
    GLuint index_buffer_;
    GLuint index_count_;

    DISALLOW_COPY_AND_ASSIGN(Renderer);
  };

 private:
  void CreateBackgroundTexture();
  void CreateGradientTextures();

  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::KeyframeModel* keyframe_model) override;

  std::unique_ptr<SkBitmap> initialization_bitmap_;
  std::unique_ptr<SkBitmap> initialization_normal_gradient_bitmap_;
  std::unique_ptr<SkBitmap> initialization_incognito_gradient_bitmap_;
  std::unique_ptr<SkBitmap> initialization_fullscreen_gradient_bitmap_;
  GLuint texture_handle_ = 0;
  GLuint normal_gradient_texture_handle_ = 0;
  GLuint incognito_gradient_texture_handle_ = 0;
  GLuint fullscreen_gradient_texture_handle_ = 0;
  sk_sp<SkSurface> surface_;
  sk_sp<SkSurface> normal_gradient_surface_;
  sk_sp<SkSurface> incognito_gradient_surface_;
  sk_sp<SkSurface> fullscreen_gradient_surface_;
  SkiaSurfaceProvider* provider_ = nullptr;

  float normal_factor_ = 1.0f;
  float incognito_factor_ = 0.0f;
  float fullscreen_factor_ = 0.0f;

  DISALLOW_COPY_AND_ASSIGN(Background);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_
