// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event.h"

namespace syncer {

ProtocolEvent::ProtocolEvent() = default;

ProtocolEvent::~ProtocolEvent() = default;

std::unique_ptr<base::DictionaryValue> ProtocolEvent::ToValue(
    bool include_specifics) const {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetDoubleKey("time", GetTimestamp().ToJsTime());
  dict->SetStringKey("type", GetType());
  dict->SetStringKey("details", GetDetails());
  dict->SetKey("proto", base::Value::FromUniquePtrValue(
                            GetProtoMessage(include_specifics)));
  return dict;
}

base::Time ProtocolEvent::GetTimestampForTesting() const {
  return GetTimestamp();
}

}  // namespace syncer
