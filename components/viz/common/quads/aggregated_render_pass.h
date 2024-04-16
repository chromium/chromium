// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_H_
#define COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/types/id_type.h"
#include "cc/base/list_container.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/render_pass_internal.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
class AggregatedRenderPass;
class CompositorRenderPassDrawQuad;
class AggregatedRenderPassDrawQuad;

using AggregatedRenderPassId = base::IdTypeU64<AggregatedRenderPass>;

// This class represents a render pass that is a result of aggregating render
// passes from all of the relevant surfaces. It is _not_ mojo-serializable since
// it is local to the viz process. It has a unique AggregatedRenderPassId across
// all of the AggregatedRenderPasses.
class VIZ_COMMON_EXPORT AggregatedRenderPass : public RenderPassInternal {
 public:
  AggregatedRenderPass(const AggregatedRenderPass&) = delete;
  AggregatedRenderPass& operator=(const AggregatedRenderPass&) = delete;

  ~AggregatedRenderPass();

  AggregatedRenderPass();
  AggregatedRenderPass(size_t shared_quad_state_size, size_t draw_quad_size);

  void SetNew(AggregatedRenderPassId pass_id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target);

  void SetAll(AggregatedRenderPassId pass_id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target,
              const cc::FilterOperations& filters,
              const cc::FilterOperations& backdrop_filters,
              const std::optional<gfx::RRectF>& backdrop_filter_bounds,
              gfx::ContentColorUsage color_usage,
              bool has_transparent_background,
              bool cache_render_pass,
              bool has_damage_from_contributing_content,
              bool generate_mipmap);

  AggregatedRenderPassDrawQuad* CopyFromAndAppendRenderPassDrawQuad(
      const CompositorRenderPassDrawQuad* quad,
      AggregatedRenderPassId render_pass_id);
  AggregatedRenderPassDrawQuad* CopyFromAndAppendRenderPassDrawQuad(
      const AggregatedRenderPassDrawQuad* quad);
  DrawQuad* CopyFromAndAppendDrawQuad(const DrawQuad* quad);

  // A shallow copy of the render pass, which does not include its quads or copy
  // requests.
  std::unique_ptr<AggregatedRenderPass> Copy(
      AggregatedRenderPassId new_id) const;

  // A deep copy of the render pass that includes quads.
  std::unique_ptr<AggregatedRenderPass> DeepCopy() const;

  template <typename DrawQuadType>
  DrawQuadType* CreateAndAppendDrawQuad() {
    static_assert(
        !std::is_same<DrawQuadType, CompositorRenderPassDrawQuad>::value,
        "cannot create CompositorRenderPassDrawQuad in AggregatedRenderPass");
    return quad_list.AllocateAndConstruct<DrawQuadType>();
  }

  // Indicates if any its quad needs to draw with blending.
  bool ShouldDrawWithBlending() const;

  // Indicates if this pass has copy requests or video capture enabled.
  bool HasCapture() const;

  // Uniquely identifies the render pass in the aggregated frame.
  AggregatedRenderPassId id;

  // The type of color content present in this RenderPass.
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;

  // Indicates current RenderPass is a color conversion pass.
  bool is_color_conversion_pass = false;

  // |true| if this render pass, prior to aggregation, was the root pass of a
  // surface's resolved frame.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  bool is_from_surface_root_pass = false;

#if BUILDFLAG(IS_WIN)
  // |true| if this render pass backing needs to be read by Viz to be composited
  // correctly. If |DirectRenderer| will compose this render pass, this must be
  // set to |true|.
  //
  // DComp surfaces on Windows do not allow reading, so this field is overridden
  // during overlay processing to help us detect when we can back a render pass
  // with a DComp surface to scanout directly.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  bool will_backing_be_read_by_viz = true;

  // Windows only: Indicates that the render pass backing's updates need to be
  // synchronized with tree updates. A swap chain does not synchronize its
  // presents with DComp commits. This is needed when e.g. the render pass has
  // video holes that need to line up with other overlays or is itself presented
  // as an overlay.
  bool needs_synchronous_dcomp_commit = false;
#endif

  // Indicates whether video capture has been enabled for this render pass.
  //
  // This is useful to avoid flipping back and forth between promoting quads to
  // overlays since a 30fps capture on a 60fps monitor can make a copy request
  // every other frame.
  bool video_capture_enabled = false;

  void AsValueInto(base::trace_event::TracedValue* dict) const;

 private:
  template <typename DrawQuadType>
  DrawQuadType* CopyFromAndAppendTypedDrawQuad(const DrawQuad* quad) {
    static_assert(
        !std::is_same<DrawQuadType, CompositorRenderPassDrawQuad>::value,
        "cannot copy CompositorRenderPassDrawQuad type into "
        "AggregatedRenderPass");
    return quad_list.AllocateAndCopyFrom(DrawQuadType::MaterialCast(quad));
  }
};

using AggregatedRenderPassList =
    std::vector<std::unique_ptr<AggregatedRenderPass>>;

}  // namespace viz
#endif  // COMPONENTS_VIZ_COMMON_QUADS_AGGREGATED_RENDER_PASS_H_
