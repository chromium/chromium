// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/hit_test/hit_test_query.h"

#include "base/containers/stack.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
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
  std::string names = "";
  uint32_t mask = 1;

  while (flag) {
    std::string name = "";
    switch (flag & mask) {
      case kHitTestMine:
        name = "Mine";
        break;
      case kHitTestIgnore:
        name = "Ignore";
        break;
      case kHitTestChildSurface:
        name = "ChildSurface";
        break;
      case kHitTestAsk:
        name = "Ask";
        break;
      case kHitTestMouse:
        name = "Mouse";
        break;
      case kHitTestTouch:
        name = "Touch";
        break;
      case kHitTestNotActive:
        name = "NotActive";
        break;
      case 0:
        break;
    }
    if (!name.empty()) {
      names += names.empty() ? name : ", " + name;
    }

    flag &= ~mask;
    mask <<= 1;
  }

  return names;
}

}  // namespace

HitTestQuery::HitTestQuery(base::RepeatingClosure bad_message_gpu_callback)
    : bad_message_gpu_callback_(std::move(bad_message_gpu_callback)) {}

HitTestQuery::~HitTestQuery() = default;

void HitTestQuery::OnAggregatedHitTestRegionListUpdated(
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  hit_test_data_.clear();
  hit_test_data_ = hit_test_data;
}

Target HitTestQuery::FindTargetForLocation(
    EventSource event_source,
    const gfx::PointF& location_in_root) const {
  if (hit_test_data_.empty())
    return Target();

  base::ElapsedTimer target_timer;
  Target target;
  FindTargetInRegionForLocation(event_source, location_in_root, 0, &target);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Event.VizHitTest.TargetTimeUs",
                                          target_timer.Elapsed(),
                                          base::TimeDelta::FromMicroseconds(1),
                                          base::TimeDelta::FromSeconds(10), 50);
  return target;
}

bool HitTestQuery::TransformLocationForTarget(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    const gfx::PointF& location_in_root,
    gfx::PointF* transformed_location) const {
  base::ElapsedTimer transform_timer;

  if (hit_test_data_.empty())
    return false;

  // Use GetTransformToTarget if |target_ancestors| only has the target.
  if (target_ancestors.size() == 1u) {
    gfx::Transform transform;
    if (!GetTransformToTarget(target_ancestors.front(), &transform))
      return false;

    *transformed_location = location_in_root;
    transform.TransformPoint(transformed_location);
    return true;
  }

  if (target_ancestors.size() == 0u ||
      target_ancestors[target_ancestors.size() - 1] !=
          hit_test_data_[0].frame_sink_id) {
    return false;
  }

  // TODO(riajiang): Cache the matrix product such that the transform can be
  // done immediately. crbug/758062.
  *transformed_location = location_in_root;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Event.VizHitTest.TransformTimeUs",
                                          transform_timer.Elapsed(),
                                          base::TimeDelta::FromMicroseconds(1),
                                          base::TimeDelta::FromSeconds(10), 50);
  return TransformLocationForTargetRecursively(event_source, target_ancestors,
                                               target_ancestors.size() - 1, 0,
                                               transformed_location);
}

bool HitTestQuery::GetTransformToTarget(const FrameSinkId& target,
                                        gfx::Transform* transform) const {
  if (hit_test_data_.empty())
    return false;

  return GetTransformToTargetRecursively(target, 0, transform);
}

bool HitTestQuery::ContainsActiveFrameSinkId(
    const FrameSinkId& frame_sink_id) const {
  for (auto& it : hit_test_data_) {
    if (it.frame_sink_id == frame_sink_id &&
        !(it.flags & HitTestRegionFlags::kHitTestNotActive)) {
      return true;
    }
  }
  return false;
}

