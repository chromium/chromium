// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace viz {

HitTestAggregator::HitTestAggregator(
    const HitTestManager* hit_test_manager,
    HitTestAggregatorDelegate* delegate,
    LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate,
    const FrameSinkId& frame_sink_id,
    uint32_t initial_region_size,
    uint32_t max_region_size)
    : hit_test_manager_(hit_test_manager),
      delegate_(delegate),
      local_surface_id_lookup_delegate_(local_surface_id_lookup_delegate),
      root_frame_sink_id_(frame_sink_id),
      initial_region_size_(initial_region_size),
      incremental_region_size_(initial_region_size),
      max_region_size_(max_region_size),
      weak_ptr_factory_(this) {}

HitTestAggregator::~HitTestAggregator() = default;

void HitTestAggregator::Aggregate(const SurfaceId& display_surface_id) {
  DCHECK(referenced_child_regions_.empty());

  // Reset states.
  hit_test_data_.clear();
  hit_test_data_capacity_ = initial_region_size_;
  hit_test_data_size_ = 0;
  hit_test_data_.resize(hit_test_data_capacity_);

  base::ElapsedTimer aggregate_timer;
  AppendRoot(display_surface_id);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Event.VizHitTest.AggregateTimeUs",
                                          aggregate_timer.Elapsed(),
                                          base::TimeDelta::FromMicroseconds(1),
                                          base::TimeDelta::FromSeconds(10), 50);
  referenced_child_regions_.clear();
  SendHitTestData();
}

void HitTestAggregator::SendHitTestData() {
  hit_test_data_.resize(hit_test_data_size_);
  delegate_->OnAggregatedHitTestRegionListUpdated(root_frame_sink_id_,
                                                  hit_test_data_);
}

base::Optional<int64_t> HitTestAggregator::GetTraceIdIfUpdated(
    const SurfaceId& surface_id,
    uint64_t active_frame_index) {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"), &enabled);
  if (!enabled)
    return base::nullopt;

  uint64_t& frame_index = last_active_frame_index_[surface_id.frame_sink_id()];
  if (frame_index == active_frame_index)
    return base::nullopt;
  frame_index = active_frame_index;
  return ~hit_test_manager_->GetTraceId(surface_id);
}

void HitTestAggregator::AppendRoot(const SurfaceId& surface_id) {
  uint64_t active_frame_index;
  const HitTestRegionList* hit_test_region_list =
      hit_test_manager_->GetActiveHitTestRegionList(
          local_surface_id_lookup_delegate_, surface_id.frame_sink_id(),
          &active_frame_index);

  if (!hit_test_region_list)
    return;

  base::Optional<int64_t> trace_id =
      GetTraceIdIfUpdated(surface_id, active_frame_index);
  TRACE_EVENT_WITH_FLOW1(
      TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"), "Event.Pipeline",
      TRACE_ID_GLOBAL(trace_id.value_or(-1)),
      trace_id ? TRACE_EVENT_FLAG_FLOW_IN : TRACE_EVENT_FLAG_NONE, "step",
      "AggregateHitTestData(Root)");

  referenced_child_regions_.insert(surface_id.frame_sink_id());

  size_t region_index = 1;
  for (const auto& region : hit_test_region_list->regions) {
    if (region_index >= hit_test_data_capacity_ - 1)
      break;
    region_index = AppendRegion(region_index, region);
  }

  DCHECK_GE(region_index, 1u);
  int32_t child_count = region_index - 1;
  UMA_HISTOGRAM_COUNTS_1000("Event.VizHitTest.HitTestRegions", region_index);
  SetRegionAt(0, surface_id.frame_sink_id(), hit_test_region_list->flags,
              hit_test_region_list->bounds, hit_test_region_list->transform,
              child_count);
}

size_t HitTestAggregator::AppendRegion(size_t region_index,
                                       const HitTestRegion& region) {
  size_t parent_index = region_index++;
  if (region_index >= hit_test_data_capacity_ - 1) {
    if (hit_test_data_capacity_ > max_region_size_) {
      return region_index;
    } else {
      hit_test_data_capacity_ += incremental_region_size_;
      hit_test_data_.resize(hit_test_data_capacity_);
    }
  }

  uint32_t flags = region.flags;
  gfx::Transform transform = region.transform;

  if (region.flags & HitTestRegionFlags::kHitTestChildSurface) {
    if (referenced_child_regions_.count(region.frame_sink_id))
      return parent_index;

    referenced_child_regions_.insert(region.frame_sink_id);

    uint64_t active_frame_index;
    const HitTestRegionList* hit_test_region_list =
        hit_test_manager_->GetActiveHitTestRegionList(
            local_surface_id_lookup_delegate_, region.frame_sink_id,
            &active_frame_index);
    if (!hit_test_region_list) {
      // Hit-test data not found with this FrameSinkId. This means that it
      // failed to find a surface corresponding to this FrameSinkId at surface
      // aggregation time. This might be because the embedded client hasn't
      // submitted its own hit-test data yet, we are going to do async
      // targeting for this embedded client.
      flags |= (HitTestRegionFlags::kHitTestAsk |
                HitTestRegionFlags::kHitTestNotActive);
    } else {
      // Rather than add a node in the tree for this hit_test_region_list
      // element we can simplify the tree by merging the flags and transform
      // into the kHitTestChildSurface element.
      if (!hit_test_region_list->transform.IsIdentity())
        transform.PreconcatTransform(hit_test_region_list->transform);

      flags |= hit_test_region_list->flags;

      bool enabled;
      TRACE_EVENT_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"), &enabled);
      if (enabled) {
        // Preconditions are already verified in GetActiveHitTestRegionList.
        LocalSurfaceId local_surface_id =
            local_surface_id_lookup_delegate_->GetSurfaceAtAggregation(
                region.frame_sink_id);
        SurfaceId surface_id(region.frame_sink_id, local_surface_id);

        base::Optional<int64_t> trace_id =
            GetTraceIdIfUpdated(surface_id, active_frame_index);
        TRACE_EVENT_WITH_FLOW1(
            TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"), "Event.Pipeline",
            TRACE_ID_GLOBAL(trace_id.value_or(-1)),
            trace_id ? TRACE_EVENT_FLAG_FLOW_IN : TRACE_EVENT_FLAG_NONE, "step",
            "AggregateHitTestData");
      }

      for (const auto& child_region : hit_test_region_list->regions) {
        region_index = AppendRegion(region_index, child_region);
        if (region_index >= hit_test_data_capacity_ - 1)
          break;
      }
    }
  }
  DCHECK_GE(region_index - parent_index - 1, 0u);
  int32_t child_count = region_index - parent_index - 1;
  SetRegionAt(parent_index, region.frame_sink_id, flags, region.rect, transform,
              child_count);
  return region_index;
}

void HitTestAggregator::SetRegionAt(size_t index,
                                    const FrameSinkId& frame_sink_id,
                                    uint32_t flags,
                                    const gfx::Rect& rect,
                                    const gfx::Transform& transform,
                                    int32_t child_count) {
  hit_test_data_[index] = AggregatedHitTestRegion(frame_sink_id, flags, rect,
                                                  transform, child_count);
  hit_test_data_size_++;
}

}  // namespace viz
