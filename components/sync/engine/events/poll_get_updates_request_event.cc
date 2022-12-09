// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/poll_get_updates_request_event.h"

#include "base/values.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

PollGetUpdatesRequestEvent::PollGetUpdatesRequestEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerMessage& request)
    : timestamp_(timestamp), request_(request) {}

PollGetUpdatesRequestEvent::~PollGetUpdatesRequestEvent() = default;

std::unique_ptr<ProtocolEvent> PollGetUpdatesRequestEvent::Clone() const {
  return std::make_unique<PollGetUpdatesRequestEvent>(timestamp_, request_);
}

base::Time PollGetUpdatesRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string PollGetUpdatesRequestEvent::GetType() const {
  return "Poll GetUpdate request";
}

std::string PollGetUpdatesRequestEvent::GetDetails() const {
  return std::string();
}

base::Value::Dict PollGetUpdatesRequestEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerMessageToValue(
             request_, {.include_specifics = include_specifics,
                        .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
