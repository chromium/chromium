// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/get_updates_response_event.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

GetUpdatesResponseEvent::GetUpdatesResponseEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerResponse& response,
    SyncerError error)
    : timestamp_(timestamp), response_(response), error_(error) {}

GetUpdatesResponseEvent::~GetUpdatesResponseEvent() = default;

std::unique_ptr<ProtocolEvent> GetUpdatesResponseEvent::Clone() const {
  return std::make_unique<GetUpdatesResponseEvent>(timestamp_, response_,
                                                   error_);
}

base::Time GetUpdatesResponseEvent::GetTimestamp() const {
  return timestamp_;
}

std::string GetUpdatesResponseEvent::GetType() const {
  return "GetUpdates Response";
}

std::string GetUpdatesResponseEvent::GetDetails() const {
  switch (error_.type()) {
    case SyncerError::Type::kSuccess: {
      std::string details = base::StringPrintf(
          "Received %d update(s).", response_.get_updates().entries_size());
      if (response_.get_updates().changes_remaining() != 0) {
        details += " Some updates remain.";
      }
      return details;
    }
    case SyncerError::Type::kNetworkError:
    case SyncerError::Type::kHttpError:
    case SyncerError::Type::kProtocolError:
    case SyncerError::Type::kProtocolViolationError:
      return "Received error: " + error_.ToString();
  }
}

base::Value::Dict GetUpdatesResponseEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerResponseToValue(
             response_, {.include_specifics = include_specifics,
                         .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
