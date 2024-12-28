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
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/url_formatter/elide_url.h"

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

collaboration_pb::Message CreateTabGroupMessage(
    data_sharing::GroupId collaboration_group_id,
    const tab_groups::SavedTabGroup& tab_group,
    collaboration_pb::EventType event_type,
    DirtyType dirty_type) {
  collaboration_pb::Message message =
      CreateMessage(collaboration_group_id, event_type, dirty_type,
                    tab_group.update_time_windows_epoch_micros());
  message.mutable_tab_group_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  switch (event_type) {
    case collaboration_pb::TAB_GROUP_ADDED:
      message.set_triggering_user_gaia_id(
          tab_group.shared_attribution().created_by.ToString());
      break;
    case collaboration_pb::TAB_GROUP_REMOVED:
    case collaboration_pb::TAB_GROUP_NAME_UPDATED:
    case collaboration_pb::TAB_GROUP_COLOR_UPDATED:
      message.set_triggering_user_gaia_id(
          tab_group.shared_attribution().updated_by.ToString());
      break;
    default:
      break;
  }
  return message;
}

collaboration_pb::Message CreateTabMessage(
    data_sharing::GroupId collaboration_group_id,
    const tab_groups::SavedTabGroupTab& tab,
    collaboration_pb::EventType event_type,
    DirtyType dirty_type) {
  collaboration_pb::Message message =
      CreateMessage(collaboration_group_id, event_type, dirty_type,
                    event_type == collaboration_pb::TAB_ADDED
                        ? tab.creation_time_windows_epoch_micros()
                        : tab.update_time_windows_epoch_micros());
  message.mutable_tab_data()->set_sync_tab_id(
      tab.saved_tab_guid().AsLowercaseString());
  message.mutable_tab_data()->set_sync_tab_group_id(
      tab.saved_group_guid().AsLowercaseString());
  message.mutable_tab_data()->set_last_url(tab.url().spec());
  switch (event_type) {
    case collaboration_pb::TAB_ADDED:
      message.set_triggering_user_gaia_id(
          tab.shared_attribution().created_by.ToString());
      break;
    case collaboration_pb::TAB_UPDATED:
    case collaboration_pb::TAB_REMOVED:
      message.set_triggering_user_gaia_id(
          tab.shared_attribution().updated_by.ToString());
      break;
    default:
      break;
  }
  return message;
}

CollaborationEvent ToCollaborationEvent(
    collaboration_pb::EventType event_type) {
  switch (event_type) {
    case collaboration_pb::TAB_ADDED:
      return CollaborationEvent::TAB_ADDED;
    case collaboration_pb::TAB_REMOVED:
      return CollaborationEvent::TAB_REMOVED;
    case collaboration_pb::TAB_UPDATED:
      return CollaborationEvent::TAB_UPDATED;
    case collaboration_pb::TAB_GROUP_ADDED:
      return CollaborationEvent::TAB_GROUP_ADDED;
    case collaboration_pb::TAB_GROUP_REMOVED:
      return CollaborationEvent::TAB_GROUP_REMOVED;
    case collaboration_pb::TAB_GROUP_NAME_UPDATED:
      return CollaborationEvent::TAB_GROUP_NAME_UPDATED;
    case collaboration_pb::TAB_GROUP_COLOR_UPDATED:
      return CollaborationEvent::TAB_GROUP_COLOR_UPDATED;
    case collaboration_pb::COLLABORATION_ADDED:
      return CollaborationEvent::COLLABORATION_ADDED;
    case collaboration_pb::COLLABORATION_REMOVED:
      return CollaborationEvent::COLLABORATION_REMOVED;
    case collaboration_pb::COLLABORATION_MEMBER_ADDED:
      return CollaborationEvent::COLLABORATION_MEMBER_ADDED;
    case collaboration_pb::COLLABORATION_MEMBER_REMOVED:
      return CollaborationEvent::COLLABORATION_MEMBER_REMOVED;
    default:
      return CollaborationEvent::UNDEFINED;
  }
}

RecentActivityAction GetRecentActivityActionFromCollaborationEvent(
    CollaborationEvent event) {
  switch (event) {
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_UPDATED:
      return RecentActivityAction::kFocusTab;
    case CollaborationEvent::TAB_REMOVED:
      return RecentActivityAction::kReopenTab;
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      return RecentActivityAction::kOpenTabGroupEditDialog;
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return RecentActivityAction::kManageSharing;
    case CollaborationEvent::UNDEFINED:
      return RecentActivityAction::kNone;
  }
}

