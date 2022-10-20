// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_LASER_H_
#define CHROME_BROWSER_VR_ELEMENTS_LASER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "ui/gfx/geometry/point3_f.h"

namespace vr {

struct Model;

class Laser : public UiElement {
 public:
  explicit Laser(Model* model);

  Laser(const Laser&) = delete;
  Laser& operator=(const Laser&) = delete;

  ~Laser() override;

  class Renderer : public BaseQuadRenderer {
   public:
    Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    ~Renderer() override;

    void Draw(float opacity, const gfx::Transform& view_proj_matrix);

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint texture_unit_handle_;
    GLuint texture_data_handle_;
    GLuint color_handle_;
    GLuint fade_point_handle_;
    GLuint fade_end_handle_;
    GLuint opacity_handle_;
  };

 private:
  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  // Since the laser needs to render in response to model changes that occur
  // after the scene update (i.e., after input), we cannot rely on the usual
  // data binding flow since that would result in a frame of latency. Opacity
  // changes, however, are not latency sensitive and are bound in the usual way
  // (they also do not update due to input).
  raw_ptr<Model, DanglingUntriaged> model_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_LASER_H_
