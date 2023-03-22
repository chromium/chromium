// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_

#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

// Returns |damage_rect| field from the DrawQuad if it exists otherwise returns
// an empty optional.
const absl::optional<gfx::Rect>& GetOptionalDamageRectFromQuad(
    const DrawQuad* quad);

// Data associated with a DrawQuad in a resolved frame.
struct VIZ_SERVICE_EXPORT ResolvedQuadData {
  explicit ResolvedQuadData(const DrawQuad& quad);

  // Remapped display ResourceIds.
  DrawQuad::Resources remapped_resources;
};

// Render pass data that is fixed for the lifetime of ResolvedPassData.
struct VIZ_SERVICE_EXPORT FixedPassData {
  FixedPassData();
  FixedPassData(FixedPassData&& other);
  FixedPassData& operator=(FixedPassData&& other);
  ~FixedPassData();

  raw_ptr<CompositorRenderPass, DanglingUntriaged> render_pass = nullptr;
  // DrawQuads in |render_pass| that can contribute additional damage (eg.
  // surface and render passes) that need to be visited during the prewalk phase
  // of aggregation. Stored in front-to-back order like in |render_pass|.
  std::vector<const DrawQuad*> prewalk_quads;

  AggregatedRenderPassId remapped_id;
  bool is_root = false;
  std::vector<ResolvedQuadData> draw_quads;
};

class ResolvedPassData;

// Render pass data that must be recomputed each aggregation. Unlike
// FixedPassData this changes each aggregation depending on what other
// Surfaces/CompositorFrames are part of the draw tree.
struct VIZ_SERVICE_EXPORT AggregationPassData {
  AggregationPassData();
  AggregationPassData(AggregationPassData&& other);
  AggregationPassData& operator=(AggregationPassData&& other);
  ~AggregationPassData();

  // Resets to default constructed state.
  void Reset();

  // Embedded render passes that contribute pixels to this render pass.
  base::flat_set<ResolvedPassData*> embedded_passes;

  // True if the render pass is drawn to fulfil part of a copy request. This
  // property is transitive from parent pass to embedded passes.
  bool in_copy_request_pass = false;

  // True if the render pass is be impacted by a pixel moving foreground filter.
  // This property is transitive from parent pass to embedded passes.
  bool in_pixel_moving_filter_pass = false;

  // True if the render pass will be stored as part of a cached render pass.
  // This property is transitive from parent pass to embedded passes.
  bool in_cached_render_pass = false;

  // True if there is accumulated damage from contributing render pass or
  // surface quads.
  bool has_damage_from_contributing_content = false;

  // Indicates that the render pass is embedded from the root surface root
  // render pass and will contribute pixels to framebuffer. Render passes this
  // is false for may still be drawn but they won't contribute pixels to
  // framebuffer.
  bool will_draw = false;
};

// Data associated with a CompositorRenderPass in a resolved frame. Has fixed
// portion that does not change and an aggregation portion that does change.
class VIZ_SERVICE_EXPORT ResolvedPassData {
 public:
  explicit ResolvedPassData(FixedPassData fixed_data);
  ~ResolvedPassData();
  ResolvedPassData(ResolvedPassData&& other);
  ResolvedPassData& operator=(ResolvedPassData&& other);

  const CompositorRenderPass& render_pass() const {
    return *fixed_.render_pass;
  }
  AggregatedRenderPassId remapped_id() const { return fixed_.remapped_id; }
  bool is_root() const { return fixed_.is_root; }
  const std::vector<ResolvedQuadData>& draw_quads() const {
    return fixed_.draw_quads;
  }
  const std::vector<const DrawQuad*>& prewalk_quads() const {
    return fixed_.prewalk_quads;
  }

  AggregationPassData& aggregation() { return aggregation_; }
  const AggregationPassData& aggregation() const { return aggregation_; }

 private:
  // Data that is constant for the life of the resolved pass.
  FixedPassData fixed_;

  // Data that will change each aggregation.
  AggregationPassData aggregation_;
};