std::optional<GaiaId> GetGaiaIdFromMessage(
    const collaboration_pb::Message& message) {
  switch (GetMessageCategory(message)) {
    case MessageCategory::kTab:
    case MessageCategory::kTabGroup:
      if (message.triggering_user_gaia_id().empty()) {
        return std::nullopt;
      }
      return GaiaId(message.triggering_user_gaia_id());
    case MessageCategory::kCollaboration:
      if (message.affected_user_gaia_id().empty()) {
        return std::nullopt;
      }
      return GaiaId(message.affected_user_gaia_id());
    default:
      return std::nullopt;
  }
}

std::optional<data_sharing::GroupId> GroupIdForTabGroup(
    const tab_groups::SavedTabGroup& tab_group) {
  if (!tab_group.collaboration_id()) {
    return std::nullopt;
  }
  return data_sharing::GroupId(tab_group.collaboration_id().value().value());
}

tab_groups::CollaborationId ToCollaborationId(
    const data_sharing::GroupId& group_id) {
  return tab_groups::CollaborationId(group_id.value());
}

TabGroupMessageMetadata CreateTabGroupMessageMetadata(
    const tab_groups::SavedTabGroup& tab_group) {
  TabGroupMessageMetadata metadata;
  metadata.local_tab_group_id = tab_group.local_group_id();
  metadata.sync_tab_group_id = tab_group.saved_guid();
  metadata.last_known_title = base::UTF16ToUTF8(tab_group.title());
  metadata.last_known_color = tab_group.color();
  return metadata;
}

TabMessageMetadata CreateTabMessageMetadata(
    const tab_groups::SavedTabGroupTab& tab) {
  auto tab_metadata = TabMessageMetadata();
  tab_metadata.local_tab_id = tab.local_tab_id();
  tab_metadata.sync_tab_id = tab.saved_tab_guid();
  tab_metadata.last_known_url = tab.url().spec();
  tab_metadata.last_known_title = base::UTF16ToUTF8(tab.title());
  return tab_metadata;
}

TabMessageMetadata CreateTabMessageMetadataFromMessageOrTab(
    const collaboration_pb::Message& message,
    std::optional<tab_groups::SavedTabGroupTab> tab) {
  if (tab) {
    return CreateTabMessageMetadata(*tab);
  }

  // Tab no longer available, so fill in what we can.
  TabMessageMetadata tab_metadata = TabMessageMetadata();
  tab_metadata.last_known_url = message.tab_data().last_url();
  tab_metadata.sync_tab_id =
      base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());
  return tab_metadata;
}

std::optional<tab_groups::SavedTabGroupTab> GetTabFromGroup(
    const collaboration_pb::Message& message,
    std::optional<tab_groups::SavedTabGroup> tab_group) {
  if (!tab_group) {
    return std::nullopt;
  }

  const tab_groups::SavedTabGroupTab* tab = tab_group->GetTab(
      base::Uuid::ParseCaseInsensitive(message.tab_data().sync_tab_id()));
  if (tab) {
    return std::make_optional(*tab);
  }
  return std::nullopt;
}

DirtyType GetDirtyTypeFromPersistentNotificationTypeForQuery(
    std::optional<PersistentNotificationType> type) {
  if (!type) {
    // Ask for all dirty messages.
    return DirtyType::kAll;
  }
  if (*type == PersistentNotificationType::DIRTY_TAB) {
    return DirtyType::kDot;
  } else if (*type == PersistentNotificationType::CHIP) {
    return DirtyType::kChip;
  } else {
    // Ask for all dirty messages.
    return DirtyType::kAll;
  }
}

std::vector<PersistentMessage> RemoveDuplicateDirtyTabGroupMessages(
    const std::vector<PersistentMessage>& messages) {
  std::unordered_set<data_sharing::GroupId> dirty_tab_groups;
  std::vector<PersistentMessage> result;
  for (const auto& message : messages) {
    if (message.type == PersistentNotificationType::DIRTY_TAB_GROUP) {
      // We only want one DIRTY_TAB_GROUP per collaboration.
      if (dirty_tab_groups.find(message.attribution.collaboration_id) ==
          dirty_tab_groups.end()) {
        // This is the first one, so we add it.
        dirty_tab_groups.emplace(message.attribution.collaboration_id);
        result.emplace_back(message);
      }
    } else {
      // If this is not a dirty tab group, add it to the result
      result.emplace_back(message);
    }
  }
  return result;
}

std::vector<PersistentMessage> CreatePersistentMessagesForTypes(
    PersistentMessage base_message,
    const std::vector<PersistentNotificationType>& types) {
  std::vector<PersistentMessage> messages;
  for (const PersistentNotificationType& type : types) {
    PersistentMessage message = base_message;
    message.type = type;
    messages.emplace_back(message);
  }
  return messages;
}

}  // namespace

