// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {

ResolvedQuadData::ResolvedQuadData(const DrawQuad& quad)
    : remapped_resources(quad.resources) {}

ResolvedPassData::ResolvedPassData() = default;
ResolvedPassData::~ResolvedPassData() = default;
ResolvedPassData::ResolvedPassData(ResolvedPassData&& other) = default;
ResolvedPassData& ResolvedPassData::operator=(ResolvedPassData&& other) =
    default;

ResolvedFrameData::ResolvedFrameData(const SurfaceId& surface_id,
                                     Surface* surface)
    : surface_id_(surface_id), surface_(surface) {}

ResolvedFrameData::~ResolvedFrameData() = default;

ResourceIdSet ResolvedFrameData::UpdateForActiveFrame(
    const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
        child_to_parent_map) {
  auto& compositor_frame = surface_->GetActiveOrInterpolatedFrame();
  auto& resource_list = compositor_frame.resource_list;
  auto& render_passes = compositor_frame.render_pass_list;

  // Figure out which resources are actually used in the render pass.
  // Note that we first gather them in a vector, since ResourceIdSet (which we
  // actually need) is a flat_set, which means bulk insertion we do at the end
  // is more efficient.
  std::vector<ResourceId> referenced_resources;
  referenced_resources.reserve(resource_list.size());

  // Will be repopulated based on active frame.
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  render_pass_id_map_.reserve(render_passes.size());
  resolved_passes_.resize(render_passes.size());

  // Reset and compute new render pass / quad data for this frame. This stores
  // remapped display resource ids.
  for (size_t i = 0; i < render_passes.size(); ++i) {
    auto& render_pass = render_passes[i];
    auto& resolved_pass = resolved_passes_[i];

    resolved_pass.render_pass = render_pass.get();

    // Loop through the quads, remapping resource ids and storing them.
    auto& draw_quads = resolved_passes_[i].draw_quads;
    draw_quads.reserve(render_pass->quad_list.size());
    for (auto* quad : render_pass->quad_list) {
      if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
        // Check CompositorRenderPassDrawQuad refers to a render pass
        // that exists and is drawn before the current render pass.
        auto quad_render_pass_id =
            CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id;
        if (!base::Contains(render_pass_id_map_, quad_render_pass_id)) {
          DLOG(ERROR) << "CompositorRenderPassDrawQuad with invalid id";
          SetInvalid();
          return {};
        }
      }

      draw_quads.emplace_back(*quad);
      for (ResourceId& resource_id : draw_quads.back().remapped_resources) {
        // If we're using a resource which was not declared in the
        // |resource_list| then this is an invalid frame, we can abort.
        auto iter = child_to_parent_map.find(resource_id);
        if (iter == child_to_parent_map.end()) {
          DLOG(ERROR) << "Invalid resource for " << surface_id();
          SetInvalid();
          return {};
        }

        referenced_resources.push_back(resource_id);
        resource_id = iter->second;
      }
    }

    // Build render pass id map and check for duplicate ids at the same time.
    if (!render_pass_id_map_
             .insert(std::make_pair(render_pass->id, &resolved_pass))
             .second) {
      DLOG(ERROR) << "Duplicate render pass ids";
      SetInvalid();
      return {};
    }
  }

  frame_index_ = surface_->GetActiveFrameIndex();
  DCHECK_NE(frame_index_, 0u);

  valid_ = true;
  return ResourceIdSet(std::move(referenced_resources));
}

void ResolvedFrameData::SetInvalid() {
  frame_index_ = surface_->GetActiveFrameIndex();
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  valid_ = false;
}

size_t ResolvedFrameData::RenderPassCount() const {
  DCHECK(valid_);
  return resolved_passes_.size();
}

const ResolvedPassData& ResolvedFrameData::GetRenderPassDataById(
    CompositorRenderPassId render_pass_id) const {
  DCHECK(valid_);

  // TODO(kylechar): We need to validate that RenderPassDrawQuads only refer to
  // CompositorRenderPassIds that exist.
  auto iter = render_pass_id_map_.find(render_pass_id);
  DCHECK(iter != render_pass_id_map_.end());
  return *iter->second;
}

const ResolvedPassData& ResolvedFrameData::GetRenderPassDataByIndex(
    size_t index) const {
  DCHECK(valid_);
  return resolved_passes_[index];
}

const ResolvedPassData& ResolvedFrameData::GetRootRenderPassData() const {
  DCHECK(valid_);
  return resolved_passes_.back();
}

bool ResolvedFrameData::MarkAsUsed() {
  // Returns true the first time this is called after reset.
  return !std::exchange(used_, true);
}

bool ResolvedFrameData::CheckIfUsedAndReset() {
  return std::exchange(used_, false);
}

}  // namespace viz
