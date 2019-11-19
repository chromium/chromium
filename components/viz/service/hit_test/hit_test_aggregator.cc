// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_conversions.h"

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
      max_region_size_(max_region_size) {}

HitTestAggregator::~HitTestAggregator() = default;

void HitTestAggregator::Aggregate(const SurfaceId& display_surface_id,
                                  RenderPassList* render_passes) {
  DCHECK(referenced_child_regions_.empty());

  // The index will only have changed when new hit-test data has been submitted.
  uint64_t submit_hit_test_region_list_index =
      hit_test_manager_->submit_hit_test_region_list_index();

  if (submit_hit_test_region_list_index ==
      last_submit_hit_test_region_list_index_) {
    return;
  }

  last_submit_hit_test_region_list_index_ = submit_hit_test_region_list_index;
  // Reset states.
  hit_test_data_.clear();
  hit_test_data_capacity_ = initial_region_size_;
  hit_test_data_size_ = 0;
  hit_test_data_.resize(hit_test_data_capacity_);

  hit_test_debug_ = false;
  hit_test_debug_ask_regions_ = 0;

  base::ElapsedTimer aggregate_timer;
  AppendRoot(display_surface_id);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Event.VizHitTest.AggregateTimeUs",
                                          aggregate_timer.Elapsed(),
                                          base::TimeDelta::FromMicroseconds(1),
                                          base::TimeDelta::FromSeconds(10), 50);
  SendHitTestData();

  if (hit_test_debug_ && render_passes) {
    InsertHitTestDebugQuads(render_passes);
  }
}

void HitTestAggregator::InsertHitTestDebugQuads(RenderPassList* render_passes) {
  const base::flat_set<FrameSinkId>* hit_test_async_queried_debug_regions =
      hit_test_manager_->GetHitTestAsyncQueriedDebugRegions(
          root_frame_sink_id_);

  QuadList& ql = render_passes->back()->quad_list;
  ql.InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
      ql.begin(), hit_test_data_size_);
  ql.InsertBeforeAndInvalidateAllPointers<SolidColorDrawQuad>(
      ql.begin(), hit_test_debug_ask_regions_);

  SharedQuadState* sqs =
      render_passes->back()->CreateAndAppendSharedQuadState();
  sqs->opacity = 0.25f;

  const SkColor colors[3] = {SK_ColorCYAN, SK_ColorGREEN, SK_ColorMAGENTA};

  base::stack<uint32_t> parents;
  base::stack<gfx::Transform> parent_transforms;
  parent_transforms.push(gfx::Transform());

  for (uint32_t i = 0, ask_i = 0; i < hit_test_data_size_; ++i) {
    const AggregatedHitTestRegion& htr = hit_test_data_[i];

    gfx::Transform child_to_parent;
    // Hit-test transforms are guaranteed to be invertible.
    if (!htr.transform().GetInverse(&child_to_parent)) {
      return;
    }

    SkColor color = colors[parents.size() % 3];
    if (hit_test_async_queried_debug_regions &&
        hit_test_async_queried_debug_regions->count(htr.frame_sink_id)) {
      color = SK_ColorRED;
    }

    parents.push(i);
    // Concatenate transformation.
    parent_transforms.push(parent_transforms.top() * child_to_parent);

    // We can only transform gfx::RectF.
    gfx::RectF rf(hit_test_data_[i].rect);
    parent_transforms.top().TransformRect(&rf);
    const gfx::Rect debug_rect = gfx::ToEnclosedRect(rf);

    DebugBorderDrawQuad* debug_quad = static_cast<DebugBorderDrawQuad*>(
        ql.ElementAt(hit_test_debug_ask_regions_ + i));
    debug_quad->SetNew(sqs, debug_rect, debug_rect, color, /*width=*/5);

    if (htr.flags & kHitTestAsk) {
      SolidColorDrawQuad* ask_quad =
          static_cast<SolidColorDrawQuad*>(ql.ElementAt(ask_i++));
      ask_quad->SetNew(sqs, debug_rect, debug_rect, color,
                       /*force_anti_aliasing_off=*/false);
    }

    while (!parents.empty()) {
      uint32_t parent_index = parents.top();
      uint32_t max_child_index =
          parent_index + hit_test_data_[parent_index].child_count;
      if (max_child_index > i)
        break;
      parents.pop();
      parent_transforms.pop();
    }
  }
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

  DCHECK(referenced_child_regions_.empty());
  referenced_child_regions_.insert(surface_id.frame_sink_id());

  size_t region_index = 1;
  for (const auto& region : hit_test_region_list->regions) {
    if (region_index >= hit_test_data_capacity_ - 1)
      break;
    region_index = AppendRegion(region_index, region);
    DCHECK_EQ(referenced_child_regions_.size(), 1u);
  }
  referenced_child_regions_.erase(referenced_child_regions_.begin());

  DCHECK_GE(region_index, 1u);
  int32_t child_count = region_index - 1;
  UMA_HISTOGRAM_COUNTS_1000("Event.VizHitTest.HitTestRegions", region_index);
  SetRegionAt(0, surface_id.frame_sink_id(), hit_test_region_list->flags,
              hit_test_region_list->async_hit_test_reasons,
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
  uint32_t reasons = region.async_hit_test_reasons;
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
      reasons |= AsyncHitTestReasons::kRegionNotActive;
    } else {
      // Rather than add a node in the tree for this hit_test_region_list
      // element we can simplify the tree by merging the flags and transform
      // into the kHitTestChildSurface element.
      if (!hit_test_region_list->transform.IsIdentity())
        transform.PreconcatTransform(hit_test_region_list->transform);

      flags |= hit_test_region_list->flags;
      reasons |= hit_test_region_list->async_hit_test_reasons;

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
    referenced_child_regions_.erase(region.frame_sink_id);
  }
  DCHECK_GE(region_index - parent_index - 1, 0u);
  int32_t child_count = region_index - parent_index - 1;
  SetRegionAt(parent_index, region.frame_sink_id, flags, reasons, region.rect,
              transform, child_count);
  return region_index;
}

void HitTestAggregator::SetRegionAt(size_t index,
                                    const FrameSinkId& frame_sink_id,
                                    uint32_t flags,
                                    uint32_t async_hit_test_reasons,
                                    const gfx::Rect& rect,
                                    const gfx::Transform& transform,
                                    int32_t child_count) {
  hit_test_data_[index] =
      AggregatedHitTestRegion(frame_sink_id, flags, rect, transform,
                              child_count, async_hit_test_reasons);
  hit_test_data_size_++;

  hit_test_debug_ |= flags & kHitTestDebug;
  if (flags & kHitTestAsk)
    ++hit_test_debug_ask_regions_;
}

}  // namespace viz
