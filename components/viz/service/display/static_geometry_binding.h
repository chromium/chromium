// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_STATIC_GEOMETRY_BINDING_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_STATIC_GEOMETRY_BINDING_H_

#include "base/macros.h"
#include "components/viz/service/display/geometry_binding.h"
#include "components/viz/service/viz_service_export.h"

using gpu::gles2::GLES2Interface;

namespace viz {

class VIZ_SERVICE_EXPORT StaticGeometryBinding {
 public:
  StaticGeometryBinding(gpu::gles2::GLES2Interface* gl,
                        const gfx::RectF& quad_vertex_rect);
  ~StaticGeometryBinding();

  void PrepareForDraw();

  enum {
    NUM_QUADS = 9,
  };

 private:
  gpu::gles2::GLES2Interface* gl_;

  GLuint quad_vertices_vbo_;
  GLuint quad_elements_vbo_;

  DISALLOW_COPY_AND_ASSIGN(StaticGeometryBinding);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_STATIC_GEOMETRY_BINDING_H_
