// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/hit_test_query.h"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/stack.h"
#include "base/strings/string_util.h"
#include "components/viz/common/features.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {
namespace {

// If we want to add new source type here, consider switching to use
// ui::EventPointerType instead of EventSource.
bool RegionMatchEventSource(EventSource event_source, uint32_t flags) {
  if (event_source == EventSource::TOUCH)
    return (flags & HitTestRegionFlags::kHitTestTouch) != 0u;
  if (event_source == EventSource::MOUSE)
    return (flags & HitTestRegionFlags::kHitTestMouse) != 0u;
  return (flags & (HitTestRegionFlags::kHitTestMouse |
                   HitTestRegionFlags::kHitTestTouch)) != 0u;
}

bool CheckChildCount(int32_t child_count, size_t child_count_max) {
  return (child_count >= 0) &&
         (static_cast<size_t>(child_count) < child_count_max);
}

const std::string GetFlagNames(uint32_t flag) {
  std::vector<std::string_view> names;

  if (flag & kHitTestMine)
    names.emplace_back("Mine");
  if (flag & kHitTestIgnore)
    names.emplace_back("Ignore");
  if (flag & kHitTestChildSurface)
    names.emplace_back("ChildSurface");
  if (flag & kHitTestAsk)
    names.emplace_back("Ask");
  if (flag & kHitTestMouse)
    names.emplace_back("Mouse");
  if (flag & kHitTestTouch)
    names.emplace_back("Touch");
  if (flag & kHitTestNotActive)
    names.emplace_back("NotActive");

  return base::JoinString(std::move(names), ", ");
}

const std::string GetAsyncHitTestReasons(uint32_t async_hit_test_reasons) {
  std::vector<std::string_view> reasons;

  if (async_hit_test_reasons & kOverlappedRegion)
    reasons.emplace_back("OverlappedRegion");
  if (async_hit_test_reasons & kIrregularClip)
    reasons.emplace_back("IrregularClip");
  if (async_hit_test_reasons & kRegionNotActive)
    reasons.emplace_back("RegionNotActive");
  if (async_hit_test_reasons & kPerspectiveTransform)
    reasons.emplace_back("PerspectiveTransform");
  if (async_hit_test_reasons & kUseDrawQuadData)
    reasons.emplace_back("UseDrawQuadData");

  return reasons.empty() ? "None" : base::JoinString(std::move(reasons), ", ");
}

}  // namespace

HitTestQuery::~HitTestQuery() = default;

HitTestQuery::HitTestQuery(std::optional<base::SafeRef<DataProvider>> provider)
    : provider_(provider) {}

void HitTestQuery::OnAggregatedHitTestRegionListUpdated(
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  CHECK(!provider_.has_value());
  hit_test_data_.clear();
  hit_test_data_ = hit_test_data;
}

Target HitTestQuery::FindTargetForLocation(
    EventSource event_source,
    const gfx::PointF& location_in_root) const {
  if (GetHitTestRegionData().empty()) {
    return Target();
  }
  return FindTargetForLocationStartingFromImpl(
      event_source, location_in_root, GetHitTestRegionData()[0].frame_sink_id,
      /* is_location_relative_to_parent */ true);
}

Target HitTestQuery::FindTargetForLocationStartingFrom(
    EventSource event_source,
    const gfx::PointF& location,
    const FrameSinkId& frame_sink_id) const {
  return FindTargetForLocationStartingFromImpl(
      event_source, location, frame_sink_id,
      /* is_location_relative_to_parent */ false);
}

bool HitTestQuery::TransformLocationForTarget(
    const std::vector<FrameSinkId>& target_ancestors,
    const gfx::PointF& location_in_root,
    gfx::PointF* transformed_location) const {
  if (GetHitTestRegionData().empty()) {
    return false;
  }

  // Use GetTransformToTarget if |target_ancestors| only has the target.
  if (target_ancestors.size() == 1u) {
    gfx::Transform transform;
    if (!GetTransformToTarget(target_ancestors.front(), &transform))
      return false;

    *transformed_location = transform.MapPoint(location_in_root);
    return true;
  }

  if (target_ancestors.size() == 0u ||
      target_ancestors[target_ancestors.size() - 1] !=
          GetHitTestRegionData()[0].frame_sink_id) {
    return false;
  }

  // TODO(crbug.com/41460941): Cache the matrix product such that the transform
  // can be done immediately.
  *transformed_location = location_in_root;
  return TransformLocationForTargetRecursively(
      target_ancestors, target_ancestors.size() - 1, 0, transformed_location);
}

