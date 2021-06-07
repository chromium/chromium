// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <utility>

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

void ResolvedFrameData::UpdateResolvedPassData(
    std::vector<ResolvedPassData> resolved_passes) {
  DCHECK(!resolved_passes.empty());

  frame_index_ = surface_->GetActiveFrameIndex();
  DCHECK_NE(frame_index_, 0u);

  resolved_passes_ = std::move(resolved_passes);

  // Build a map from render pass id to data.
  std::vector<std::pair<CompositorRenderPassId, ResolvedPassData*>> entries;
  entries.reserve(resolved_passes_.size());
  for (auto& resolved_pass : resolved_passes_)
    entries.emplace_back(resolved_pass.render_pass->id, &resolved_pass);
  render_pass_id_map_ =
      base::flat_map<CompositorRenderPassId, ResolvedPassData*>(
          std::move(entries));

  valid_ = true;
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

ResolvedPassData& ResolvedFrameData::GetRenderPassDataById(
    CompositorRenderPassId render_pass_id) {
  DCHECK(valid_);

  // TODO(kylechar): We need to validate that RenderPassDrawQuads only refer to
  // CompositorRenderPassIds that exist.
  auto iter = render_pass_id_map_.find(render_pass_id);
  DCHECK(iter != render_pass_id_map_.end());
  return *iter->second;
}

ResolvedPassData& ResolvedFrameData::GetRenderPassDataByIndex(size_t index) {
  DCHECK(valid_);
  return resolved_passes_[index];
}

ResolvedPassData& ResolvedFrameData::GetRootRenderPassData() {
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
