// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_PICTURE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_PICTURE_DRAW_QUAD_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/scroll_offset_map.h"
#include "components/viz/common/quads/content_draw_quad_base.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// Used for on-demand tile rasterization.
class VIZ_COMMON_EXPORT PictureDrawQuad : public ContentDrawQuadBase {
 public:
  static constexpr Material kMaterial = Material::kPictureContent;

  PictureDrawQuad();
  PictureDrawQuad(const PictureDrawQuad& other);
  ~PictureDrawQuad() override;

  using ImageAnimationMap = base::flat_map<cc::PaintImage::Id, size_t>;
  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool nearest_neighbor,
              const gfx::Rect& content,
              float scale,
              ImageAnimationMap animation_map,
              scoped_refptr<const cc::DisplayItemList> display_items,
              cc::ScrollOffsetMap raster_inducing_scroll_offsets);

  gfx::Rect content_rect;
  float contents_scale;
  ImageAnimationMap image_animation_map;
  scoped_refptr<const cc::DisplayItemList> display_item_list;
  cc::ScrollOffsetMap raster_inducing_scroll_offsets;

  static const PictureDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_PICTURE_DRAW_QUAD_H_
