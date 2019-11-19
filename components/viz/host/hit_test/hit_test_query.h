// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_
#define COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/host/viz_host_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace viz {

struct Target {
  FrameSinkId frame_sink_id;
  // Coordinates in the coordinate system of the target FrameSinkId.
  gfx::PointF location_in_target;
  // Different flags are defined in services/viz/public/mojom/hit_test/
  // hit_test_region_list.mojom.
  uint32_t flags = 0;
};

enum class EventSource {
  MOUSE,
  TOUCH,
  ANY,
};

// Finds the target for a given location based on the AggregatedHitTestRegion
// list aggregated by HitTestAggregator.
// TODO(crbug.com/966939): Handle 3d space cases correctly.
class VIZ_HOST_EXPORT HitTestQuery {
 public:
  HitTestQuery();
  virtual ~HitTestQuery();

  // HitTestAggregator has sent the most recent |hit_test_data| for targeting/
  // transforming requests.
  void OnAggregatedHitTestRegionListUpdated(
      const std::vector<AggregatedHitTestRegion>& hit_test_data);

  // Finds Target for |location_in_root|, including the FrameSinkId of the
  // target, updated location in the coordinate system of the target and
  // hit-test flags for the target.
  // Assumptions about the AggregatedHitTestRegion list received.
  // 1. The list is in ascending (front to back) z-order.
  // 2. Children count includes children of children.
  // 3. After applying transform to the incoming point, point is in the same
  // coordinate system as the bounds it is comparing against. We shouldn't
  // need to apply rect's origin offset as it should be included in this
  // transform.
  // For example,
  //  +e-------------+
  //  |   +c---------|
  //  | 1 |+a--+     |
  //  |   || 2 |     |
  //  |   |+b--------|
  //  |   ||         |
  //  |   ||   3     |
  //  +--------------+
  // In this case, after applying identity transform, 1 is in the coordinate
  // system of e; apply the transfrom-from-e-to-c and transform-from-c-to-a
  // then we get 2 in the coordinate system of a; apply the
  // transfrom-from-e-to-c and transform-from-c-to-b then we get 3 in the
  // coordinate system of b.
  Target FindTargetForLocation(EventSource event_source,
                               const gfx::PointF& location_in_root) const;

  // Same as FindTargetForLocation(), but starts from |frame_sink_id|.
  // |location| is in the coordinate space of |frame_sink_id|. Returns an empty
  // target if |frame_sink_id| is not found.
  Target FindTargetForLocationStartingFrom(
      EventSource event_source,
      const gfx::PointF& location,
      const FrameSinkId& frame_sink_id) const;

  // When a target window is already known, e.g. capture/latched window, convert
  // |location_in_root| to be in the coordinate space of the target and store
  // that in |transformed_location|. Return true if the transform is successful
  // and false otherwise.
  // |target_ancestors| contains the FrameSinkId from target to root.
  // |target_ancestors.front()| is the target, and |target_ancestors.back()|
  // is the root.
  bool TransformLocationForTarget(
      const std::vector<FrameSinkId>& target_ancestors,
      const gfx::PointF& location_in_root,
      gfx::PointF* transformed_location) const;

  // Gets the transform from root to |target| in physical pixels. Returns true
  // and stores the result into |transform| if successful, returns false
  // otherwise. This is potentially a little more expensive than
  // TransformLocationForTarget(). So if the path from root to target is known,
  // then that is the preferred API.
  bool GetTransformToTarget(const FrameSinkId& target,
                            gfx::Transform* transform) const;

  // Returns whether client has submitted hit test data for |frame_sink_id|.
  // Note that this returns false even if the embedder has submitted hit-test
  // data for |frame_sink_id|.
  bool ContainsActiveFrameSinkId(const FrameSinkId& frame_sink_id) const;

  // Returns hit-test data, using indentation to visualize the tree structure.
  std::string PrintHitTestData() const;
  const std::vector<AggregatedHitTestRegion>& GetHitTestData() const {
    return hit_test_data_;
  }

 protected:
  // The FindTargetForLocation() functions call into this.
  // If |is_location_relative_to_parent| is true, |location| is relative to
  // the parent, otherwise it is in the coordinate space of |frame_sink_id|.
  // Virtual for testing.
  virtual Target FindTargetForLocationStartingFromImpl(
      EventSource event_source,
      const gfx::PointF& location,
      const FrameSinkId& frame_sink_id,
      bool is_location_relative_to_parent) const;

 private:
  // Helper function to find |target| for |location| in the |region_index|,
  // returns true if a target is found and false otherwise. If
  // |is_location_relative_to_parent| is true, |location| is in the coordinate
  // space of |region_index|'s parent, otherwise it is in the coordinate space
  // of |region_index|.
  bool FindTargetInRegionForLocation(EventSource event_source,
                                     const gfx::PointF& location,
                                     size_t region_index,
                                     bool is_location_relative_to_parent,
                                     const FrameSinkId& root_view_frame_sink_id,
                                     Target* target) const;

  // Transform |location_in_target| to be in |region_index|'s coordinate space.
  // |location_in_target| is in the coordinate space of |region_index|'s parent
  // at the beginning.
  bool TransformLocationForTargetRecursively(
      const std::vector<FrameSinkId>& target_ancestors,
      size_t target_ancestor,
      size_t region_index,
      gfx::PointF* location_in_target) const;

  bool GetTransformToTargetRecursively(const FrameSinkId& target,
                                       size_t region_index,
                                       gfx::Transform* transform) const;

  void RecordSlowPathHitTestReasons(uint32_t) const;

  // Returns true if |id| is present in |hit_test_data|. If |id| is present
  // |index| is set accordingly.
  bool FindIndexOfFrameSink(const FrameSinkId& id, size_t* index) const;

  std::vector<AggregatedHitTestRegion> hit_test_data_;

  DISALLOW_COPY_AND_ASSIGN(HitTestQuery);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_
