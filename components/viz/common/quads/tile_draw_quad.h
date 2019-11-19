// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_TILE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_TILE_DRAW_QUAD_H_

#include <stddef.h>

#include "components/viz/common/quads/content_draw_quad_base.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

class VIZ_COMMON_EXPORT TileDrawQuad : public ContentDrawQuadBase {
 public:
  static const size_t kResourceIdIndex = 0;

  TileDrawQuad();
  ~TileDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              // |tex_coord_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool is_premultiplied,
              bool nearest_neighbor,
              bool force_anti_aliasing_off);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              // |tex_coord_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool is_premultiplied,
              bool nearest_neighbor,
              bool force_anti_aliasing_off);

  static const TileDrawQuad* MaterialCast(const DrawQuad*);

  ResourceId resource_id() const { return resources.ids[kResourceIdIndex]; }

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_TILE_DRAW_QUAD_H_
