// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_

#include <memory>
#include <optional>

#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace base::trace_event {
class TracedValue;
}  // namespace base::trace_event

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
  SharedQuadState& operator=(const SharedQuadState& other);
  ~SharedQuadState();

  // No comparison for |overlay_damage_index| and |is_fast_rounded_corner|.
  bool Equals(const SharedQuadState& other) const;

  void SetAll(const SharedQuadState& other);

  void SetAll(const gfx::Transform& transform,
              const gfx::Rect& layer_rect,
              const gfx::Rect& visible_layer_rect,
              const gfx::MaskFilterInfo& filter_info,
              const std::optional<gfx::Rect>& clip,
              bool contents_opaque,
              float opacity_f,
              SkBlendMode blend,
              int sorting_context,
              uint32_t layer_id,
              bool fast_rounded_corner);
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
  // This mask filter's coordinates is in the target content space. It defines
  // the corner radius to clip the quads with, and the gradient mask applied to
  // the clip rect given by the Rect part of |roudned_corner_bounds|.
  gfx::MaskFilterInfo mask_filter_info;
  // This rect lives in the target content space.
  std::optional<gfx::Rect> clip_rect;
  // Indicates whether the content in |quad_layer_rect| are fully opaque.
  bool are_contents_opaque = true;
  float opacity = 1.0f;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  // Optionally set by the client with a stable ID for the layer that produced
  // the DrawQuad(s). This is used to help identify that DrawQuad(s) in one
  // frame came from the same layer as DrawQuads() from a previous frame, even
  // if they changed position or other attributes.
  uint32_t layer_id = 0;
  // Used by SurfaceAggregator to namespace layer_ids from different clients.
  uint32_t layer_namespace_id = 0;
  // Used by SurfaceAggregator to decide whether to merge quads for a surface
  // into their target render pass. It is a performance optimization by avoiding
  // render passes as much as possible.
  bool is_fast_rounded_corner = false;
  // This is for underlay optimization and used only in the SurfaceAggregator
  // and the OverlayProcessor. Do not set the value in CompositorRenderPass.
  // This index points to the damage rect in the surface damage rect list where
  // the overlay quad belongs to. SetAll() doesn't update this data.
  // TODO(crbug.com/40072194): Consider moving this member out of this struct
  // and into the quads themselves.
  std::optional<size_t> overlay_damage_index;

  // If not zero then the quads can be offset by some provided value. Offset is
  // in target content space.
  OffsetTag offset_tag;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
