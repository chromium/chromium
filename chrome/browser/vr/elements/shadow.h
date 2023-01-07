// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SHADOW_H_
#define CHROME_BROWSER_VR_ELEMENTS_SHADOW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

// A shadow is meant to be the ancestor of elements to which a shadow is to be
// applied. The shadow is applied across its padding.
// By default the direct child is used as the shadow caster. This behavior can
// be changed by manually setting a shadow caster.
class VR_UI_EXPORT Shadow : public UiElement {
 public:
  Shadow();

  Shadow(const Shadow&) = delete;
  Shadow& operator=(const Shadow&) = delete;

  ~Shadow() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& camera_model) const override;

  void LayOutContributingChildren() override;
  void set_intensity(float intensity) { intensity_ = intensity; }
  void set_shadow_caster(UiElement* shadow_caster) {
    shadow_caster_ = shadow_caster;
  }

  class Renderer : public BaseQuadRenderer {
   public:
    Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    ~Renderer() override;

    void Draw(const gfx::Transform& model_view_proj_matrix,
              const gfx::SizeF& element_size,
              float x_padding,
              float y_padding,
              float y_offset,
              SkColor color,
              float opacity,
              float corner_radius);

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint x_padding_handle_;
    GLuint y_padding_handle_;
    GLuint y_offset_handle_;
    GLuint color_handle_;
    GLuint opacity_handle_;
    GLuint x_corner_radius_handle_;
    GLuint y_corner_radius_handle_;
  };

 private:
  float depth_;
  float intensity_ = 1.0f;
  raw_ptr<UiElement> shadow_caster_ = nullptr;
  gfx::SizeF contributed_size_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SHADOW_H_