MessagingBackendServiceImpl::MessagingBackendServiceImpl(
    const MessagingBackendConfiguration& configuration,
    std::unique_ptr<TabGroupChangeNotifier> tab_group_change_notifier,
    std::unique_ptr<DataSharingChangeNotifier> data_sharing_change_notifier,
    std::unique_ptr<MessagingBackendStore> messaging_backend_store,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service)
    : configuration_(configuration),
      tab_group_change_notifier_(std::move(tab_group_change_notifier)),
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
  if (IsInitialized()) {
    // We invoke the observer here in a re-entrant manner (documented in the
    // public API), because at any time after adding the observer, new messages
    // can come in, and they could arrive before the posted task with the
    // callback executes, which means they could get calls to Display/Hide of
    // PersistentMessages before they get the call to
    // OnMessagingBackendServiceInitialized().
    observer->OnMessagingBackendServiceInitialized();
  }
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
  std::optional<tab_groups::SavedTabGroupTab> tab = GetTabFromTabId(tab_id);
  if (!tab) {
    // Unable to find tab.
    return {};
  }

  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(tab->saved_group_guid());
  if (!tab_group) {
    // Unable to find group.
    return {};
  }

  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(*tab_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID.
    return {};
  }

  DirtyType dirty_type =
      GetDirtyTypeFromPersistentNotificationTypeForQuery(type);

  std::optional<collaboration_pb::Message> message =
      store_->GetDirtyMessageForTab(*collaboration_group_id,
                                    tab->saved_tab_guid(), dirty_type);
  if (!message) {
    return {};
  }
  return ConvertMessageToPersistentMessages(
      *message, dirty_type, type, /*allow_dirty_tab_group_message=*/false);
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessagesForGroup(
    tab_groups::EitherGroupID group_id,
    std::optional<PersistentNotificationType> type) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupId(group_id);
  if (!collaboration_group_id) {
    // Unable to find collaboration.
    return {};
  }

  DirtyType dirty_type =
      GetDirtyTypeFromPersistentNotificationTypeForQuery(type);

  std::vector<collaboration_pb::Message> messages =
      store_->GetDirtyMessagesForGroup(*collaboration_group_id, dirty_type);
  return RemoveDuplicateDirtyTabGroupMessages(
      ConvertMessagesToPersistentMessages(messages, dirty_type, type));
}

std::vector<PersistentMessage> MessagingBackendServiceImpl::GetMessages(
    std::optional<PersistentNotificationType> type) {
  DirtyType dirty_type =
      GetDirtyTypeFromPersistentNotificationTypeForQuery(type);

  std::vector<collaboration_pb::Message> messages =
      store_->GetDirtyMessages(dirty_type);
  return RemoveDuplicateDirtyTabGroupMessages(
      ConvertMessagesToPersistentMessages(messages, dirty_type, type));
}

std::vector<ActivityLogItem> MessagingBackendServiceImpl::GetActivityLog(
    const ActivityLogQueryParams& params) {
  std::vector<ActivityLogItem> result;
  std::vector<collaboration_pb::Message> messages =
      store_->GetRecentMessagesForGroup(params.collaboration_id);
  int message_count = 0;
  for (const auto& message : messages) {
    std::optional<ActivityLogItem> activity_log_item =
        ConvertMessageToActivityLogItem(message);
    if (!activity_log_item) {
      continue;
    }
    result.emplace_back(*activity_log_item);
    if (params.result_length == 0) {
      continue;
    }
    if (++message_count >= params.result_length) {
      break;
    }
  }
  return result;
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
  SetCurrentlySelectedTabOnStartup();
  initialized_ = true;
  for (auto& observer : persistent_message_observers_) {
    observer.OnMessagingBackendServiceInitialized();
  }
  CHECK(data_sharing_flush_callback_);
  std::move(data_sharing_flush_callback_).Run();
}

void MessagingBackendServiceImpl::SetCurrentlySelectedTabOnStartup() {
  std::pair<std::optional<base::Uuid>, std::optional<base::Uuid>>
      selected_tab_id_pair =
          tab_group_sync_service_->GetCurrentlySelectedTabID();
  std::optional<base::Uuid> tab_group_id = selected_tab_id_pair.first;
  std::optional<base::Uuid> tab_id = selected_tab_id_pair.second;
  if (!tab_group_id || !tab_id) {
    // We are missing tab group ID or tab ID, so we will be unable to find the
    // tab.
    return;
  }
  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(*tab_group_id);
  if (!tab_group) {
    // This should not happen, but since the API returns an std::optional, we
    // should still do this check to ensure we do not crash when dereferencing
    // `tab_group`.
    return;
  }
  tab_groups::SavedTabGroupTab* tab = tab_group->GetTab(*tab_id);
  if (!tab) {
    // Just as with the `tab_group`, this is not expected to happen, but in case
    // there is an invariant we are not currently aware of we return early here.
    return;
  }

  last_selected_tab_ = *tab;
}

