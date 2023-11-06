// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event.h"

namespace syncer {

ProtocolEvent::ProtocolEvent() = default;

ProtocolEvent::~ProtocolEvent() = default;

base::Value::Dict ProtocolEvent::ToValue(bool include_specifics) const {
  return base::Value::Dict()
      .Set("time", GetTimestamp().InMillisecondsFSinceUnixEpoch())
      .Set("type", GetType())
      .Set("details", GetDetails())
      .Set("proto", GetProtoMessage(include_specifics));
}

base::Time ProtocolEvent::GetTimestampForTesting() const {
  return GetTimestamp();
}

}  // namespace syncer
