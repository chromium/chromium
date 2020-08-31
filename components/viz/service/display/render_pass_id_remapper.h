// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ID_REMAPPER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ID_REMAPPER_H_

#include <utility>

#include "base/containers/flat_map.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {

// A class responsible for remapping surface namespace render pass ids to a
// global namespace to avoid collisions.
class RenderPassIdRemapper {
 public:
  RenderPassIdRemapper();
  RenderPassIdRemapper(const RenderPassIdRemapper&) = delete;
  ~RenderPassIdRemapper();

  RenderPassIdRemapper& operator=(const RenderPassIdRemapper&) = delete;

  AggregatedRenderPassId Remap(CompositorRenderPassId surface_local_pass_id,
                               const SurfaceId& surface_id);
  AggregatedRenderPassId NextAvailableId();

  void ClearUnusedMappings();

 private:
  struct RenderPassInfo {
    RenderPassInfo();
    RenderPassInfo(const RenderPassInfo& other);
    ~RenderPassInfo();

    RenderPassInfo& operator=(const RenderPassInfo& other);

    // This is the id the pass is mapped to.
    AggregatedRenderPassId id;
    // This is true if the pass was used in the last aggregated frame.
    bool in_use = true;
  };

  base::flat_map<std::pair<SurfaceId, CompositorRenderPassId>, RenderPassInfo>
      render_pass_allocator_map_;
  AggregatedRenderPassId::Generator render_pass_id_generator_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ID_REMAPPER_H_
