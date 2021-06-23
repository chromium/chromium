// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_
#define COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_

#include "base/macros.h"
#include "components/viz/host/host_frame_sink_manager.h"

namespace viz {

// A test wrapper for HostFrameSinkManager. For use in unit testing, where the
// Viz Host never instanciates Viz. Without Viz, HostFrameSinkManager can never
// actually establish connections to compositor frame sinks.
//
// This allows for the explicit overriding of the hit test data with the
// HostFrameSinkManager. So that unit tests can test event routing without Viz
// providing the hit test data.
class HostFrameSinkManagerTestApi {
 public:
  explicit HostFrameSinkManagerTestApi(
      HostFrameSinkManager* host_frame_sink_manager);
  ~HostFrameSinkManagerTestApi() = default;

  // Clears out the currently set hit test queries, and overrides it with |map|.
  // The HostFrameSinkManager will take ownership of |map|. There should only be
  // one HitTestQuery per root FrameSinkId.
  void SetDisplayHitTestQuery(HostFrameSinkManager::DisplayHitTestQueryMap map);

 private:
  // Not owned.
  HostFrameSinkManager* host_frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerTestApi);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_
