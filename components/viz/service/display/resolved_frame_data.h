// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

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

  CompositorRenderPass* render_pass;
  std::vector<ResolvedQuadData> draw_quads;

  // Tracks if prewalk is visiting this render pass to avoid cycles.
  bool is_visited = false;
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
  Surface* surface() { return surface_; }
  bool is_valid() const { return valid_; }
  uint64_t frame_index() const { return frame_index_; }

  // Update the list of resolved pass data. This will set frame index and mark
  // as valid.
  void UpdateResolvedPassData(std::vector<ResolvedPassData> resolved_passes);

  // Sets frame index and marks as invalid. This also clears any existing
  // resolved pass data.
  void SetInvalid();

  // RenderPassData accessors. These should only be used if is_valid() returns
  // true.
  size_t RenderPassCount() const;
  ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id);
  ResolvedPassData& GetRenderPassDataByIndex(size_t index);
  ResolvedPassData& GetRootRenderPassData();

  // Marks this as used and returns true if this was the first time MarkAsUsed()
  // was called since last reset.
  bool MarkAsUsed();

  // Returns true if MarkAsUsed() was called since last reset and then resets
  // used to false.
  bool CheckIfUsedAndReset();

 private:
  const SurfaceId surface_id_;
  Surface* const surface_;

  // Data associated with CompositorFrame with |frame_index_|.
  bool valid_ = false;
  uint64_t frame_index_ = 0;
  std::vector<ResolvedPassData> resolved_passes_;
  base::flat_map<CompositorRenderPassId, ResolvedPassData*> render_pass_id_map_;

  // Track if the this resolved frame was used this frame.
  bool used_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
