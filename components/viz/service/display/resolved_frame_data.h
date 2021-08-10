// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_

#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/surface_id.h"
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

// Data associated with a CompositorRenderPass in a resolved frame.
struct VIZ_SERVICE_EXPORT ResolvedPassData {
  ResolvedPassData();
  ~ResolvedPassData();
  ResolvedPassData(ResolvedPassData&& other);
  ResolvedPassData& operator=(ResolvedPassData&& other);

  CompositorRenderPass* render_pass = nullptr;
  AggregatedRenderPassId remapped_id;
  bool is_root = false;
  std::vector<ResolvedQuadData> draw_quads;
};

// Holds computed information for a particular Surface+CompositorFrame. The
// CompositorFrame computed information will be updated whenever the active
// frame for the surface has changed.
class VIZ_SERVICE_EXPORT ResolvedFrameData {
 public:
  ResolvedFrameData(const SurfaceId& surface_id, Surface* surface);
  ~ResolvedFrameData();
  ResolvedFrameData(ResolvedFrameData&& other) = delete;
  ResolvedFrameData& operator=(ResolvedFrameData&& other) = delete;

  const SurfaceId& surface_id() const { return surface_id_; }
  Surface* surface() const { return surface_; }
  bool is_valid() const { return valid_; }
  uint64_t frame_index() const { return frame_index_; }

  // Updates resolved frame data for a new active frame. This will recompute
  // ResolvedPassData. |child_to_parent_map| is the ResourceId mapping provided
  // from DisplayResourceProvider which includes all of ResourceIds referenced
  // by quads in the active frame. Returns all ResourceIds that are used in the
  // active frame.
  //
  // This performs the following validation on the active CompositorFrame.
  // 1. Checks each ResourceId was registered with DisplayResourceProvider and
  //    is in |child_to_parent_map|.
  // 2. Checks that CompositorRenderPasses have unique ids.
  // 3. Checks that CompositorRenderPassDrawQuads only embed render passes that
  //    are drawn before. This has the side effect of disallowing any cycles.
  //
  // If validation fails then an empty set of resources will be returned, all
  // ResolvedPassData will be cleared and is_valid() will return false.
  ResourceIdSet UpdateForActiveFrame(
      const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
          child_to_parent_map,
      AggregatedRenderPassId::Generator& render_pass_id_generator);

  // Sets frame index and marks as invalid. This also clears any existing
  // resolved pass data.
  void SetInvalid();

  // Marks this as used and returns true if this was the first time MarkAsUsed()
  // was called since last reset.
  bool MarkAsUsed();

  // Returns true if MarkAsUsed() was called since last reset and then resets
  // used to false.
  bool CheckIfUsedAndReset();

  // All functions after this point are accessors for the resolved frame and
  // should only be called if is_valid() returns true.

  // RenderPassData accessors.
  size_t RenderPassCount() const;
  const ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id) const;
  const ResolvedPassData& GetRenderPassDataByIndex(size_t index) const;
  const ResolvedPassData& GetRootRenderPassData() const;
  const std::vector<ResolvedPassData>& GetResolvedPasses() const {
    return resolved_passes_;
  }

  // Returns active frame damage rect. If |include_per_quad_damage| then the
  // damage_rect will include unioned per quad damage, otherwise it will be
  // limited to the root render passes damage_rect.
  const gfx::Rect& GetDamageRect(bool include_per_quad_damage) const;

  // Returns the root render pass output_rect.
  const gfx::Rect& GetOutputRect() const;

 private:
  const SurfaceId surface_id_;
  Surface* const surface_;

  // Data associated with CompositorFrame with |frame_index_|.
  bool valid_ = false;
  uint64_t frame_index_ = 0;
  std::vector<ResolvedPassData> resolved_passes_;
  base::flat_map<CompositorRenderPassId, ResolvedPassData*> render_pass_id_map_;
  base::flat_map<CompositorRenderPassId, AggregatedRenderPassId>
      aggregated_id_map_;
  gfx::Rect root_damage_rect_;

  // Track if the this resolved frame was used this frame.
  bool used_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
