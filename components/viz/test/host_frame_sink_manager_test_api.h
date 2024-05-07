// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_
#define COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_

#include "base/memory/raw_ptr.h"
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

  HostFrameSinkManagerTestApi(const HostFrameSinkManagerTestApi&) = delete;
  HostFrameSinkManagerTestApi& operator=(const HostFrameSinkManagerTestApi&) =
      delete;

  ~HostFrameSinkManagerTestApi() = default;

  // Clears out the currently set hit test queries, and overrides it with |map|.
  // The HostFrameSinkManager will take ownership of |map|. There should only be
  // one HitTestQuery per root FrameSinkId.
  void SetDisplayHitTestQuery(DisplayHitTestQueryMap map);

 private:
  // Not owned.
  raw_ptr<HostFrameSinkManager, DanglingUntriaged> host_frame_sink_manager_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_HOST_FRAME_SINK_MANAGER_TEST_API_H_