// Holds computed information for a particular Surface+CompositorFrame. The
// CompositorFrame computed information will be updated whenever the active
// frame for the surface has changed. On destruction any resources registered
// with DisplayResourceProvider will be released.
class VIZ_SERVICE_EXPORT ResolvedFrameData {
 public:
  ResolvedFrameData(DisplayResourceProvider* resource_provider,
                    Surface* surface,
                    uint64_t prev_frame_index);
  ~ResolvedFrameData();
  ResolvedFrameData(ResolvedFrameData&& other) = delete;
  ResolvedFrameData& operator=(ResolvedFrameData&& other) = delete;

  const SurfaceId& surface_id() const { return surface_id_; }
  Surface* surface() const { return surface_; }
  bool is_valid() const { return valid_; }
  uint64_t previous_frame_index() const { return previous_frame_index_; }

  // Returns namespace ID for the client that submitted this frame. This is used
  // to deduplicate layer IDs from different clients.
  uint32_t GetClientNamespaceId() const;

  void SetFullDamageForNextAggregation();

  // Force release all resources registered with display resource provider. Note
  // there must be a new CompositorFrame available that doesn't use any existing
  // resources since resources (might) be missing on next draw.
  void ForceReleaseResource();

  // Updates resolved frame data for a new active frame. This will recompute
  // ResolvedPassData. It also updates display resource provider with resources
  // used in new active frame.
  //
  // This performs the following validation on the active CompositorFrame.
  // 1. Checks each ResourceId was registered with DisplayResourceProvider and
  //    is in |child_to_parent_map|.
  // 2. Checks that CompositorRenderPasses have unique ids.
  // 3. Checks that CompositorRenderPassDrawQuads only embed render passes that
  //    are drawn before. This has the side effect of disallowing any cycles.
  //
  // If validation fails then ResolvedPassData will be cleared and is_valid()
  // will return false.
  void UpdateForActiveFrame(
      AggregatedRenderPassId::Generator& render_pass_id_generator);

  // Sets frame index and marks as invalid. This also clears any existing
  // resolved pass data.
  void SetInvalid();

  void MarkAsUsedInAggregation();
  bool WasUsedInAggregation() const;

  // Resets aggregation data and WasUsedInAggregation() will now return false.
  void ResetAfterAggregation();

  // All functions after this point are accessors for the resolved frame and
  // should only be called if is_valid() returns true.

  // Returns true if the root render pass is embedded from the the root surface
  // root render pass.
  bool WillDraw() const;

  // RenderPassData accessors.
  ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id);
  const ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id) const;

  ResolvedPassData& GetRootRenderPassData();
  const ResolvedPassData& GetRootRenderPassData() const;

  std::vector<ResolvedPassData>& GetResolvedPasses() {
    return resolved_passes_;
  }
  const std::vector<ResolvedPassData>& GetResolvedPasses() const {
    return resolved_passes_;
  }

  // The active CompositorFrame is the same this aggregation as last
  // aggregation, aka nothing has changed.
  bool IsSameFrameAsLastAggregation() const;

  // The active CompositorFrame this aggregation is the next frame in the
  // sequence compared to last aggregation.
  bool IsNextFrameSinceLastAggregation() const;

  // Returns surface damage rect. This is based on changes from the
  // CompositorFrame aggregated last frame. This limited to the root render
  // passes damage_rect and does not include individual quads that add damage.
  gfx::Rect GetSurfaceDamage() const;

  // Returns the root render pass output_rect.
  const gfx::Rect& GetOutputRect() const;

 private:
  void RegisterWithResourceProvider();

  const raw_ptr<DisplayResourceProvider> resource_provider_;
  const SurfaceId surface_id_;
  const raw_ptr<Surface> surface_;

  // Child resource ID assigned by `resource_provider_`.
  int child_resource_id_ = 0;

  // Data associated with CompositorFrame with |frame_index_|.
  bool valid_ = false;
  uint64_t frame_index_ = kInvalidFrameIndex;
  std::vector<ResolvedPassData> resolved_passes_;
  base::flat_map<CompositorRenderPassId, ResolvedPassData*> render_pass_id_map_;
  base::flat_map<CompositorRenderPassId, AggregatedRenderPassId>
      aggregated_id_map_;

  uint64_t previous_frame_index_ = kInvalidFrameIndex;

  // Track if the this resolved frame was used this aggregation.
  bool used_in_aggregation_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
