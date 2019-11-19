// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_CONTENT_DRAW_QUAD_BASE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_CONTENT_DRAW_QUAD_BASE_H_

#include <memory>

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace viz {

class VIZ_COMMON_EXPORT ContentDrawQuadBase : public DrawQuad {
 public:
  void SetNew(const SharedQuadState* shared_quad_state,
              DrawQuad::Material material,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool is_premultiplied,
              bool nearest_neighbor,
              bool force_anti_aliasing_off);

  void SetAll(const SharedQuadState* shared_quad_state,
              DrawQuad::Material material,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool is_premultiplied,
              bool nearest_neighbor,
              bool force_anti_aliasing_off);

  gfx::RectF tex_coord_rect;
  gfx::Size texture_size;
  bool is_premultiplied = false;
  bool nearest_neighbor = false;
  bool force_anti_aliasing_off = false;

 protected:
  ContentDrawQuadBase();
  ~ContentDrawQuadBase() override;
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_CONTENT_DRAW_QUAD_BASE_H_
