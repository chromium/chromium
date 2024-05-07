// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/host_frame_sink_manager_test_api.h"

namespace viz {

HostFrameSinkManagerTestApi::HostFrameSinkManagerTestApi(
    HostFrameSinkManager* host_frame_sink_manager)
    : host_frame_sink_manager_(host_frame_sink_manager) {}

void HostFrameSinkManagerTestApi::SetDisplayHitTestQuery(
    DisplayHitTestQueryMap map) {
  host_frame_sink_manager_->display_hit_test_query_.clear();
  host_frame_sink_manager_->display_hit_test_query_ = std::move(map);
}

}  // namespace viz
