// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/commit_response_event.h"

#include "base/values.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitResponseEvent::CommitResponseEvent(
    base::Time timestamp,
    SyncerError result,
    const sync_pb::ClientToServerResponse& response)
    : timestamp_(timestamp), result_(result), response_(response) {}

CommitResponseEvent::~CommitResponseEvent() = default;

std::unique_ptr<ProtocolEvent> CommitResponseEvent::Clone() const {
  return std::make_unique<CommitResponseEvent>(timestamp_, result_, response_);
}

base::Time CommitResponseEvent::GetTimestamp() const {
  return timestamp_;
}

std::string CommitResponseEvent::GetType() const {
  return "Commit Response";
}

std::string CommitResponseEvent::GetDetails() const {
  return "Result: " + result_.ToString();
}

base::Value::Dict CommitResponseEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerResponseToValue(
             response_, {.include_specifics = include_specifics,
                         .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
