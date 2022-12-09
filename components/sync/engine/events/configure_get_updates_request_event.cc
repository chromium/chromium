// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/configure_get_updates_request_event.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

ConfigureGetUpdatesRequestEvent::ConfigureGetUpdatesRequestEvent(
    base::Time timestamp,
    sync_pb::SyncEnums::GetUpdatesOrigin origin,
    const sync_pb::ClientToServerMessage& request)
    : timestamp_(timestamp), origin_(origin), request_(request) {}

ConfigureGetUpdatesRequestEvent::~ConfigureGetUpdatesRequestEvent() = default;

std::unique_ptr<ProtocolEvent> ConfigureGetUpdatesRequestEvent::Clone() const {
  return std::make_unique<ConfigureGetUpdatesRequestEvent>(timestamp_, origin_,
                                                           request_);
}

base::Time ConfigureGetUpdatesRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string ConfigureGetUpdatesRequestEvent::GetType() const {
  return "Initial GetUpdates";
}

std::string ConfigureGetUpdatesRequestEvent::GetDetails() const {
  return base::StringPrintf("Reason: %s", ProtoEnumToString(origin_));
}

base::Value::Dict ConfigureGetUpdatesRequestEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerMessageToValue(
             request_, {.include_specifics = include_specifics,
                        .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
