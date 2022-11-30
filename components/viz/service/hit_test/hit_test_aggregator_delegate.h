// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_
#define COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"

namespace viz {
// Used by HitTestAggregator to talk to FrameSinkManagerImpl.
class HitTestAggregatorDelegate {
 public:
  // Called to send |hit_test_data| when we receive new data.
  virtual void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) = 0;

 protected:
  // The dtor is protected so that HitTestAggregator does not take ownership.
  virtual ~HitTestAggregatorDelegate() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_