bool HitTestQuery::GetTransformToTarget(const FrameSinkId& target,
                                        gfx::Transform* transform) const {
  if (GetHitTestRegionData().empty()) {
    return false;
  }

  return GetTransformToTargetRecursively(target, 0, transform);
}

bool HitTestQuery::ContainsActiveFrameSinkId(
    const FrameSinkId& frame_sink_id) const {
  for (auto& it : GetHitTestRegionData()) {
    if (it.frame_sink_id == frame_sink_id &&
        !(it.flags & HitTestRegionFlags::kHitTestNotActive)) {
      return true;
    }
  }
  return false;
}

Target HitTestQuery::FindTargetForLocationStartingFromImpl(
    EventSource event_source,
    const gfx::PointF& location,
    const FrameSinkId& frame_sink_id,
    bool is_location_relative_to_parent) const {
  if (GetHitTestRegionData().empty()) {
    return Target();
  }

  Target target;
  size_t start_index = 0;
  if (!FindIndexOfFrameSink(frame_sink_id, &start_index))
    return Target();

  FindTargetInRegionForLocation(event_source, location, start_index,
                                is_location_relative_to_parent, frame_sink_id,
                                &target);
  return target;
}

bool HitTestQuery::FindTargetInRegionForLocation(
    EventSource event_source,
    const gfx::PointF& location,
    size_t region_index,
    bool is_location_relative_to_parent,
    const FrameSinkId& root_view_frame_sink_id,
    Target* target) const {
  gfx::PointF location_transformed(location);

  // Exclude a region and all its descendants if the region has the ignore bit
  // set.
  if (GetHitTestRegionData()[region_index].flags &
      HitTestRegionFlags::kHitTestIgnore) {
    return false;
  }

  if (is_location_relative_to_parent) {
    // HasPerspective() is checked for the transform because the point will not
    // be transformed correctly for a plane with a different normal.
    // See https://crbug.com/854247.
    if (GetHitTestRegionData()[region_index].transform.HasPerspective()) {
      target->frame_sink_id =
          GetHitTestRegionData()[region_index].frame_sink_id;
      target->location_in_target = gfx::PointF();
      target->flags = HitTestRegionFlags::kHitTestAsk;
      return true;
    }

    location_transformed =
        GetHitTestRegionData()[region_index].transform.MapPoint(
            location_transformed);
    if (!gfx::RectF(GetHitTestRegionData()[region_index].rect)
             .Contains(location_transformed)) {
      return false;
    }
  }

  const int32_t region_child_count =
      GetHitTestRegionData()[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       GetHitTestRegionData().size() - region_index)) {
    return false;
  }

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  gfx::PointF location_in_target = location_transformed;

  const uint32_t flags = GetHitTestRegionData()[region_index].flags;

  DCHECK(GetHitTestRegionData()[region_index].frame_sink_id.is_valid());
  // Root view should not be overlapped by others. However, the root view
  // information is not visible to LTHI::BuildHitTestData(). Therefore the
  // kOverlappedRegion flag could still be set for the root view upon building
  // hit test data, e.g. overlapped by ShelfApp on ChromeOS.
  // The kHitTestAsk flag should be ignored in such a case because there is no
  // need to do async hit testing on the root merely because it was overlapped.
  // TODO(crbug.com/40646023): Do not set the kHitTestAsk and kOverlappedRegion
  // flags for root when building hit test data.
  bool root_view_overlapped =
      GetHitTestRegionData()[region_index].frame_sink_id ==
          root_view_frame_sink_id &&
      GetHitTestRegionData()[region_index].async_hit_test_reasons ==
          AsyncHitTestReasons::kOverlappedRegion;

  // Verify that async_hit_test_reasons is set if and only if there's
  // a kHitTestAsk flag.
  DCHECK_EQ(!!(flags & HitTestRegionFlags::kHitTestAsk),
            !!GetHitTestRegionData()[region_index].async_hit_test_reasons);

  if ((flags & HitTestRegionFlags::kHitTestAsk) &&
      !(flags & HitTestRegionFlags::kHitTestIgnore) && !root_view_overlapped) {
    target->frame_sink_id = GetHitTestRegionData()[region_index].frame_sink_id;
    target->location_in_target = location_in_target;
    target->flags = flags;
    return true;
  }

  // If the current region is not the root view, neither is its children.
  // Therefore when recursively calling FindTargetInRegionForLocation, pass an
  // invalid frame sink id as "root".
  while (child_region < child_region_end) {
    if (FindTargetInRegionForLocation(
            event_source, location_in_target, child_region,
            /*is_location_relative_to_parent=*/true, FrameSinkId(), target)) {
      return true;
    }

    const int32_t child_region_child_count =
        GetHitTestRegionData()[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  if (!RegionMatchEventSource(event_source, flags))
    return false;

  if ((flags & HitTestRegionFlags::kHitTestMine) &&
      !(flags & HitTestRegionFlags::kHitTestIgnore)) {
    target->frame_sink_id = GetHitTestRegionData()[region_index].frame_sink_id;
    target->location_in_target = location_in_target;
    uint32_t target_flags = flags;
    if (root_view_overlapped) {
      DCHECK_EQ(GetHitTestRegionData()[region_index].async_hit_test_reasons,
                AsyncHitTestReasons::kOverlappedRegion);
      target_flags &= ~HitTestRegionFlags::kHitTestAsk;
    }
    target->flags = target_flags;
    // We record fast path hit testing instances with reason kNotAsyncHitTest.
    return true;
  }
  return false;
}

bool HitTestQuery::TransformLocationForTargetRecursively(
    const std::vector<FrameSinkId>& target_ancestors,
    size_t target_ancestor,
    size_t region_index,
    gfx::PointF* location_in_target) const {
  *location_in_target = GetHitTestRegionData()[region_index].transform.MapPoint(
      *location_in_target);
  if (!target_ancestor)
    return true;

  const int32_t region_child_count =
      GetHitTestRegionData()[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       GetHitTestRegionData().size() - region_index)) {
    return false;
  }

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    if (GetHitTestRegionData()[child_region].frame_sink_id ==
        target_ancestors[target_ancestor - 1]) {
      return TransformLocationForTargetRecursively(
          target_ancestors, target_ancestor - 1, child_region,
          location_in_target);
    }

    const int32_t child_region_child_count =
        GetHitTestRegionData()[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  return false;
}

bool HitTestQuery::GetTransformToTargetRecursively(
    const FrameSinkId& target,
    size_t region_index,
    gfx::Transform* transform) const {
  // TODO(crbug.com/41460941): Cache the matrix product such that the transform
  // can be found immediately.
  if (GetHitTestRegionData()[region_index].frame_sink_id == target) {
    *transform = GetHitTestRegionData()[region_index].transform;
    return true;
  }

  const int32_t region_child_count =
      GetHitTestRegionData()[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       GetHitTestRegionData().size() - region_index)) {
    return false;
  }

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    gfx::Transform transform_to_child;
    if (GetTransformToTargetRecursively(target, child_region,
                                        &transform_to_child)) {
      gfx::Transform region_transform(
          GetHitTestRegionData()[region_index].transform);
      *transform = transform_to_child * region_transform;
      return true;
    }

    const int32_t child_region_child_count =
        GetHitTestRegionData()[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  return false;
}

const std::vector<AggregatedHitTestRegion>& HitTestQuery::GetHitTestRegionData()
    const {
  // |provider_| is not null when input is being handled on VizCompositor
  // thread.
  if (provider_.has_value()) {
    return provider_.value()->GetHitTestData();
  } else {
    return hit_test_data_;
  }
}

std::string HitTestQuery::PrintHitTestData() const {
  std::ostringstream oss;
  base::stack<uint32_t> parents;
  std::string tabs = "";

  for (uint32_t i = 0; i < GetHitTestRegionData().size(); ++i) {
    const AggregatedHitTestRegion& htr = GetHitTestRegionData()[i];

    oss << tabs << "Index: " << i << '\n';
    oss << tabs << "Children: " << htr.child_count << '\n';
    oss << tabs << "Flags: " << GetFlagNames(htr.flags) << '\n';
    oss << tabs << "AsyncHitTestReasons: "
        << GetAsyncHitTestReasons(htr.async_hit_test_reasons) << '\n';
    oss << tabs << "Frame Sink Id: " << htr.frame_sink_id.ToString() << '\n';
    oss << tabs << "Rect: " << htr.rect.ToString() << '\n';
    oss << tabs << "Transform:" << '\n';

    // gfx::Transform::ToString spans multiple lines, so we use an additional
    // stringstream.
    {
      std::string s;
      std::stringstream transform_ss;

      transform_ss << htr.transform.ToString() << '\n';

      while (getline(transform_ss, s)) {
        oss << tabs << s << '\n';
      }
    }

    tabs += "\t\t";
    parents.push(i);

    while (!parents.empty() &&
           parents.top() + GetHitTestRegionData()[parents.top()].child_count <=
               i) {
      tabs.pop_back();
      tabs.pop_back();

      parents.pop();
    }
  }

  return oss.str();
}

bool HitTestQuery::FindIndexOfFrameSink(const FrameSinkId& id,
                                        size_t* index) const {
  for (uint32_t i = 0; i < GetHitTestRegionData().size(); ++i) {
    if (GetHitTestRegionData()[i].frame_sink_id == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace viz