void MessagingBackendServiceImpl::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& added_group) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(added_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, added_group, collaboration_pb::TAB_GROUP_ADDED,
      DirtyType::kNone);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnTabGroupRemoved(
    tab_groups::SavedTabGroup removed_group) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(removed_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, removed_group,
      collaboration_pb::TAB_GROUP_REMOVED, DirtyType::kNone);
  store_->AddMessage(message);

  if (instant_message_delegate_) {
    InstantMessage instant_message;
    instant_message.attribution = CreateMessageAttributionForTabUpdates(
        message, removed_group, std::nullopt);
    instant_message.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
    instant_message.type = InstantNotificationType::UNDEFINED;

    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message, {InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnTabGroupNameUpdated(
    const tab_groups::SavedTabGroup& updated_group) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(updated_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, updated_group,
      collaboration_pb::TAB_GROUP_NAME_UPDATED, DirtyType::kNone);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnTabGroupColorUpdated(
    const tab_groups::SavedTabGroup& updated_group) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(updated_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, updated_group,
      collaboration_pb::TAB_GROUP_COLOR_UPDATED, DirtyType::kNone);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnTabAdded(
    const tab_groups::SavedTabGroupTab& added_tab) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(added_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, added_tab,
                       collaboration_pb::TAB_ADDED, DirtyType::kDotAndChip);
  store_->AddMessage(message);

  PersistentMessage persistent_message =
      CreatePersistentMessage(message, std::nullopt, added_tab, std::nullopt);

  NotifyDisplayPersistentMessagesForTypes(
      persistent_message, {PersistentNotificationType::CHIP,
                           PersistentNotificationType::DIRTY_TAB});

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           added_tab.saved_group_guid());
}

void MessagingBackendServiceImpl::OnTabRemoved(
    tab_groups::SavedTabGroupTab removed_tab) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(removed_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, removed_tab,
                       collaboration_pb::TAB_REMOVED, DirtyType::kNone);
  store_->AddMessage(message);

  // Tab no longer available, so should not contribute to any dirty tab groups.
  store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                  removed_tab.saved_tab_guid(),
                                  DirtyType::kDotAndChip);

  PersistentMessage persistent_message =
      CreatePersistentMessage(message, std::nullopt, removed_tab, std::nullopt);

  NotifyHidePersistentMessagesForTypes(persistent_message,
                                       {PersistentNotificationType::CHIP,
                                        PersistentNotificationType::DIRTY_TAB});

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           removed_tab.saved_group_guid());

  if (last_selected_tab_ &&
      last_selected_tab_->saved_tab_guid() == removed_tab.saved_tab_guid() &&
      instant_message_delegate_) {
    InstantMessage instant_message_base;
    instant_message_base.attribution = CreateMessageAttributionForTabUpdates(
        message, std::nullopt, removed_tab);
    instant_message_base.collaboration_event = CollaborationEvent::TAB_REMOVED;
    instant_message_base.type = InstantNotificationType::CONFLICT_TAB_REMOVED;

    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message_base,
                          {InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnTabUpdated(
    const tab_groups::SavedTabGroupTab& updated_tab) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(updated_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, updated_tab,
                       collaboration_pb::TAB_UPDATED, DirtyType::kDotAndChip);
  store_->AddMessage(message);

  PersistentMessage persistent_message =
      CreatePersistentMessage(message, std::nullopt, updated_tab, std::nullopt);

  NotifyDisplayPersistentMessagesForTypes(
      persistent_message, {PersistentNotificationType::CHIP,
                           PersistentNotificationType::DIRTY_TAB});

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           updated_tab.saved_group_guid());
}

