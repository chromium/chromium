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
namespace {
collaboration_pb::Message CreateMessage(
    const data_sharing::GroupId& collaboration_group_id,
    collaboration_pb::EventType event_type,
    DirtyType dirty_type,
    const base::Time& event_time) {
  collaboration_pb::Message message;
  message.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  message.set_collaboration_id(collaboration_group_id.value());
  message.set_event_type(event_type);
  message.set_dirty(static_cast<int>(dirty_type));
  message.set_event_timestamp(event_time.ToTimeT());
  return message;
}

}  // namespace

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
  store_->Initialize(
      base::BindOnce(&MessagingBackendServiceImpl::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
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
  return initialized_;
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

void MessagingBackendServiceImpl::OnStoreInitialized(bool success) {
  if (!success) {
    DVLOG(2) << "Failed to initialize MessagingBackendServiceImpl.";
    return;
  }
  data_sharing_change_notifier_observer_.Observe(
      data_sharing_change_notifier_.get());
  data_sharing_flush_callback_ = data_sharing_change_notifier_->Initialize();
}

void MessagingBackendServiceImpl::OnDataSharingChangeNotifierInitialized() {
  tab_group_change_notifier_observer_.Observe(tab_group_change_notifier_.get());
  tab_group_change_notifier_->Initialize();
}

void MessagingBackendServiceImpl::OnTabGroupChangeNotifierInitialized() {
  initialized_ = true;
  for (auto& observer : persistent_message_observers_) {
    observer.OnMessagingBackendServiceInitialized();
  }
  CHECK(data_sharing_flush_callback_);
  std::move(data_sharing_flush_callback_).Run();
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

void MessagingBackendServiceImpl::OnGroupAdded(
    const data_sharing::GroupId& group_id,
    const std::optional<data_sharing::GroupData>& group_data,
    const base::Time& event_time) {
  collaboration_pb::Message message =
      CreateMessage(group_id, collaboration_pb::COLLABORATION_ADDED,
                    DirtyType::kNone, event_time);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnGroupRemoved(
    const data_sharing::GroupId& group_id,
    const std::optional<data_sharing::GroupData>& group_data,
    const base::Time& event_time) {
  collaboration_pb::Message message =
      CreateMessage(group_id, collaboration_pb::COLLABORATION_REMOVED,
                    DirtyType::kMessageOnly, event_time);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnGroupMemberAdded(
    const data_sharing::GroupData& group_data,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  collaboration_pb::Message message =
      CreateMessage(group_data.group_token.group_id,
                    collaboration_pb::COLLABORATION_MEMBER_ADDED,
                    DirtyType::kMessageOnly, event_time);
  message.set_affected_user_gaia_id(member_gaia_id.ToString());
  std::optional<std::string> user_display_name =
      GetDisplayNameForUserInGroup(group_data.group_token.group_id,
                                   member_gaia_id, group_data, std::nullopt);
  if (user_display_name) {
    message.mutable_collaboration_data()->set_affected_user_name(
        *user_display_name);
  }
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnGroupMemberRemoved(
    const data_sharing::GroupData& group_data,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  collaboration_pb::Message message =
      CreateMessage(group_data.group_token.group_id,
                    collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                    DirtyType::kNone, event_time);
  message.set_affected_user_gaia_id(member_gaia_id.ToString());
  std::optional<std::string> user_display_name =
      GetDisplayNameForUserInGroup(group_data.group_token.group_id,
                                   member_gaia_id, group_data, std::nullopt);
  if (user_display_name) {
    message.mutable_collaboration_data()->set_affected_user_name(
        *user_display_name);
  }
  store_->AddMessage(message);
}

std::optional<std::string>
MessagingBackendServiceImpl::GetDisplayNameForUserInGroup(
    const data_sharing::GroupId& group_id,
    const GaiaId& gaia_id,
    const std::optional<data_sharing::GroupData>& group_data,
    const std::optional<collaboration_pb::Message>& db_message) {
  std::optional<data_sharing::GroupMemberPartialData> group_member_data =
      data_sharing_service_->GetPossiblyRemovedGroupMember(group_id, gaia_id);
  // Try given name from live data first.
  if (group_member_data && !group_member_data->given_name.empty()) {
    return group_member_data->given_name;
  }

  // Then try given name from provided data.
  if (group_data) {
    for (const data_sharing::GroupMember& member : group_data->members) {
      if (member.gaia_id == gaia_id) {
        if (member.given_name.empty()) {
          break;
        }
        return member.given_name;
      }
    }
  }

  // Then try given name from stored data.
  if (db_message) {
    if (db_message->affected_user_gaia_id() == gaia_id.ToString()) {
      if (!db_message->collaboration_data().affected_user_name().empty()) {
        return db_message->collaboration_data().affected_user_name();
      }
    }
  }

  // Then try display name from live data.
  if (group_member_data && !group_member_data->display_name.empty()) {
    return group_member_data->display_name;
  }

  // Then try display name from provided data.
  if (group_data) {
    for (const data_sharing::GroupMember& member : group_data->members) {
      if (member.gaia_id == gaia_id) {
        if (member.display_name.empty()) {
          break;
        }
        return member.display_name;
      }
    }
  }

  return std::nullopt;
}

}  // namespace collaboration::messaging
