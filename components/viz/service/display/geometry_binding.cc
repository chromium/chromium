// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/geometry_binding.h"

#include <stdint.h>
#include <string.h>

#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

void SetupGLContext(gpu::gles2::GLES2Interface* gl,
                    GLuint quad_elements_vbo,
                    GLuint quad_vertices_vbo) {
  gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo);

  gl->BindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo);
  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[3] = {
      nullptr, reinterpret_cast<const void*>(3 * sizeof(float)),
      reinterpret_cast<const void*>(5 * sizeof(float)),
  };

  gl->VertexAttribPointer(GeometryBinding::PositionAttribLocation(), 3,
                          GL_FLOAT, false, 6 * sizeof(float), offsets[0]);
  gl->VertexAttribPointer(GeometryBinding::TexCoordAttribLocation(), 2,
                          GL_FLOAT, false, 6 * sizeof(float), offsets[1]);
  gl->VertexAttribPointer(GeometryBinding::TriangleIndexAttribLocation(), 1,
                          GL_FLOAT, false, 6 * sizeof(float), offsets[2]);
  gl->EnableVertexAttribArray(GeometryBinding::PositionAttribLocation());
  gl->EnableVertexAttribArray(GeometryBinding::TexCoordAttribLocation());
  gl->EnableVertexAttribArray(GeometryBinding::TriangleIndexAttribLocation());
}

GeometryBindingQuad::GeometryBindingQuad() {
  v0 = {{0, 0, 0}, {0, 0}, 0};
  v1 = {{0, 0, 0}, {0, 0}, 0};
  v2 = {{0, 0, 0}, {0, 0}, 0};
  v3 = {{0, 0, 0}, {0, 0}, 0};
}

GeometryBindingQuad::GeometryBindingQuad(const GeometryBindingVertex& vert0,
                                         const GeometryBindingVertex& vert1,
                                         const GeometryBindingVertex& vert2,
                                         const GeometryBindingVertex& vert3) {
  v0 = vert0;
  v1 = vert1;
  v2 = vert2;
  v3 = vert3;
}

GeometryBindingQuadIndex::GeometryBindingQuadIndex() {
  memset(data, 0x0, sizeof(data));
}

GeometryBindingQuadIndex::GeometryBindingQuadIndex(uint16_t index0,
                                                   uint16_t index1,
                                                   uint16_t index2,
                                                   uint16_t index3,
                                                   uint16_t index4,
                                                   uint16_t index5) {
  data[0] = index0;
  data[1] = index1;
  data[2] = index2;
  data[3] = index3;
  data[4] = index4;
  data[5] = index5;
}

}  // namespace viz
