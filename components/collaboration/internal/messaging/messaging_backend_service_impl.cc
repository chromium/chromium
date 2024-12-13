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
#include "base/strings/utf_string_conversions.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
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
  initialized_ = true;
  for (auto& observer : persistent_message_observers_) {
    observer.OnMessagingBackendServiceInitialized();
  }
  CHECK(data_sharing_flush_callback_);
  std::move(data_sharing_flush_callback_).Run();
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
}

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
  std::optional<data_sharing::GroupMember> group_member;
  if (gaia_id) {
    std::optional<std::string> user_name_for_display =
        GetDisplayNameForUserInGroup(collaboration_group_id, *gaia_id,
                                     std::nullopt, message);
    if (user_name_for_display) {
      item.user_display_name = *user_name_for_display;
    }
    std::optional<data_sharing::GroupMemberPartialData> group_member_data =
        data_sharing_service_->GetPossiblyRemovedGroupMember(
            collaboration_group_id, *gaia_id);
    if (group_member_data) {
      group_member = group_member_data->ToGroupMember();
    }
  }

  // TODO(nyquist): Compare GaiaId with current user in this profile.
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
          tab_group_sync_service_->GetGroup(base::Uuid::ParseCaseInsensitive(
              message.tab_data().sync_tab_group_id()));
      if (!tab_group) {
        break;
      }
      item.activity_metadata.tab_group_metadata =
          CreateTabGroupMessageMetadata(*tab_group);
      tab_groups::SavedTabGroupTab* tab = tab_group->GetTab(
          base::Uuid::ParseCaseInsensitive(message.tab_data().sync_tab_id()));
      GURL url;
      if (tab) {
        item.activity_metadata.tab_metadata = CreateTabMessageMetadata(*tab);
        url = tab->url();
      } else {
        // Tab no longer available, so fill in what we can.
        item.activity_metadata.tab_metadata = TabMessageMetadata();
        item.activity_metadata.tab_metadata->last_known_url =
            message.tab_data().last_url();
        item.activity_metadata.tab_metadata->sync_tab_id =
            base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());
        url = GURL(message.tab_data().last_url());
      }
      item.activity_metadata.triggering_user = group_member;

      item.description =
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url);

      break;
    }
    case MessageCategory::kTabGroup: {
      item.activity_metadata.triggering_user = group_member;
      std::optional<tab_groups::SavedTabGroup> tab_group;
      if (!message.tab_group_data().sync_tab_group_id().empty()) {
        tab_group =
            tab_group_sync_service_->GetGroup(base::Uuid::ParseLowercase(
                message.tab_group_data().sync_tab_group_id()));
      }
      if (tab_group) {
        item.activity_metadata.tab_group_metadata =
            CreateTabGroupMessageMetadata(*tab_group);
      } else {
        item.activity_metadata.tab_group_metadata = TabGroupMessageMetadata();
        std::optional<std::u16string> previous_title =
            tab_group_sync_service_
                ->GetTitleForPreviouslyExistingSharedTabGroup(
                    ToCollaborationId(collaboration_group_id));
        if (previous_title) {
          item.activity_metadata.tab_group_metadata->last_known_title =
              base::UTF16ToUTF8(*previous_title);
        }
      }

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

}  // namespace collaboration::messaging
