// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/messaging_backend_service_impl.h"

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/saved_tab_groups/messaging/message.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups::messaging {

MessagingBackendServiceImpl::MessagingBackendServiceImpl(
    TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service)
    : tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service) {}

MessagingBackendServiceImpl::~MessagingBackendServiceImpl() = default;

void MessagingBackendServiceImpl::SetInstantMessageDelegate(
    InstantMessageDelegate* instant_message_delegate) {
  instant_message_delegate_ = instant_message_delegate;
}

void MessagingBackendServiceImpl::AddPersistentMessageObserver(
    PersistentMessageObserver* observer) {
  persistent_message_observers_.AddObserver(observer);
  // TODO(345856704): Implement this and inform the observer if we have already
  // initialized.
}

void MessagingBackendServiceImpl::RemovePersistentMessageObserver(
    PersistentMessageObserver* observer) {
  persistent_message_observers_.RemoveObserver(observer);
}

bool MessagingBackendServiceImpl::IsInitialized() {
  return false;
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessagesForTab(
    EitherTabID tab_id,
    std::optional<PersistentNotificationType> type) {
  // TODO(345856704): Implement this and DCHECK(IsInitialized()) and update
  // interface description.
  return {};
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessagesForGroup(
    EitherGroupID group_id,
    std::optional<PersistentNotificationType> type) {
  // TODO(345856704): Implement this and DCHECK(IsInitialized()) and update
  // interface description.
  return {};
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessages(
    std::optional<PersistentNotificationType> type) {
  // TODO(345856704): Implement this and DCHECK(IsInitialized()) and update
  // interface description.
  return {};
}

std::vector<ActivityLogItem> MessagingBackendServiceImpl::GetActivityLog(
    const ActivityLogQueryParams& params) {
  // TODO(345856704): Implement this and DCHECK(IsInitialized()) and update
  // interface description.
  return std::vector<ActivityLogItem>();
}

}  // namespace tab_groups::messaging
