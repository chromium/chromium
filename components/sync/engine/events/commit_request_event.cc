// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/commit_request_event.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitRequestEvent::CommitRequestEvent(
    base::Time timestamp,
    size_t num_items,
    DataTypeSet contributing_types,
    const sync_pb::ClientToServerMessage& request)
    : timestamp_(timestamp),
      num_items_(num_items),
      contributing_types_(contributing_types),
      request_(request) {}

CommitRequestEvent::~CommitRequestEvent() = default;

std::unique_ptr<ProtocolEvent> CommitRequestEvent::Clone() const {
  return std::make_unique<CommitRequestEvent>(timestamp_, num_items_,
                                              contributing_types_, request_);
}

base::Time CommitRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string CommitRequestEvent::GetType() const {
  return "Commit Request";
}

std::string CommitRequestEvent::GetDetails() const {
  return base::StringPrintf(
      "Item count: %" PRIuS
      "\n"
      "Contributing types: %s",
      num_items_, DataTypeSetToDebugString(contributing_types_).c_str());
}

base::Value::Dict CommitRequestEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerMessageToValue(
             request_, {.include_specifics = include_specifics,
                        .include_full_get_update_triggers = false})
      .TakeDict();
}

}  // namespace syncer
