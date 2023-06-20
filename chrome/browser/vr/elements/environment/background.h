// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_
#define CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "chrome/browser/vr/skia_surface_provider.h"
#include "device/vr/gl_bindings.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkBitmap;

namespace vr {

class Background : public UiElement {
 public:
  Background();

  Background(const Background&) = delete;
  Background& operator=(const Background&) = delete;

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

  class Renderer final : public BaseRenderer {
   public:
    Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    ~Renderer() override;

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
  };

 private:
  void CreateBackgroundTexture();
  void CreateGradientTextures();

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  std::unique_ptr<SkBitmap> initialization_bitmap_;
  std::unique_ptr<SkBitmap> initialization_normal_gradient_bitmap_;
  std::unique_ptr<SkBitmap> initialization_incognito_gradient_bitmap_;
  std::unique_ptr<SkBitmap> initialization_fullscreen_gradient_bitmap_;
  raw_ptr<SkiaSurfaceProvider, DanglingUntriaged> provider_ = nullptr;

  std::unique_ptr<SkiaSurfaceProvider::Texture> texture_;
  std::unique_ptr<SkiaSurfaceProvider::Texture> normal_gradient_texture_;
  std::unique_ptr<SkiaSurfaceProvider::Texture> incognito_gradient_texture_;
  std::unique_ptr<SkiaSurfaceProvider::Texture> fullscreen_gradient_texture_;

  float normal_factor_ = 1.0f;
  float incognito_factor_ = 0.0f;
  float fullscreen_factor_ = 0.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_BACKGROUND_H_
