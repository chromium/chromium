// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"

namespace collaboration::messaging {

MessageCategory GetMessageCategory(const collaboration_pb::Message& message) {
  switch (message.event_type()) {
    case collaboration_pb::TAB_ADDED:
    case collaboration_pb::TAB_REMOVED:
    case collaboration_pb::TAB_UPDATED:
      return MessageCategory::kTab;
    case collaboration_pb::TAB_GROUP_ADDED:
    case collaboration_pb::TAB_GROUP_REMOVED:
    case collaboration_pb::TAB_GROUP_NAME_UPDATED:
    case collaboration_pb::TAB_GROUP_COLOR_UPDATED:
      return MessageCategory::kTabGroup;
    case collaboration_pb::COLLABORATION_ADDED:
    case collaboration_pb::COLLABORATION_REMOVED:
    case collaboration_pb::COLLABORATION_MEMBER_ADDED:
    case collaboration_pb::COLLABORATION_MEMBER_REMOVED:
      return MessageCategory::kCollaboration;
    default:
      return MessageCategory::kUnknown;
  }
}

}  // namespace collaboration::messaging