void MessagingBackendServiceImpl::OnTabSelected(
    std::optional<tab_groups::SavedTabGroupTab> selected_tab) {
  if (!configuration_.clear_chip_on_tab_selection && last_selected_tab_) {
    // When we do not clear the chip on selection, we instead clear the chip
    // when a user selects a different tab after having selected one.
    std::optional<data_sharing::GroupId> last_selected_collaboration_group_id =
        GetCollaborationGroupIdForTab(last_selected_tab_.value());
    if (last_selected_collaboration_group_id) {
      store_->ClearDirtyMessageForTab(*last_selected_collaboration_group_id,
                                      last_selected_tab_->saved_tab_guid(),
                                      DirtyType::kChip);

      // Specialized handling of creating a PersistentMessage, since we do not
      // have a stored collaboration_pb::Message available.
      PersistentMessage persistent_message =
          CreatePersistentMessageFromTabGroupAndTab(
              *last_selected_collaboration_group_id, *last_selected_tab_,
              CollaborationEvent::UNDEFINED);

      NotifyHidePersistentMessagesForTypes(persistent_message,
                                           {PersistentNotificationType::CHIP});
    }
  }
  last_selected_tab_ = selected_tab;
  if (!selected_tab) {
    // A tab outside shared tab groups was selected.
    return;
  }

  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(selected_tab.value());

  if (!collaboration_group_id) {
    // Unable to find the collaboration, so nothing to clear.
    return;
  }

  if (configuration_.clear_chip_on_tab_selection) {
    store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                    selected_tab->saved_tab_guid(),
                                    DirtyType::kDotAndChip);
  } else {
    store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                    selected_tab->saved_tab_guid(),
                                    DirtyType::kDot);
  }

  PersistentMessage persistent_message =
      CreatePersistentMessageFromTabGroupAndTab(*collaboration_group_id,
                                                *selected_tab,
                                                CollaborationEvent::UNDEFINED);

  if (configuration_.clear_chip_on_tab_selection) {
    NotifyHidePersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::CHIP,
                             PersistentNotificationType::DIRTY_TAB});
  } else {
    NotifyHidePersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::DIRTY_TAB});
  }

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           selected_tab->saved_group_guid());
}

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
  std::optional<tab_groups::SavedTabGroup> tab_group;
  for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
    if (group.collaboration_id() &&
        data_sharing::GroupId(group.collaboration_id().value().value()) ==
            group_data.group_token.group_id) {
      tab_group = group;
      break;
    }
  }
  if (!tab_group) {
    // The tab group may be deleted or not synced.
    // TODO(386420717): Maybe persist the message to disk in case the tab group
    // gets synced at a later time. If this is persisted, then it may never get
    // cleared if the group was deleted.
    return;
  }

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

  if (instant_message_delegate_) {
    InstantMessage instant_message;
    instant_message.attribution.collaboration_id =
        group_data.group_token.group_id;
    instant_message.attribution.tab_group_metadata =
        CreateTabGroupMessageMetadataFromMessageOrTabGroup(message, *tab_group);
    instant_message.collaboration_event =
        CollaborationEvent::COLLABORATION_MEMBER_ADDED;

    // Look for the member in the provided data.
    for (const data_sharing::GroupMember& member : group_data.members) {
      if (member.gaia_id == member_gaia_id) {
        instant_message.attribution.affected_user = member;
        break;
      }
    }

    DisplayInstantMessage(
        base::Uuid::ParseLowercase(message.uuid()), instant_message,
        {InstantNotificationLevel::SYSTEM, InstantNotificationLevel::BROWSER});
  }
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

std::optional<ActivityLogItem>
MessagingBackendServiceImpl::ConvertMessageToActivityLogItem(
    const collaboration_pb::Message& message) {
  switch (message.event_type()) {
    case collaboration_pb::TAB_GROUP_ADDED:
    case collaboration_pb::TAB_GROUP_REMOVED:
    case collaboration_pb::COLLABORATION_ADDED:
    case collaboration_pb::COLLABORATION_REMOVED:
      return std::nullopt;
    default:
      break;
  }
  ActivityLogItem item;
  item.collaboration_event = ToCollaborationEvent(message.event_type());
  data_sharing::GroupId collaboration_group_id(message.collaboration_id());

  std::optional<GaiaId> gaia_id = GetGaiaIdFromMessage(message);
  std::optional<data_sharing::GroupMember> group_member =
      GetGroupMemberFromGaiaId(collaboration_group_id, gaia_id);
  if (gaia_id) {
    std::optional<std::string> user_name_for_display =
        GetDisplayNameForUserInGroup(collaboration_group_id, *gaia_id,
                                     std::nullopt, message);
    if (user_name_for_display) {
      item.user_display_name = *user_name_for_display;
    }
  }

  // TODO(crbug.com/380517719): Compare GaiaId with current user in this
  // profile.
  item.user_is_self = false;

  // By default, we use an empty description. This is special cased below.
  item.description = u"";
  item.time_delta =
      base::Time::Now() - base::Time::FromTimeT(message.event_timestamp());
  item.action =
      GetRecentActivityActionFromCollaborationEvent(item.collaboration_event);

  item.activity_metadata = MessageAttribution();
  item.activity_metadata.collaboration_id = collaboration_group_id;

  // The code below needs to fill in `activity_metadata`, and optionally
  // `show_favicon` if it is true.
  switch (GetMessageCategory(message)) {
    case MessageCategory::kTab: {
      item.show_favicon = true;

      std::optional<tab_groups::SavedTabGroup> tab_group =
          GetTabGroupFromMessage(message);
      item.activity_metadata.tab_group_metadata =
          CreateTabGroupMessageMetadataFromMessageOrTabGroup(message,
                                                             tab_group);
      item.activity_metadata.tab_metadata =
          CreateTabMessageMetadataFromMessageOrTab(
              message, GetTabFromGroup(message, tab_group));
      // We are guaranteed to have a value for `last_known_url`.
      GURL url = GURL(*item.activity_metadata.tab_metadata->last_known_url);
      item.activity_metadata.triggering_user = group_member;

      item.description =
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url);

      break;
    }
    case MessageCategory::kTabGroup: {
      item.activity_metadata.triggering_user = group_member;
      item.activity_metadata.tab_group_metadata =
          CreateTabGroupMessageMetadataFromMessageOrTabGroup(message,
                                                             std::nullopt);

      // Only tab group name changes have specialized description.
      if (message.event_type() == collaboration_pb::TAB_GROUP_NAME_UPDATED) {
        if (item.activity_metadata.tab_group_metadata->last_known_title) {
          item.description = base::UTF8ToUTF16(
              *item.activity_metadata.tab_group_metadata->last_known_title);
        }
      }

      break;
    }
    case MessageCategory::kCollaboration:
      item.activity_metadata.affected_user = group_member;
      if (group_member) {
        item.description = base::UTF8ToUTF16(group_member->email);
      }
      break;
    default:
      break;
  }
  return item;
}

