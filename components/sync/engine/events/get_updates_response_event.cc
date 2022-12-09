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
  switch (error_.value()) {
    case SyncerError::SYNCER_OK:
      return base::StringPrintf("Received %d update(s).",
                                response_.get_updates().entries_size());
    case SyncerError::SERVER_MORE_TO_DOWNLOAD:
      return base::StringPrintf("Received %d update(s).  Some updates remain.",
                                response_.get_updates().entries_size());
    case SyncerError::UNSET:
    case SyncerError::NETWORK_CONNECTION_UNAVAILABLE:
    case SyncerError::NETWORK_IO_ERROR:
    case SyncerError::SYNC_SERVER_ERROR:
    case SyncerError::SYNC_AUTH_ERROR:
    case SyncerError::SERVER_RETURN_UNKNOWN_ERROR:
    case SyncerError::SERVER_RETURN_THROTTLED:
    case SyncerError::SERVER_RETURN_TRANSIENT_ERROR:
    case SyncerError::SERVER_RETURN_MIGRATION_DONE:
    case SyncerError::SERVER_RETURN_CLEAR_PENDING:
    case SyncerError::SERVER_RETURN_NOT_MY_BIRTHDAY:
    case SyncerError::SERVER_RETURN_CONFLICT:
    case SyncerError::SERVER_RESPONSE_VALIDATION_FAILED:
    case SyncerError::SERVER_RETURN_DISABLED_BY_ADMIN:
    case SyncerError::SERVER_RETURN_CLIENT_DATA_OBSOLETE:
    case SyncerError::SERVER_RETURN_ENCRYPTION_OBSOLETE:
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
