// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_OBSERVER_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace viz {

// An observer to be used within a Viz Host. Allows for notifications of when
// updated hit test regions have been provided. See HostFrameSinkManager for
// usage.
class HitTestRegionObserver : public base::CheckedObserver {
 public:
  HitTestRegionObserver() = default;

  HitTestRegionObserver(const HitTestRegionObserver&) = delete;
  HitTestRegionObserver& operator=(const HitTestRegionObserver&) = delete;

  ~HitTestRegionObserver() override = default;

  virtual void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_OBSERVER_H_