std::optional<data_sharing::GroupId>
MessagingBackendServiceImpl::GetCollaborationGroupIdForTab(
    const tab_groups::SavedTabGroupTab& tab) {
  // Find tab group using the tab group ID and look up collaboration group ID.
  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(tab.saved_group_guid());
  if (!tab_group) {
    return std::nullopt;
  }
  return GroupIdForTabGroup(*tab_group);
}

TabGroupMessageMetadata
MessagingBackendServiceImpl::CreateTabGroupMessageMetadataFromCollaborationId(
    std::optional<tab_groups::SavedTabGroup> tab_group,
    std::optional<data_sharing::GroupId> collaboration_group_id) {
  if (tab_group) {
    return CreateTabGroupMessageMetadata(*tab_group);
  }

  TabGroupMessageMetadata tab_group_metadata = TabGroupMessageMetadata();
  if (!collaboration_group_id) {
    return tab_group_metadata;
  }
  std::optional<std::u16string> previous_title =
      tab_group_sync_service_->GetTitleForPreviouslyExistingSharedTabGroup(
          ToCollaborationId(data_sharing::GroupId(*collaboration_group_id)));
  if (previous_title) {
    tab_group_metadata.last_known_title = base::UTF16ToUTF8(*previous_title);
  }
  return tab_group_metadata;
}

TabGroupMessageMetadata
MessagingBackendServiceImpl::CreateTabGroupMessageMetadataFromMessageOrTabGroup(
    const collaboration_pb::Message& message,
    const std::optional<tab_groups::SavedTabGroup>& tab_group) {
  if (tab_group) {
    return CreateTabGroupMessageMetadata(*tab_group);
  }

  return CreateTabGroupMessageMetadataFromCollaborationId(
      GetTabGroupFromMessage(message),
      data_sharing::GroupId(message.collaboration_id()));
}

std::optional<tab_groups::SavedTabGroup>
MessagingBackendServiceImpl::GetTabGroupFromMessage(
    const collaboration_pb::Message& message) {
  std::string sync_tab_group_id = message.tab_group_data().sync_tab_group_id();
  if (sync_tab_group_id.empty()) {
    // Try from tab data next.
    sync_tab_group_id = message.tab_data().sync_tab_group_id();
  }

  if (sync_tab_group_id.empty()) {
    return std::nullopt;
  }

  return tab_group_sync_service_->GetGroup(
      base::Uuid::ParseLowercase(sync_tab_group_id));
}

std::optional<data_sharing::GroupMember>
MessagingBackendServiceImpl::GetGroupMemberFromGaiaId(
    const data_sharing::GroupId& collaboration_group_id,
    std::optional<GaiaId> gaia_id) {
  if (!gaia_id) {
    return std::nullopt;
  }

  std::optional<data_sharing::GroupMemberPartialData> group_member_data =
      data_sharing_service_->GetPossiblyRemovedGroupMember(
          collaboration_group_id, *gaia_id);
  if (group_member_data) {
    return group_member_data->ToGroupMember();
  }
  return std::nullopt;
}

std::optional<data_sharing::GroupId>
MessagingBackendServiceImpl::GetCollaborationGroupId(
    tab_groups::EitherGroupID group_id) {
  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(group_id);
  if (!tab_group) {
    return std::nullopt;
  }
  return GroupIdForTabGroup(*tab_group);
}

std::optional<tab_groups::SavedTabGroupTab>
MessagingBackendServiceImpl::GetTabFromTabId(tab_groups::EitherTabID tab_id) {
  if (std::holds_alternative<base::Uuid>(tab_id)) {
    base::Uuid sync_tab_id = std::get<base::Uuid>(tab_id);
    for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
      if (group.ContainsTab(sync_tab_id)) {
        return std::make_optional(*group.GetTab(sync_tab_id));
      }
    }
  }
  if (std::holds_alternative<tab_groups::LocalTabID>(tab_id)) {
    tab_groups::LocalTabID local_tab_id =
        std::get<tab_groups::LocalTabID>(tab_id);
    for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
      if (group.ContainsTab(local_tab_id)) {
        return std::make_optional(*group.GetTab(local_tab_id));
      }
    }
  }
  return std::nullopt;
}

