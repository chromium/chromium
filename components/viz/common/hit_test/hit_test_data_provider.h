// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/common/hit_test/hit_test_region_observer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace viz {
using DisplayHitTestQueryMap =
    base::flat_map<FrameSinkId, std::unique_ptr<HitTestQuery>>;

// This interface will be implemented by both HostFrameSinkManager and
// FrameSinkManagerImpl allowing RenderWidgetHostInputEventRouter to find target
// view synchronously.
class VIZ_COMMON_EXPORT HitTestDataProvider {
 public:
  virtual ~HitTestDataProvider() = default;
  // Add/Remove an observer to receive notifications of when the host receives
  // new hit test data.
  virtual void AddHitTestRegionObserver(HitTestRegionObserver* observer) = 0;
  virtual void RemoveHitTestRegionObserver(HitTestRegionObserver* observer) = 0;
  virtual const DisplayHitTestQueryMap& GetDisplayHitTestQuery() const = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_DATA_PROVIDER_H_
