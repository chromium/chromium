// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_STARS_H_
#define CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_STARS_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_renderer.h"

namespace vr {

class Stars : public UiElement {
 public:
  Stars();
  ~Stars() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& camera) const override;

  class Renderer : public BaseRenderer {
   public:
    Renderer();
    ~Renderer() override;
    void Draw(float t, const gfx::Transform& view_proj_matrix);

    static void CreateBuffers();

   private:
    static GLuint vertex_buffer_;
    static GLuint index_buffer_;

    // Uniforms
    GLuint model_view_proj_matrix_handle_ = 0;
    GLuint time_handle_ = 0;

    // Attributes
    GLuint opacity_handle_;
    GLuint phase_handle_;

    DISALLOW_COPY_AND_ASSIGN(Renderer);
  };

 private:
  void Initialize(SkiaSurfaceProvider* provider) override;

  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stars);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_STARS_H_
