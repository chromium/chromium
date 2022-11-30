// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_host_frame_sink_client.h"

#include "base/time/time.h"

namespace viz {

FakeHostFrameSinkClient::FakeHostFrameSinkClient() = default;

FakeHostFrameSinkClient::~FakeHostFrameSinkClient() = default;

void FakeHostFrameSinkClient::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  last_frame_token_seen_ = frame_token;
}

}  // namespace viz
