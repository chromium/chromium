// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/test_support/mock_transport_channel.h"

namespace stream_transport {

MockTransportChannel::MockTransportChannel() = default;
MockTransportChannel::~MockTransportChannel() = default;

base::WeakPtr<MockTransportChannel> MockTransportChannel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace stream_transport