bool HitTestQuery::FindTargetInRegionForLocation(
    EventSource event_source,
    const gfx::PointF& location_in_parent,
    size_t region_index,
    Target* target) const {
  gfx::PointF location_transformed(location_in_parent);

  // HasPerspective() is checked for the transform because the point will not
  // be transformed correctly for a plane with a different normal.
  // See https://crbug.com/854247.
  if (hit_test_data_[region_index].transform().HasPerspective()) {
    target->frame_sink_id = hit_test_data_[region_index].frame_sink_id;
    target->location_in_target = gfx::PointF();
    target->flags = HitTestRegionFlags::kHitTestAsk;
    return true;
  }

  hit_test_data_[region_index].transform().TransformPoint(
      &location_transformed);
  if (!gfx::RectF(hit_test_data_[region_index].rect)
           .Contains(location_transformed)) {
    return false;
  }

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       hit_test_data_.size() - region_index))
    return false;

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  gfx::PointF location_in_target =
      location_transformed -
      hit_test_data_[region_index].rect.OffsetFromOrigin();
  while (child_region < child_region_end) {
    if (FindTargetInRegionForLocation(event_source, location_in_target,
                                      child_region, target)) {
      return true;
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  const uint32_t flags = hit_test_data_[region_index].flags;
  if (!RegionMatchEventSource(event_source, flags))
    return false;
  if (flags &
      (HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestAsk)) {
    target->frame_sink_id = hit_test_data_[region_index].frame_sink_id;
    target->location_in_target = location_in_target;
    target->flags = flags;
    return true;
  }
  return false;
}

bool HitTestQuery::TransformLocationForTargetRecursively(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    size_t target_ancestor,
    size_t region_index,
    gfx::PointF* location_in_target) const {
  const uint32_t flags = hit_test_data_[region_index].flags;
  if ((flags & HitTestRegionFlags::kHitTestChildSurface) == 0u &&
      !RegionMatchEventSource(event_source, flags)) {
    return false;
  }

  hit_test_data_[region_index].transform().TransformPoint(location_in_target);
  location_in_target->Offset(-hit_test_data_[region_index].rect.x(),
                             -hit_test_data_[region_index].rect.y());
  if (!target_ancestor)
    return true;

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       hit_test_data_.size() - region_index)) {
    return false;
  }

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    if (hit_test_data_[child_region].frame_sink_id ==
        target_ancestors[target_ancestor - 1]) {
      return TransformLocationForTargetRecursively(
          event_source, target_ancestors, target_ancestor - 1, child_region,
          location_in_target);
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
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
  // TODO(riajiang): Cache the matrix product such that the transform can be
  // found immediately.
  if (hit_test_data_[region_index].frame_sink_id == target) {
    *transform = hit_test_data_[region_index].transform();
    transform->Translate(-hit_test_data_[region_index].rect.x(),
                         -hit_test_data_[region_index].rect.y());
    return true;
  }

  const int32_t region_child_count = hit_test_data_[region_index].child_count;
  if (!CheckChildCount(region_child_count,
                       hit_test_data_.size() - region_index)) {
    return false;
  }

  size_t child_region = region_index + 1;
  size_t child_region_end = child_region + region_child_count;
  while (child_region < child_region_end) {
    gfx::Transform transform_to_child;
    if (GetTransformToTargetRecursively(target, child_region,
                                        &transform_to_child)) {
      gfx::Transform region_transform(hit_test_data_[region_index].transform());
      region_transform.Translate(-hit_test_data_[region_index].rect.x(),
                                 -hit_test_data_[region_index].rect.y());
      *transform = transform_to_child * region_transform;
      return true;
    }

    const int32_t child_region_child_count =
        hit_test_data_[child_region].child_count;
    if (!CheckChildCount(child_region_child_count, region_child_count))
      return false;

    child_region = child_region + child_region_child_count + 1;
  }

  return false;
}

void HitTestQuery::ReceivedBadMessageFromGpuProcess() const {
  if (!bad_message_gpu_callback_.is_null())
    bad_message_gpu_callback_.Run();
}

std::string HitTestQuery::PrintHitTestData() const {
  std::ostringstream oss;
  base::stack<uint32_t> parents;
  std::string tabs = "";

  for (uint32_t i = 0; i < hit_test_data_.size(); ++i) {
    const AggregatedHitTestRegion& htr = hit_test_data_[i];

    oss << tabs << "Index: " << i << '\n';
    oss << tabs << "Children: " << htr.child_count << '\n';
    oss << tabs << "Flags: " << GetFlagNames(htr.flags) << '\n';
    oss << tabs << "Frame Sink Id: " << htr.frame_sink_id.ToString() << '\n';
    oss << tabs << "Rect: " << htr.rect.ToString() << '\n';
    oss << tabs << "Transform:" << '\n';

    // gfx::Transform::ToString spans multiple lines, so we use an additional
    // stringstream.
    {
      std::string s;
      std::stringstream transform_ss;

      transform_ss << htr.transform().ToString() << '\n';

      while (getline(transform_ss, s)) {
        oss << tabs << s << '\n';
      }
    }

    tabs += "\t\t";
    parents.push(i);

    while (!parents.empty() &&
           parents.top() + hit_test_data_[parents.top()].child_count <= i) {
      tabs.pop_back();
      tabs.pop_back();

      parents.pop();
    }
  }

  return oss.str();
}

}  // namespace viz
