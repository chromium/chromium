// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_

#include <memory>

#include "base/optional.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {

// SharedQuadState holds a set of properties that are common across multiple
// DrawQuads. It's purely an optimization - the properties behave in exactly the
// same way as if they were replicated on each DrawQuad. A given SharedQuadState
// can only be shared by DrawQuads that are adjacent in their RenderPass'
// QuadList.
class VIZ_COMMON_EXPORT SharedQuadState {
 public:
  SharedQuadState();
  SharedQuadState(const SharedQuadState& other);
  ~SharedQuadState();

  void SetAll(const gfx::Transform& quad_to_target_transform,
              const gfx::Rect& layer_rect,
              const gfx::Rect& visible_layer_rect,
              const gfx::RRectF& rounded_corner_bounds,
              const gfx::Rect& clip_rect,
              bool is_clipped,
              bool are_contents_opaque,
              float opacity,
              SkBlendMode blend_mode,
              int sorting_context_id);
  void AsValueInto(base::trace_event::TracedValue* dict) const;

  // Transforms quad rects into the target content space.
  gfx::Transform quad_to_target_transform;
  // The rect of the quads' originating layer in the space of the quad rects.
  // Note that the |quad_layer_rect| represents the union of the |rect| of
  // DrawQuads in this SharedQuadState. If it does not hold, then
  // |are_contents_opaque| needs to be set to false.
  gfx::Rect quad_layer_rect;
  // The size of the visible area in the quads' originating layer, in the space
  // of the quad rects.
  gfx::Rect visible_quad_layer_rect;
  // This rect lives in the target content space. It defines the corner radius
  // to clip the quads with.
  gfx::RRectF rounded_corner_bounds;
  // This rect lives in the target content space.
  gfx::Rect clip_rect;
  bool is_clipped = false;
  // Indicates whether the content in |quad_layer_rect| are fully opaque.
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  // Used by SurfaceAggregator to decide whether to merge quads for a surface
  // into their target render pass. It is a performance optimization by avoiding
  // render passes as much as possible.
  bool is_fast_rounded_corner = false;
  // This is for underlay optimization and used only in the SurfaceAggregator
  // and the OverlayProcessor. This damage rect contains union of damage from
  // occluding surfaces and is only for quads that are the only quad in
  // their surface. SetAll() doesn't update this data.
  base::Optional<gfx::Rect> occluding_damage_rect;

  // The amount to skew quads in this layer. For experimental de-jelly effect.
  float de_jelly_delta_y = 0.0f;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
