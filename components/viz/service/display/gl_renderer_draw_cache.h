// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_DRAW_CACHE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_DRAW_CACHE_H_

#include <vector>

#include "base/macros.h"
#include "components/viz/service/display/program_binding.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rrect_f.h"

namespace viz {

// Collects 4 floats at a time for easy upload to GL.
struct Float4 {
  float data[4];
};

// Collects 16 floats at a time for easy upload to GL.
struct Float16 {
  float data[16];
};

// A cache for storing textured quads to be drawn.  Stores the minimum required
// data to tell if two back to back draws only differ in their transform. Quads
// that only differ by transform may be coalesced into a single draw call.
struct TexturedQuadDrawCache {
  TexturedQuadDrawCache();
  ~TexturedQuadDrawCache();

  bool is_empty = true;

  // Values tracked to determine if textured quads may be coalesced.
  ProgramKey program_key;
  int resource_id = -1;
  bool needs_blending = false;
  bool nearest_neighbor = false;
  SkColor background_color = 0;
  gfx::RRectF rounded_corner_bounds;

  // A cache for the coalesced quad data.
  std::vector<Float4> uv_xform_data;
  std::vector<float> vertex_opacity_data;
  std::vector<Float16> matrix_data;

  // Don't batch if tex clamp rect is given.
  Float4 tex_clamp_rect_data;

 private:
  DISALLOW_COPY_AND_ASSIGN(TexturedQuadDrawCache);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_DRAW_CACHE_H_
