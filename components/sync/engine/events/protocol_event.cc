// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event.h"

namespace syncer {

ProtocolEvent::ProtocolEvent() = default;

ProtocolEvent::~ProtocolEvent() = default;

base::Value::Dict ProtocolEvent::ToValue(bool include_specifics) const {
  base::Value::Dict dict;
  dict.Set("time", GetTimestamp().ToJsTime());
  dict.Set("type", GetType());
  dict.Set("details", GetDetails());
  dict.Set("proto", GetProtoMessage(include_specifics));
  return dict;
}

base::Time ProtocolEvent::GetTimestampForTesting() const {
  return GetTimestamp();
}

}  // namespace syncer
