// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/messaging/empty_messaging_backend_service.h"

#include <vector>

#include "components/collaboration/public/messaging/message.h"
#include "components/saved_tab_groups/public/types.h"

namespace collaboration::messaging {

EmptyMessagingBackendService::EmptyMessagingBackendService() = default;

EmptyMessagingBackendService::~EmptyMessagingBackendService() = default;

void EmptyMessagingBackendService::SetInstantMessageDelegate(
    InstantMessageDelegate* instant_message_delegate) {}

void EmptyMessagingBackendService::AddPersistentMessageObserver(
    PersistentMessageObserver* observer) {}

void EmptyMessagingBackendService::RemovePersistentMessageObserver(
    PersistentMessageObserver* observer) {}

bool EmptyMessagingBackendService::IsInitialized() {
  return false;
}

std::vector<PersistentMessage> EmptyMessagingBackendService::GetMessagesForTab(
    tab_groups::EitherTabID tab_id,
    PersistentNotificationType type) {
  return std::vector<PersistentMessage>();
}

std::vector<PersistentMessage>
EmptyMessagingBackendService::GetMessagesForGroup(
    tab_groups::EitherGroupID group_id,
    PersistentNotificationType type) {
  return std::vector<PersistentMessage>();
}

std::vector<PersistentMessage> EmptyMessagingBackendService::GetMessages(
    PersistentNotificationType type) {
  return std::vector<PersistentMessage>();
}

std::vector<ActivityLogItem> EmptyMessagingBackendService::GetActivityLog(
    const ActivityLogQueryParams& params) {
  return std::vector<ActivityLogItem>();
}

void EmptyMessagingBackendService::ClearDirtyTabMessagesForGroup(
    const data_sharing::GroupId& collaboration_group_id) {}

void EmptyMessagingBackendService::ClearPersistentMessage(
    const base::Uuid& message_id,
    PersistentNotificationType type) {}

void EmptyMessagingBackendService::RemoveMessages(
    const std::vector<base::Uuid>& message_ids) {}

void EmptyMessagingBackendService::AddActivityLogForTesting(
    data_sharing::GroupId collaboration_id,
    const std::vector<ActivityLogItem>& activity_log) {}

}  // namespace collaboration::messaging