std::vector<PersistentMessage>
MessagingBackendServiceImpl::ConvertMessagesToPersistentMessages(
    const std::vector<collaboration_pb::Message>& messages,
    DirtyType lookup_dirty_type,
    const std::optional<PersistentNotificationType>& type) {
  std::vector<PersistentMessage> result;
  for (const auto& message : messages) {
    // Each DB message might result in multiple individual PersistentMessages.
    std::vector<PersistentMessage> converted_messages =
        ConvertMessageToPersistentMessages(
            message, lookup_dirty_type, type,
            /*allow_dirty_tab_group_message=*/true);
    result.insert(result.end(), converted_messages.begin(),
                  converted_messages.end());
  }
  return result;
}

std::vector<PersistentMessage>
MessagingBackendServiceImpl::ConvertMessageToPersistentMessages(
    const collaboration_pb::Message& message,
    DirtyType lookup_dirty_type,
    const std::optional<PersistentNotificationType>& type,
    bool allow_dirty_tab_group_message) {
  std::vector<PersistentMessage> persistent_messages;
  if (GetMessageCategory(message) != MessageCategory::kTab) {
    return persistent_messages;
  }

  // Helper local variables to increase readability of code below.
  bool has_dirty_chip = message.dirty() & static_cast<int>(DirtyType::kChip);
  bool looking_for_dirty_chip = lookup_dirty_type == DirtyType::kAll ||
                                lookup_dirty_type == DirtyType::kChip;
  bool has_dirty_dot = message.dirty() & static_cast<int>(DirtyType::kDot);
  bool looking_for_dirty_dot = lookup_dirty_type == DirtyType::kAll ||
                               lookup_dirty_type == DirtyType::kDot;
  bool add_dirty_tab_messages =
      !type || *type == PersistentNotificationType::DIRTY_TAB;
  bool add_dirty_tab_group_messages =
      allow_dirty_tab_group_message &&
      (!type || *type == PersistentNotificationType::DIRTY_TAB_GROUP);
  bool has_dirty_tab_messages_in_group =
      !store_
           ->GetDirtyMessagesForGroup(
               data_sharing::GroupId(message.collaboration_id()),
               DirtyType::kDot)
           .empty();

  std::optional<tab_groups::SavedTabGroup> tab_group =
      GetTabGroupFromMessage(message);

  if (has_dirty_chip && looking_for_dirty_chip) {
    persistent_messages.push_back(CreatePersistentMessage(
        message, tab_group, std::nullopt, PersistentNotificationType::CHIP));
  }

  if (has_dirty_dot && looking_for_dirty_dot) {
    if (add_dirty_tab_messages) {
      persistent_messages.push_back(
          CreatePersistentMessage(message, tab_group, std::nullopt,
                                  PersistentNotificationType::DIRTY_TAB));
    }

    if (add_dirty_tab_group_messages && has_dirty_tab_messages_in_group) {
      PersistentMessage persistent_message =
          CreatePersistentMessage(message, tab_group, std::nullopt,
                                  PersistentNotificationType::DIRTY_TAB_GROUP);
      // Override collaboration event and tab metadata since this is about
      // a group.
      persistent_message.collaboration_event = CollaborationEvent::UNDEFINED;
      persistent_message.attribution.tab_metadata = TabMessageMetadata();
      persistent_messages.push_back(persistent_message);
    }
  }
  return persistent_messages;
}

PersistentMessage MessagingBackendServiceImpl::CreatePersistentMessage(
    const collaboration_pb::Message& message,
    const std::optional<tab_groups::SavedTabGroup>& tab_group,
    const std::optional<tab_groups::SavedTabGroupTab>& tab,
    const std::optional<PersistentNotificationType>& type) {
  PersistentMessage persistent_message;
  persistent_message.collaboration_event =
      ToCollaborationEvent(message.event_type());
  persistent_message.attribution =
      CreateMessageAttributionForTabUpdates(message, tab_group, tab);
  if (type) {
    persistent_message.type = *type;
  }
  return persistent_message;
}

void MessagingBackendServiceImpl::NotifyDisplayPersistentMessagesForTypes(
    const PersistentMessage& base_message,
    const std::vector<PersistentNotificationType>& types) {
  for (const PersistentMessage& message :
       CreatePersistentMessagesForTypes(base_message, types)) {
    persistent_message_observers_.Notify(
        &PersistentMessageObserver::DisplayPersistentMessage, message);
  }
}

