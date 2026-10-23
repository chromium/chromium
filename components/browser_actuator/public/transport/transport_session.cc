// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/transport/transport_session.h"

namespace browser_actuator::transport {

void TransportSession::OnMessage(PayloadType payload_type,
                                 std::string_view serialized_payload) {}

}  // namespace browser_actuator::transport
