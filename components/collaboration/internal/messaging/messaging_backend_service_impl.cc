// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <sys/types.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/saved_tab_groups/public/types.h"

namespace collaboration::messaging {

MessagingBackendServiceImpl::MessagingBackendServiceImpl(
    std::unique_ptr<TabGroupChangeNotifier> tab_group_change_notifier,
    std::unique_ptr<DataSharingChangeNotifier> data_sharing_change_notifier,
    std::unique_ptr<MessagingBackendStore> messaging_backend_store,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service)
    : tab_group_change_notifier_(std::move(tab_group_change_notifier)),
      data_sharing_change_notifier_(std::move(data_sharing_change_notifier)),
      store_(std::move(messaging_backend_store)),
      tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service) {
  tab_group_change_notifier_observer_.Observe(tab_group_change_notifier_.get());
  tab_group_change_notifier_->Initialize();
  data_sharing_change_notifier_observer_.Observe(
      data_sharing_change_notifier_.get());
  data_sharing_change_notifier_->Initialize();
}

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
  return tab_group_change_notifier_initialized_ &&
         data_sharing_change_notifier_initialized_;
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessagesForTab(
    tab_groups::EitherTabID tab_id,
    std::optional<PersistentNotificationType> type) {
  // TODO(345856704): Implement this and DCHECK(IsInitialized()) and update
  // interface description.
  return {};
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessagesForGroup(
    tab_groups::EitherGroupID group_id,
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

void MessagingBackendServiceImpl::OnTabGroupChangeNotifierInitialized() {
  tab_group_change_notifier_initialized_ = true;
}

void MessagingBackendServiceImpl::OnDataSharingChangeNotifierInitialized() {
  data_sharing_change_notifier_initialized_ = true;
}

void MessagingBackendServiceImpl::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& added_group) {}

void MessagingBackendServiceImpl::OnTabGroupRemoved(
    tab_groups::SavedTabGroup removed_group) {}

void MessagingBackendServiceImpl::OnTabGroupNameUpdated(
    const tab_groups::SavedTabGroup& updated_group) {}

void MessagingBackendServiceImpl::OnTabGroupColorUpdated(
    const tab_groups::SavedTabGroup& updated_group) {}

void MessagingBackendServiceImpl::OnTabAdded(
    const tab_groups::SavedTabGroupTab& added_tab) {}

void MessagingBackendServiceImpl::OnTabRemoved(
    tab_groups::SavedTabGroupTab removed_tab) {}

void MessagingBackendServiceImpl::OnTabUpdated(
    const tab_groups::SavedTabGroupTab& updated_tab) {}

void MessagingBackendServiceImpl::OnTabSelected(
    std::optional<tab_groups::SavedTabGroupTab> selected_tab) {}

}  // namespace collaboration::messaging