void MessagingBackendServiceImpl::NotifyHidePersistentMessagesForTypes(
    const PersistentMessage& base_message,
    const std::vector<PersistentNotificationType>& types) {
  for (const PersistentMessage& message :
       CreatePersistentMessagesForTypes(base_message, types)) {
    persistent_message_observers_.Notify(
        &PersistentMessageObserver::HidePersistentMessage, message);
  }
}

void MessagingBackendServiceImpl::DisplayOrHideTabGroupDirtyDotForTabGroup(
    const data_sharing::GroupId& collaboration_group_id,
    base::Uuid shared_tab_group_id) {
  bool hasDirtyDotMessagesForGroup =
      !store_->GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot)
           .empty();

  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(shared_tab_group_id);
  PersistentMessage persistent_message;

  persistent_message.attribution = MessageAttribution();
  persistent_message.attribution.collaboration_id = collaboration_group_id;

  if (tab_group) {
    persistent_message.attribution.tab_group_metadata =
        CreateTabGroupMessageMetadata(*tab_group);
  } else {
    // Unable to find the group, so we fill in what we known.
    persistent_message.attribution.tab_group_metadata =
        TabGroupMessageMetadata();
    persistent_message.attribution.tab_group_metadata->sync_tab_group_id =
        shared_tab_group_id;
    std::optional<std::u16string> previous_title =
        tab_group_sync_service_->GetTitleForPreviouslyExistingSharedTabGroup(
            ToCollaborationId(collaboration_group_id));
    if (previous_title) {
      persistent_message.attribution.tab_group_metadata->last_known_title =
          base::UTF16ToUTF8(*previous_title);
    }
  }

  persistent_message.collaboration_event = CollaborationEvent::UNDEFINED;
  // We do not fill in triggering user or affeted user, because any action
  // related to dirty tabs could have been relevant here.

  if (hasDirtyDotMessagesForGroup) {
    NotifyDisplayPersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::DIRTY_TAB_GROUP});
  } else {
    NotifyHidePersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::DIRTY_TAB_GROUP});
  }
}

MessageAttribution
MessagingBackendServiceImpl::CreateMessageAttributionForTabUpdates(
    const collaboration_pb::Message& message,
    const std::optional<tab_groups::SavedTabGroup>& tab_group,
    const std::optional<tab_groups::SavedTabGroupTab>& tab) {
  MessageAttribution attribution;
  attribution.collaboration_id =
      data_sharing::GroupId(message.collaboration_id());
  std::optional<tab_groups::SavedTabGroup> stack_tab_group = tab_group;
  if (!tab_group && tab) {
    stack_tab_group =
        tab_group_sync_service_->GetGroup(tab->saved_group_guid());
  }
  attribution.tab_group_metadata =
      CreateTabGroupMessageMetadataFromMessageOrTabGroup(message,
                                                         stack_tab_group);
  attribution.tab_metadata = CreateTabMessageMetadataFromMessageOrTab(
      message,
      tab.has_value() ? tab : GetTabFromGroup(message, stack_tab_group));
  attribution.triggering_user = GetGroupMemberFromGaiaId(
      data_sharing::GroupId(message.collaboration_id()),
      GaiaId(message.triggering_user_gaia_id()));
  return attribution;
}

void MessagingBackendServiceImpl::DisplayInstantMessage(
    const base::Uuid& db_message_uuid,
    const InstantMessage& base_message,
    const std::vector<InstantNotificationLevel>& levels) {
  CHECK(instant_message_delegate_);
  for (InstantNotificationLevel level : levels) {
    InstantMessage instant_message = base_message;
    instant_message.level = level;
    instant_message_delegate_->DisplayInstantaneousMessage(
        instant_message,
        base::BindOnce(&MessagingBackendServiceImpl::ClearMessageDirtyBit,
                       weak_ptr_factory_.GetWeakPtr(), db_message_uuid));
  }
}

void MessagingBackendServiceImpl::ClearMessageDirtyBit(base::Uuid db_message_id,
                                                       bool success) {
  if (!success) {
    return;
  }

  store_->ClearDirtyMessage(db_message_id, DirtyType::kMessageOnly);
}

PersistentMessage
MessagingBackendServiceImpl::CreatePersistentMessageFromTabGroupAndTab(
    const data_sharing::GroupId& collaboration_group_id,
    const tab_groups::SavedTabGroupTab tab,
    CollaborationEvent collaboration_event) {
  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(tab.saved_group_guid());

  PersistentMessage persistent_message;
  persistent_message.collaboration_event = collaboration_event;
  persistent_message.attribution = MessageAttribution();
  persistent_message.attribution.collaboration_id = collaboration_group_id;
  if (tab_group) {
    persistent_message.attribution.tab_group_metadata =
        CreateTabGroupMessageMetadata(*tab_group);
  }
  persistent_message.attribution.tab_metadata =
      CreateTabMessageMetadata(*last_selected_tab_);
  return persistent_message;
}

}  // namespace collaboration::messaging
