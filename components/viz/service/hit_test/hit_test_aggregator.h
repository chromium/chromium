// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/hit_test/hit_test_manager.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class HitTestAggregatorDelegate;
struct HitTestRegion;

// HitTestAggregator assembles the list of HitTestRegion objects that define the
// hit test information required for one display. Active HitTestRegionList
// information is obtained from the HitTestManager. The resulting list is sent
// to HitTestQuery for event targeting. This is intended to be created in the
// viz or GPU process.
class VIZ_SERVICE_EXPORT HitTestAggregator {
 public:
  // |delegate| owns and outlives HitTestAggregator.
  HitTestAggregator(
      const HitTestManager* hit_test_manager,
      HitTestAggregatorDelegate* delegate,
      LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate,
      const FrameSinkId& frame_sink_id,
      uint32_t initial_region_size = 100,
      uint32_t max_region_size = 100 * 100);
  ~HitTestAggregator();

  // Called after surfaces have been aggregated into the DisplayFrame.
  // In this call HitTestRegionList structures received from active surfaces
  // are aggregated into |hit_test_data_|. If |render_passes| are given and
  // the correct flags are set, hit-test debug quads will be inserted.
  void Aggregate(const SurfaceId& display_surface_id,
                 RenderPassList* render_passes = nullptr);

 private:
  friend class TestHitTestAggregator;

  void SendHitTestData();

  // Appends the root element to the AggregatedHitTestRegion array.
  void AppendRoot(const SurfaceId& surface_id);

  // Appends a |region| to the HitTestRegionList structure to recursively
  // build the tree. |region_index| indicates the current index of the end of
  // the list.
  size_t AppendRegion(size_t region_index, const HitTestRegion& region);

  // Populates the HitTestRegion element at the given element |index|.
  void SetRegionAt(size_t index,
                   const FrameSinkId& frame_sink_id,
                   uint32_t flags,
                   uint32_t reasons,
                   const gfx::Rect& rect,
                   const gfx::Transform& transform,
                   int32_t child_count);

  // Returns the |trace_id| of the |begin_frame_ack| in the active frame for
  // the given |surface_id| if it is different than when it was last queried.
  // This is used in order to ensure that the flow between receiving hit-test
  // data and aggregating is included only once per submission.
  base::Optional<int64_t> GetTraceIdIfUpdated(const SurfaceId& surface_id,
                                              uint64_t active_frame_index);

  // Inserts debug quads based on hit-test data.
  void InsertHitTestDebugQuads(RenderPassList* render_passes);

  const HitTestManager* const hit_test_manager_;

  HitTestAggregatorDelegate* const delegate_;

  LatestLocalSurfaceIdLookupDelegate* const local_surface_id_lookup_delegate_;

  // This is the FrameSinkId for the corresponding root CompositorFrameSink.
  const FrameSinkId root_frame_sink_id_;

  // Initial hit-test region size.
  // TODO(https://crbug.com/746385): Review and select appropriate sizes based
  // on telemetry / UMA.
  const uint32_t initial_region_size_;
  const uint32_t incremental_region_size_;
  const uint32_t max_region_size_;

  uint32_t hit_test_data_capacity_ = 0;
  uint32_t hit_test_data_size_ = 0;
  std::vector<AggregatedHitTestRegion> hit_test_data_;

  bool hit_test_debug_ = false;
  uint32_t hit_test_debug_ask_regions_ = 0;

  // This is the set of FrameSinkIds referenced in the aggregation in this tree
  // chain so far, used to detect cycles. We can have regions that have the
  // same FrameSinkId, e.g. when ALPHA_SHAPE is set in cc::FilterOperations,
  // but only at the same hierarchy level.
  base::flat_set<FrameSinkId> referenced_child_regions_;

  base::flat_map<FrameSinkId, uint64_t> last_active_frame_index_;
  uint64_t last_submit_hit_test_region_list_index_ = 0;

  // Handles the case when this object is deleted after
  // the PostTaskAggregation call is scheduled but before invocation.
  base::WeakPtrFactory<HitTestAggregator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HitTestAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_
