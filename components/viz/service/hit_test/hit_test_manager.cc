// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_manager.h"

#include <utility>
#include <vector>

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {

namespace {
// TODO(gklassen): Review and select appropriate sizes based on
// telemetry / UMA.
constexpr uint32_t kMaxRegionsPerSurface = 1024;

// Whenever a hit test region is marked as kHitTestAsk there must be a reason
// for async hit test and vice versa.
bool FlagsAndAsyncReasonsMatch(uint32_t flags,
                               uint32_t async_hit_test_reasons) {
  if (flags & kHitTestAsk)
    return async_hit_test_reasons != kNotAsyncHitTest;
  return async_hit_test_reasons == kNotAsyncHitTest;
}

}  // namespace

HitTestManager::HitTestManager(SurfaceManager* surface_manager)
    : surface_manager_(surface_manager) {}

HitTestManager::~HitTestManager() = default;

void HitTestManager::OnSurfaceDestroyed(const SurfaceId& surface_id) {
  hit_test_region_lists_.erase(surface_id);
}

void HitTestManager::OnSurfaceActivated(const SurfaceId& surface_id) {
  // When a Surface is activated we can confidently remove all
  // associated HitTestRegionList objects with an older frame_index.
  auto search = hit_test_region_lists_.find(surface_id);
  if (search == hit_test_region_lists_.end())
    return;

  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  uint64_t frame_index = surface->GetActiveFrameIndex();

  auto& frame_index_map = search->second;
  for (auto it = frame_index_map.begin(); it != frame_index_map.end();) {
    if (it->first != frame_index)
      it = frame_index_map.erase(it);
    else
      ++it;
  }
}

void HitTestManager::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    const uint64_t frame_index,
    std::optional<HitTestRegionList> hit_test_region_list) {
  if (!hit_test_region_list) {
    auto& frame_index_map = hit_test_region_lists_[surface_id];
    if (!frame_index_map.empty()) {
      // We will reuse the last submitted hit-test data.
      uint64_t last_frame_index = frame_index_map.rbegin()->first;

      HitTestRegionList last_hit_test_region_list =
          std::move(frame_index_map[last_frame_index]);

      frame_index_map[frame_index] = std::move(last_hit_test_region_list);
      frame_index_map.erase(last_frame_index);
    }
    return;
  }
  if (!ValidateHitTestRegionList(surface_id, &*hit_test_region_list))
    return;
  ++submit_hit_test_region_list_index_;

  // TODO(gklassen): Runtime validation that hit_test_region_list is valid.
  // TODO(gklassen): Inform FrameSink that the hit_test_region_list is invalid.
  // TODO(gklassen): FrameSink needs to inform the host of a difficult renderer.
  hit_test_region_lists_[surface_id][frame_index] =
      std::move(*hit_test_region_list);
}

const HitTestRegionList* HitTestManager::GetActiveHitTestRegionList(
    LatestLocalSurfaceIdLookupDelegate* delegate,
    const FrameSinkId& frame_sink_id,
    uint64_t* store_active_frame_index) const {
  if (!delegate)
    return nullptr;

  LocalSurfaceId local_surface_id =
      delegate->GetSurfaceAtAggregation(frame_sink_id);
  if (!local_surface_id.is_valid())
    return nullptr;

  SurfaceId surface_id(frame_sink_id, local_surface_id);
  auto search = hit_test_region_lists_.find(surface_id);
  if (search == hit_test_region_lists_.end())
    return nullptr;

  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  uint64_t frame_index = surface->GetActiveFrameIndex();
  if (store_active_frame_index)
    *store_active_frame_index = frame_index;

  auto& frame_index_map = search->second;
  auto search2 = frame_index_map.find(frame_index);
  if (search2 == frame_index_map.end())
    return nullptr;

  return &search2->second;
}

int64_t HitTestManager::GetTraceId(const SurfaceId& id) const {
  Surface* surface = surface_manager_->GetSurfaceForId(id);
  return surface->GetActiveFrameMetadata().begin_frame_ack.trace_id;
}

bool HitTestManager::ValidateHitTestRegionList(
    const SurfaceId& surface_id,
    HitTestRegionList* hit_test_region_list) {
  if (hit_test_region_list->regions.size() > kMaxRegionsPerSurface)
    return false;
  if (!FlagsAndAsyncReasonsMatch(
          hit_test_region_list->flags,
          hit_test_region_list->async_hit_test_reasons)) {
    return false;
  }
  for (auto& region : hit_test_region_list->regions) {
    // TODO(gklassen): Ensure that |region->frame_sink_id| is a child of
    // |frame_sink_id|.
    if (region.frame_sink_id.client_id() == 0) {
      region.frame_sink_id = FrameSinkId(surface_id.frame_sink_id().client_id(),
                                         region.frame_sink_id.sink_id());
    }
    if (!FlagsAndAsyncReasonsMatch(region.flags, region.async_hit_test_reasons))
      return false;
  }
  return true;
}

}  // namespace viz
