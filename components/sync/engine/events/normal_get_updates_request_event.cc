// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/normal_get_updates_request_event.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

NormalGetUpdatesRequestEvent::NormalGetUpdatesRequestEvent(
    base::Time timestamp,
    const NudgeTracker& nudge_tracker,
    const sync_pb::ClientToServerMessage& request)
    : NormalGetUpdatesRequestEvent(timestamp,
                                   nudge_tracker.GetNudgedTypes(),
                                   nudge_tracker.GetNotifiedTypes(),
                                   nudge_tracker.GetRefreshRequestedTypes(),
                                   nudge_tracker.IsRetryRequired(),
                                   request) {}

NormalGetUpdatesRequestEvent::NormalGetUpdatesRequestEvent(
    base::Time timestamp,
    DataTypeSet nudged_types,
    DataTypeSet notified_types,
    DataTypeSet refresh_requested_types,
    bool is_retry,
    sync_pb::ClientToServerMessage request)
    : timestamp_(timestamp),
      nudged_types_(nudged_types),
      notified_types_(notified_types),
      refresh_requested_types_(refresh_requested_types),
      is_retry_(is_retry),
      request_(request) {}

std::unique_ptr<ProtocolEvent> NormalGetUpdatesRequestEvent::Clone() const {
  return std::make_unique<NormalGetUpdatesRequestEvent>(
      timestamp_, nudged_types_, notified_types_, refresh_requested_types_,
      is_retry_, request_);
}

NormalGetUpdatesRequestEvent::~NormalGetUpdatesRequestEvent() = default;

base::Time NormalGetUpdatesRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string NormalGetUpdatesRequestEvent::GetType() const {
  return "Normal GetUpdate request";
}

std::string NormalGetUpdatesRequestEvent::GetDetails() const {
  std::string details;

  if (!nudged_types_.empty()) {
    if (!details.empty()) {
      details.append("\n");
    }
    details.append(base::StringPrintf(
        "Nudged types: %s", DataTypeSetToDebugString(nudged_types_).c_str()));
  }

  if (!notified_types_.empty()) {
    if (!details.empty()) {
      details.append("\n");
    }
    details.append(
        base::StringPrintf("Notified types: %s",
                           DataTypeSetToDebugString(notified_types_).c_str()));
  }

  if (!refresh_requested_types_.empty()) {
    if (!details.empty()) {
      details.append("\n");
    }
    details.append(base::StringPrintf(
        "Refresh requested types: %s",
        DataTypeSetToDebugString(refresh_requested_types_).c_str()));
  }

  if (is_retry_) {
    if (!details.empty()) {
      details.append("\n");
    }
    details.append(base::StringPrintf("Is retry: True"));
  }

  return details;
}

base::Value::Dict NormalGetUpdatesRequestEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerMessageToValue(
             request_, {.include_specifics = include_specifics,
                        .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
