// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/render_pass_id_remapper.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {

RenderPassIdRemapper::RenderPassIdRemapper() = default;

RenderPassIdRemapper::~RenderPassIdRemapper() = default;

AggregatedRenderPassId RenderPassIdRemapper::Remap(
    CompositorRenderPassId surface_local_pass_id,
    const SurfaceId& surface_id) {
  auto key = std::make_pair(surface_id, surface_local_pass_id);
  auto it = render_pass_allocator_map_.find(key);
  if (it != render_pass_allocator_map_.end()) {
    it->second.in_use = true;
    return it->second.id;
  }

  RenderPassInfo render_pass_info;
  render_pass_info.id = NextAvailableId();
  render_pass_allocator_map_[key] = render_pass_info;
  return render_pass_info.id;
}

AggregatedRenderPassId RenderPassIdRemapper::NextAvailableId() {
  return render_pass_id_generator_.GenerateNextId();
}

void RenderPassIdRemapper::ClearUnusedMappings() {
  // Remove all render pass mappings that weren't used in the current frame.
  for (auto it = render_pass_allocator_map_.begin();
       it != render_pass_allocator_map_.end();) {
    if (it->second.in_use) {
      it->second.in_use = false;
      it++;
    } else {
      it = render_pass_allocator_map_.erase(it);
    }
  }
}

RenderPassIdRemapper::RenderPassInfo::RenderPassInfo() = default;
RenderPassIdRemapper::RenderPassInfo::RenderPassInfo(
    const RenderPassInfo& other) = default;
RenderPassIdRemapper::RenderPassInfo::~RenderPassInfo() = default;

RenderPassIdRemapper::RenderPassInfo&
RenderPassIdRemapper::RenderPassInfo::operator=(const RenderPassInfo& other) =
    default;

}  // namespace viz
