// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/collaboration_id.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

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

std::u16string TruncateTabTitle(const std::u16string& original_title) {
  constexpr int kMaxTabTitleCharacters = 28;
  constexpr char16_t kEllipsis = u'\u2026';
  std::u16string trimmed;
  base::TrimWhitespace(original_title, base::TrimPositions::TRIM_ALL, &trimmed);

  // If the size of the text is already smaller than the max size without
  // grapheme counting, then we can just return the text untrimmed.
  if (trimmed.size() <= kMaxTabTitleCharacters) {
    return trimmed;
  }

  // Count the number of graphemes, stopping when we hit the max size.
  // Copy the string_view contents over to the result string.
  std::u16string result;
  base::i18n::BreakIterator iter(trimmed,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  iter.Init();
  int seen = 0;
  while (seen < kMaxTabTitleCharacters && iter.Advance()) {
    ++seen;

    // GetString returns the span that Advance() moved forwards on.
    auto span = iter.GetString();
    result.append(span.data(), span.size());
  }

  // If seen count hit the max characters and there are still more, add the
  // ellipsis.
  if (iter.Advance()) {
    result.push_back(kEllipsis);
  }
  return result;
}

collaboration_pb::Message CreateTabGroupMessage(
    data_sharing::GroupId collaboration_group_id,
    const tab_groups::SavedTabGroup& tab_group,
    collaboration_pb::EventType event_type,
    DirtyType dirty_type) {
  collaboration_pb::Message message = CreateMessage(
      collaboration_group_id, event_type, dirty_type, tab_group.update_time());
  message.mutable_tab_group_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  message.mutable_tab_group_data()->set_title(
      base::UTF16ToUTF8(tab_group.title()));
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
  collaboration_pb::Message message = CreateMessage(
      collaboration_group_id, event_type, dirty_type,
      event_type == collaboration_pb::TAB_ADDED ? tab.creation_time()
                                                : tab.update_time());
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

// Enum used to show the correct selector variant of the string template.
enum class TimeDimension {
  kMinutes = 0,
  kHours = 1,
  kDays = 2,
  kMaxValue = kDays,
};

// Gets the string representation of the given time delta. Time is
// binned into minutes, hours, or days.
std::u16string GetElapsedTimeText(base::TimeDelta time_delta) {
  TimeDimension dimension;
  int number;
  if (time_delta < base::Minutes(1)) {
    return l10n_util::GetStringUTF16(IDS_DATA_SHARING_RECENT_ACTIVITY_JUST_NOW);
  } else if (time_delta < base::Hours(1)) {
    dimension = TimeDimension::kMinutes;
    number = time_delta.InMinutes();
  } else if (time_delta < base::Days(1)) {
    dimension = TimeDimension::kHours;
    number = time_delta.InHours();
  } else {
    dimension = TimeDimension::kDays;
    number = time_delta.InDays();
  }
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringFUTF16(
          IDS_DATA_SHARING_RECENT_ACTIVITY_TIME_DELTA,
          base::UTF8ToUTF16(base::NumberToString(number))),
      static_cast<int>(dimension));
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

syncer::CollaborationId ToCollaborationId(
    const data_sharing::GroupId& group_id) {
  return syncer::CollaborationId(group_id.value());
}

TabGroupMessageMetadata CreateTabGroupMessageMetadata(
    const tab_groups::SavedTabGroup& tab_group) {
  TabGroupMessageMetadata metadata;
  metadata.local_tab_group_id = tab_group.local_group_id();
  metadata.sync_tab_group_id = tab_group.saved_guid();
  metadata.last_known_title = base::UTF16ToUTF8(tab_group.title());
  if (metadata.last_known_title->empty()) {
    metadata.last_known_title = l10n_util::GetPluralStringFUTF8(
        IDS_DATA_SHARING_TAB_GROUP_DEFAULT_TITLE_TABS_COUNT,
        tab_group.saved_tabs().size());
  }
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

std::u16string GetTitleForTabRemovedMessage(const InstantMessage& message) {
  const auto& attribution = message.attributions[0];
  std::optional<data_sharing::GroupMember> user = attribution.triggering_user;
  std::optional<TabMessageMetadata> tab_metadata = attribution.tab_metadata;
  const bool has_title =
      tab_metadata.has_value() && tab_metadata->last_known_title.has_value();
  if (!user.has_value() || !has_title) {
    return std::u16string();
  }

  return l10n_util::GetStringFUTF16(
      IDS_DATA_SHARING_TOAST_TAB_REMOVED, base::UTF8ToUTF16(user->given_name),
      TruncateTabTitle(
          base::UTF8ToUTF16(tab_metadata->last_known_title.value())));
}

std::u16string GetTitleForTabUpdatedMessage(const InstantMessage& message) {
  const auto& attribution = message.attributions[0];
  std::optional<data_sharing::GroupMember> user = attribution.triggering_user;
  std::optional<TabMessageMetadata> tab_metadata = attribution.tab_metadata;
  const bool has_title =
      tab_metadata.has_value() && tab_metadata->last_known_title.has_value();
  if (!user.has_value() || !has_title) {
    return std::u16string();
  }

  return l10n_util::GetStringFUTF16(
      IDS_DATA_SHARING_TOAST_TAB_UPDATED, base::UTF8ToUTF16(user->given_name),
      TruncateTabTitle(
          base::UTF8ToUTF16(tab_metadata->last_known_title.value())));
}

std::u16string GetTitleForMemberAddedMessage(const InstantMessage& message) {
  const auto& attribution = message.attributions[0];
  std::optional<data_sharing::GroupMember> user = attribution.affected_user;
  std::optional<TabGroupMessageMetadata> tab_group_metadata =
      attribution.tab_group_metadata;
  const bool has_group_title = tab_group_metadata.has_value() &&
                               tab_group_metadata->last_known_title.has_value();
  if (!user.has_value() || !has_group_title) {
    return std::u16string();
  }

  return l10n_util::GetStringFUTF16(
      IDS_DATA_SHARING_TOAST_NEW_MEMBER, base::UTF8ToUTF16(user->given_name),
      TruncateTabTitle(
          base::UTF8ToUTF16(tab_group_metadata->last_known_title.value())));
}

std::u16string GetTitleForTabGroupRemovedMessage(
    const InstantMessage& message) {
  const auto& attribution = message.attributions[0];
  std::optional<TabGroupMessageMetadata> tab_group_metadata =
      attribution.tab_group_metadata;
  const bool has_group_title = tab_group_metadata.has_value() &&
                               tab_group_metadata->last_known_title.has_value();
  if (!has_group_title) {
    return std::u16string();
  }

  return l10n_util::GetStringFUTF16(
      IDS_DATA_SHARING_TOAST_BLOCK_LEAVE,
      TruncateTabTitle(
          base::UTF8ToUTF16(tab_group_metadata->last_known_title.value())));
}

DirtyType GetDirtyTypeFromPersistentNotificationTypeForQuery(
    PersistentNotificationType type) {
  switch (type) {
    case PersistentNotificationType::DIRTY_TAB:
      return DirtyType::kDot;
    case PersistentNotificationType::CHIP:
      return DirtyType::kChip;
    case PersistentNotificationType::TOMBSTONED:
      return DirtyType::kTombstoned;
    case PersistentNotificationType::INSTANT_MESSAGE:
      return DirtyType::kMessageOnly;
    default:
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

bool IsMemberOwner(const data_sharing::GroupData& group_data,
                   const GaiaId& member_gaia_id) {
  for (const data_sharing::GroupMember& member : group_data.members) {
    if (member.gaia_id == member_gaia_id) {
      return member.role == data_sharing::MemberRole::kOwner;
    }
  }

  return false;
}

bool IsMemberCurrentUser(const signin::IdentityManager* identity_manager,
                         const GaiaId& gaia_id) {
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    return false;
  }

  return account.gaia == gaia_id;
}

bool IsMemberSelfOrOwner(const signin::IdentityManager* identity_manager,
                         const data_sharing::GroupData& group_data,
                         const GaiaId& member_gaia_id) {
  return IsMemberCurrentUser(identity_manager, member_gaia_id) ||
         IsMemberOwner(group_data, member_gaia_id);
}

bool IsCurrentUserOwner(const signin::IdentityManager* identity_manager,
                        const data_sharing::GroupData& group_data) {
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    return false;
  }

  return IsMemberOwner(group_data, account.gaia);
}

bool HasSeenTabUpdate(const tab_groups::SavedTabGroupTab& tab) {
  return tab.last_seen_time() >= tab.navigation_time();
}

}  // namespace

// MessagingBackendServiceImpl is the central component for handling
// collaboration messages. It integrates events from TabGroupSyncService and
// DataSharingService, persists them to storage, and notifies UI components of
// relevant changes.
//
// The initialization of this service is critical and follows a strict order to
// ensure data consistency and prevent race conditions:
//
// 1. MessagingBackendStore: The service first initializes its backing store to
//    ensure that message persistence is available.
//
// 2. DataSharingService Integration: It then initializes its connection to the
//    DataSharingService via DataSharingChangeNotifier. This step is
//    asynchronous, waiting for the DataSharingService's GroupDataModel to fully
//    load. Crucially, the DataSharingChangeNotifier provides a callback to the
//    MessagingBackendServiceImpl upon its own initialization. This callback
//    acts as a gate; it is not executed immediately. Its purpose is to defer
//    the processing of any queued or subsequent data sharing events until the
//    entire messaging system is ready.
//
// 3. TabGroupSyncService Integration: After the DataSharingChangeNotifier is
//    ready, the service proceeds to initialize its connection to the
//    TabGroupSyncService via TabGroupChangeNotifier. This also waits for the
//    underlying SavedTabGroupModel to load. This ordering is vital because
//    handling shared tab group events often requires access to collaboration
//    data, which must be available beforehand.
//
// 4. Finalization and Flush: Once the TabGroupChangeNotifier confirms it is
//    initialized, the MessagingBackendServiceImpl considers itself fully
//    online. It notifies its own observers and then, finally, executes the
//    pending callback from the DataSharingChangeNotifier. This "flushes" any
//    queued data sharing events, ensuring they are processed with the full
//    context of both services being available.
//
// This structured sequence guarantees that events are handled correctly and
// that dependencies are met before any actions are taken.
MessagingBackendServiceImpl::MessagingBackendServiceImpl(
    const MessagingBackendConfiguration& configuration,
    std::unique_ptr<TabGroupChangeNotifier> tab_group_change_notifier,
    std::unique_ptr<DataSharingChangeNotifier> data_sharing_change_notifier,
    std::unique_ptr<MessagingBackendStore> messaging_backend_store,
    std::unique_ptr<InstantMessageProcessor> instant_message_processor,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager)
    : configuration_(configuration),
      tab_group_change_notifier_(std::move(tab_group_change_notifier)),
      data_sharing_change_notifier_(std::move(data_sharing_change_notifier)),
      store_(std::move(messaging_backend_store)),
      instant_message_processor_(std::move(instant_message_processor)),
      tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service),
      identity_manager_(identity_manager) {
  instant_message_processor_->SetMessagingBackendService(this);
  store_->Initialize(
      base::BindOnce(&MessagingBackendServiceImpl::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

MessagingBackendServiceImpl::~MessagingBackendServiceImpl() = default;

void MessagingBackendServiceImpl::SetInstantMessageDelegate(
    InstantMessageDelegate* instant_message_delegate) {
  instant_message_processor_->SetInstantMessageDelegate(
      instant_message_delegate);
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
    PersistentNotificationType type) {
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
    PersistentNotificationType type) {
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
    PersistentNotificationType type) {
  DirtyType dirty_type =
      GetDirtyTypeFromPersistentNotificationTypeForQuery(type);

  std::vector<collaboration_pb::Message> messages =
      store_->GetDirtyMessages(dirty_type);
  return RemoveDuplicateDirtyTabGroupMessages(
      ConvertMessagesToPersistentMessages(messages, dirty_type, type));
}

std::vector<ActivityLogItem> MessagingBackendServiceImpl::GetActivityLog(
    const ActivityLogQueryParams& params) {
  if (activity_log_for_testing_.contains(params.collaboration_id)) {
    CHECK_IS_TEST();
    return activity_log_for_testing_.at(params.collaboration_id);
  }

  const bool show_activity_for_single_tab = params.local_tab_id.has_value();
  std::vector<ActivityLogItem> result;
  std::vector<collaboration_pb::Message> messages =
      store_->GetRecentMessagesForGroup(params.collaboration_id);
  int message_count = 0;
  for (const auto& message : messages) {
    std::optional<ActivityLogItem> activity_log_item =
        ConvertMessageToActivityLogItem(message, show_activity_for_single_tab);
    if (!activity_log_item) {
      continue;
    }
    // If local_tab_id was supplied, filter for activity on this tab.
    if (show_activity_for_single_tab) {
      if (!activity_log_item->activity_metadata.tab_metadata.has_value()) {
        continue;
      }
      if (params.local_tab_id !=
          activity_log_item->activity_metadata.tab_metadata->local_tab_id) {
        continue;
      }
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

void MessagingBackendServiceImpl::ClearDirtyTabMessagesForGroup(
    const data_sharing::GroupId& collaboration_group_id,
    const std::optional<tab_groups::SavedTabGroup>& tab_group) {
  // Clear the dirty bits from the storage.
  auto cleared_messages =
      store_->ClearDirtyTabMessagesForGroup(collaboration_group_id);
  if (!tab_group) {
    return;
  }

  std::vector<base::Uuid> cleared_tab_ids;

  // Since the dirty bits are cleared from DB, hide any dirty dots from the tabs
  // and tab groups if they are already showing.
  for (auto& message : cleared_messages) {
    PersistentMessage persistent_message;
    persistent_message.collaboration_event =
        ToCollaborationEvent(message.event_type());
    persistent_message.attribution =
        CreateMessageAttributionForTabUpdates(message, tab_group, std::nullopt);
    NotifyHidePersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::CHIP,
                             PersistentNotificationType::DIRTY_TAB});
    if (persistent_message.attribution.tab_metadata.has_value() &&
        persistent_message.attribution.tab_metadata->sync_tab_id.has_value()) {
      cleared_tab_ids.emplace_back(
          persistent_message.attribution.tab_metadata->sync_tab_id.value());
    }

    if (persistent_message.attribution.tab_group_metadata &&
        persistent_message.attribution.tab_group_metadata->sync_tab_group_id) {
      base::Uuid tab_group_id =
          persistent_message.attribution.tab_group_metadata->sync_tab_group_id
              .value();
      DisplayOrHideTabGroupDirtyDotForTabGroup(collaboration_group_id,
                                               tab_group_id);
    }
  }

  for (const base::Uuid& tab_id : cleared_tab_ids) {
    tab_group_sync_service_->UpdateTabLastSeenTime(
        tab_group->saved_guid(), tab_id, tab_groups::TriggerSource::LOCAL);
  }
}

void MessagingBackendServiceImpl::ClearDirtyTabMessagesForGroup(
    const data_sharing::GroupId& collaboration_group_id) {
  ClearDirtyTabMessagesForGroup(
      collaboration_group_id,
      GetTabGroupFromCollaborationId(collaboration_group_id.value()));
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

void MessagingBackendServiceImpl::OnSyncDisabled() {
  store_->RemoveAllMessages();
}

void MessagingBackendServiceImpl::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& added_group,
    tab_groups::TriggerSource source) {
  if (source == tab_groups::TriggerSource::LOCAL) {
    // No message to show or clear if a tab group was added locally.
    return;
  }

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
    tab_groups::SavedTabGroup removed_group,
    tab_groups::TriggerSource source) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(removed_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  // Clear any the dirty persistent messages related to the group that are
  // already showing in UI. This is important in unshare flow since the tab
  // group continues to exist in the UI. This will also clear the dirty bits in
  // the DB and notify all the observers to update the UI.
  ClearDirtyTabMessagesForGroup(*collaboration_group_id, removed_group);

  // Remove all messages from the DB related to this tab group (including the
  // ones that were just cleared from dirty state). The only message that will
  // stay will be the group removal message which will be added in the next
  // section.
  std::vector<collaboration_pb::Message> messages =
      store_->GetRecentMessagesForGroup(*collaboration_group_id);
  std::set<std::string> message_uuid_strings;
  std::set<base::Uuid> message_uuids;
  for (auto& message : messages) {
    message_uuid_strings.insert(message.uuid());
    message_uuids.insert(base::Uuid::ParseLowercase(message.uuid()));
  }
  store_->RemoveMessages(message_uuid_strings);

  // Regardless of whether the user is leaving or deleting the group and
  // regardless of whether it happened from a remote event or a local event,
  // we should hide any instant messages related to the group.
  instant_message_processor_->HideInstantMessage(message_uuids);

  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  // If the user themselves are trying to leave or delete the group, they don't
  // need to be notified of anything. Note that although real event source is
  // local, it appears to be a remote event since the leave / delete attempt and
  // tab group removal is processed only after a commit happens to the server
  // side.
  if (data_sharing_service_->IsLeavingOrDeletingGroup(
          *collaboration_group_id)) {
    return;
  }

  // If the current user is the owner of the group, they might be unsharing the
  // group. Ignore the message.
  std::optional<data_sharing::GroupData> group_data =
      data_sharing_service_->ReadGroup(*collaboration_group_id);
  if (!group_data) {
    group_data =
        data_sharing_service_->GetPossiblyRemovedGroup(*collaboration_group_id);
  }

  if (group_data.has_value() &&
      IsCurrentUserOwner(identity_manager_, *group_data)) {
    return;
  }

  collaboration_pb::Message message =
      CreateTabGroupMessage(*collaboration_group_id, removed_group,
                            collaboration_pb::TAB_GROUP_REMOVED,
                            DirtyType::kTombstonedAndInstantMessage);
  store_->AddMessage(message);

  PersistentMessage persistent_message =
      CreatePersistentMessage(message, removed_group, std::nullopt,
                              PersistentNotificationType::TOMBSTONED);
  persistent_message.collaboration_event =
      CollaborationEvent::TAB_GROUP_REMOVED;
  NotifyDisplayPersistentMessagesForTypes(
      persistent_message, {PersistentNotificationType::TOMBSTONED});

  if (instant_message_processor_->IsEnabled()) {
    InstantMessage instant_message =
        CreateInstantMessage(message, removed_group, /*tab=*/std::nullopt);
    instant_message.type = InstantNotificationType::UNDEFINED;
    instant_message.localized_message =
        GetTitleForTabGroupRemovedMessage(instant_message);
    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message, {InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnTabGroupNameUpdated(
    const tab_groups::SavedTabGroup& updated_group,
    tab_groups::TriggerSource source) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(updated_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }

  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, updated_group,
      collaboration_pb::TAB_GROUP_NAME_UPDATED, DirtyType::kNone);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnTabGroupColorUpdated(
    const tab_groups::SavedTabGroup& updated_group,
    tab_groups::TriggerSource source) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(updated_group);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab group.
    return;
  }
  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  collaboration_pb::Message message = CreateTabGroupMessage(
      *collaboration_group_id, updated_group,
      collaboration_pb::TAB_GROUP_COLOR_UPDATED, DirtyType::kNone);
  store_->AddMessage(message);
}

void MessagingBackendServiceImpl::OnTabAdded(
    const tab_groups::SavedTabGroupTab& added_tab,
    tab_groups::TriggerSource source) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(added_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  bool is_local = source == tab_groups::TriggerSource::LOCAL;
  bool triggering_user_is_self = IsMemberCurrentUser(
      identity_manager_, added_tab.shared_attribution().created_by);
  DirtyType dirty_type = (is_local || triggering_user_is_self)
                             ? DirtyType::kNone
                             : DirtyType::kDotAndChip;

  if (HasSeenTabUpdate(added_tab)) {
    dirty_type = DirtyType::kNone;
  }

  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, added_tab,
                       collaboration_pb::TAB_ADDED, dirty_type);
  store_->AddMessage(message);

  if (dirty_type != DirtyType::kNone) {
    PersistentMessage persistent_message =
        CreatePersistentMessage(message, std::nullopt, added_tab,
                                PersistentNotificationType::UNDEFINED);

    NotifyDisplayPersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::CHIP,
                             PersistentNotificationType::DIRTY_TAB});
  }

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           added_tab.saved_group_guid());
}

void MessagingBackendServiceImpl::OnTabRemoved(
    tab_groups::SavedTabGroupTab removed_tab,
    tab_groups::TriggerSource source,
    bool is_selected) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(removed_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  bool is_local = source == tab_groups::TriggerSource::LOCAL;
  bool triggering_user_is_self = IsMemberCurrentUser(
      identity_manager_, removed_tab.shared_attribution().updated_by);
  DirtyType dirty_type = (is_local || triggering_user_is_self)
                             ? DirtyType::kNone
                             : DirtyType::kTombstoned;
  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, removed_tab,
                       collaboration_pb::TAB_REMOVED, dirty_type);
  store_->AddMessage(message);

  // Tab is no longer available, so should not contribute to any dirty tab
  // groups.
  store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                  removed_tab.saved_tab_guid(),
                                  DirtyType::kDotAndChip);

  // Hide any existing persistent dot or chip messages already showing.
  PersistentMessage persistent_message =
      CreatePersistentMessage(message, std::nullopt, removed_tab,
                              PersistentNotificationType::UNDEFINED);

  NotifyHidePersistentMessagesForTypes(persistent_message,
                                       {PersistentNotificationType::CHIP,
                                        PersistentNotificationType::DIRTY_TAB});

  // Hide any dirty dot on the tab group.
  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           removed_tab.saved_group_guid());

  if (dirty_type == DirtyType::kNone) {
    return;
  }

  if (is_selected && instant_message_processor_->IsEnabled()) {
    InstantMessage instant_message =
        CreateInstantMessage(message, /*tab_group=*/std::nullopt, removed_tab);
    instant_message.type = InstantNotificationType::CONFLICT_TAB_REMOVED;
    instant_message.localized_message =
        GetTitleForTabRemovedMessage(instant_message);

    // TODO(crbug.com/390794240): Remove the id argument to
    // DisplayInstantMessage as it's now contained inside the
    // MessageAttribution.
    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message, {InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnTabUpdated(
    const tab_groups::SavedTabGroupTab& before,
    const tab_groups::SavedTabGroupTab& after,
    tab_groups::TriggerSource source,
    bool is_selected) {
  const tab_groups::SavedTabGroupTab& updated_tab = after;
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GetCollaborationGroupIdForTab(updated_tab);
  if (!collaboration_group_id) {
    // Unable to find collaboration ID from tab.
    return;
  }

  bool is_local = source == tab_groups::TriggerSource::LOCAL;
  bool triggering_user_is_self = IsMemberCurrentUser(
      identity_manager_, updated_tab.shared_attribution().updated_by);

  DirtyType dirty_type =
      (is_local || triggering_user_is_self)
          ? DirtyType::kNone
          : (is_selected ? DirtyType::kChip : DirtyType::kDotAndChip);
  if (HasSeenTabUpdate(updated_tab) && dirty_type != DirtyType::kNone) {
    // If the tab has been seen before, we should not show dirty dots, only
    // the chip.
    dirty_type = DirtyType::kChip;
  }

  collaboration_pb::Message message =
      CreateTabMessage(*collaboration_group_id, updated_tab,
                       collaboration_pb::TAB_UPDATED, dirty_type);
  store_->AddMessage(message);

  if (dirty_type == DirtyType::kNone) {
    // For local updates, hide any dirty messages for tab from storage and
    // dismiss any messages already being displayed for tab.
    store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                    updated_tab.saved_tab_guid(),
                                    DirtyType::kDotAndChip);
  }

  PersistentMessage persistent_message =
      CreatePersistentMessage(message, std::nullopt, updated_tab,
                              PersistentNotificationType::UNDEFINED);

  if (dirty_type == DirtyType::kNone) {
    NotifyHidePersistentMessagesForTypes(
        persistent_message, {PersistentNotificationType::CHIP,
                             PersistentNotificationType::DIRTY_TAB});
  } else {
    std::vector<PersistentNotificationType> persistent_notification_types(
        {PersistentNotificationType::CHIP});
    if (!is_selected) {
      persistent_notification_types.emplace_back(
          PersistentNotificationType::DIRTY_TAB);
    }
    // For remote updates, show the message on UI.
    NotifyDisplayPersistentMessagesForTypes(persistent_message,
                                            persistent_notification_types);
  }

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           updated_tab.saved_group_guid());

  if (dirty_type != DirtyType::kNone && is_selected &&
      instant_message_processor_->IsEnabled()) {
    InstantMessage instant_message_base;
    auto message_attribution = CreateMessageAttributionForTabUpdates(
        message, std::nullopt, updated_tab);
    message_attribution.tab_metadata->previous_url = before.url().spec();
    instant_message_base.attributions.emplace_back(message_attribution);

    instant_message_base.collaboration_event = CollaborationEvent::TAB_UPDATED;
    // TODO(crbug.com/391941212): CONFLICT_TAB_REMOVED and UNDEFINED don't seem
    // to be used. In that case, remove them.
    instant_message_base.type = InstantNotificationType::UNDEFINED;
    instant_message_base.localized_message =
        GetTitleForTabUpdatedMessage(instant_message_base);

    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message_base,
                          {InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnTabSelectionChanged(
    const tab_groups::LocalTabID& tab_id,
    bool is_selected) {
  std::optional<tab_groups::SavedTabGroupTab> tab;
  std::optional<data_sharing::GroupId> collaboration_group_id;
  for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
    if (group.is_shared_tab_group() && group.GetTab(tab_id)) {
      tab = *(group.GetTab(tab_id));
      collaboration_group_id =
          data_sharing::GroupId(group.collaboration_id().value().value());
      break;
    }
  }

  if (!tab) {
    return;
  }

  if (!is_selected && !configuration_.clear_chip_on_tab_selection) {
    CHECK(collaboration_group_id);
    // When we do not clear the chip on selection, we instead clear the chip
    // when a user selects a different tab after having selected one.
    store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                    tab->saved_tab_guid(), DirtyType::kChip);

    // Specialized handling of creating a PersistentMessage, since we do not
    // have a stored collaboration_pb::Message available.
    PersistentMessage persistent_message =
        CreatePersistentMessageFromTabGroupAndTab(
            *collaboration_group_id, *tab, CollaborationEvent::UNDEFINED);

    NotifyHidePersistentMessagesForTypes(persistent_message,
                                         {PersistentNotificationType::CHIP});
  }

  if (!is_selected) {
    return;
  }

  if (configuration_.clear_chip_on_tab_selection) {
    store_->ClearDirtyMessageForTab(
        *collaboration_group_id, tab->saved_tab_guid(), DirtyType::kDotAndChip);
  } else {
    store_->ClearDirtyMessageForTab(*collaboration_group_id,
                                    tab->saved_tab_guid(), DirtyType::kDot);
  }

  PersistentMessage persistent_message =
      CreatePersistentMessageFromTabGroupAndTab(*collaboration_group_id, *tab,
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
                                           tab->saved_group_guid());
}

void MessagingBackendServiceImpl::OnTabLastSeenTimeChanged(
    const base::Uuid& tab_id,
    tab_groups::TriggerSource source) {
  // Only remote changes need to update the notification states.
  if (source != tab_groups::TriggerSource::REMOTE) {
    return;
  }

  std::optional<tab_groups::SavedTabGroupTab> tab;
  std::optional<data_sharing::GroupId> collaboration_group_id;
  for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
    if (group.is_shared_tab_group() && group.GetTab(tab_id)) {
      tab = *(group.GetTab(tab_id));
      collaboration_group_id =
          data_sharing::GroupId(group.collaboration_id().value().value());
      break;
    }
  }

  if (!tab) {
    return;
  }

  if (!HasSeenTabUpdate(*tab)) {
    return;
  }

  store_->ClearDirtyMessageForTab(
      *collaboration_group_id, tab->saved_tab_guid(), DirtyType::kDotAndChip);

  // Hide any existing persistent dot or chip messages already showing.
  PersistentMessage persistent_message =
      CreatePersistentMessageFromTabGroupAndTab(*collaboration_group_id, *tab,
                                                CollaborationEvent::UNDEFINED);
  NotifyHidePersistentMessagesForTypes(persistent_message,
                                       {PersistentNotificationType::CHIP,
                                        PersistentNotificationType::DIRTY_TAB});

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           tab->saved_group_guid());
}

void MessagingBackendServiceImpl::OnTabGroupOpened(
    const tab_groups::SavedTabGroup& tab_group) {
  std::optional<data_sharing::GroupId> collaboration_group_id =
      GroupIdForTabGroup(tab_group);
  if (!collaboration_group_id) {
    return;
  }

  // Redeliver instant messages for the open group.
  for (auto& message : store_->GetDirtyMessagesForGroup(
           *collaboration_group_id, DirtyType::kMessageOnly)) {
    InstantMessage instant_message =
        CreateInstantMessage(message, tab_group, /*tab=*/std::nullopt);
    DisplayInstantMessage(base::Uuid::ParseLowercase(message.uuid()),
                          instant_message, {InstantNotificationLevel::BROWSER});
  }

  // Show all the persistent messages in the group.
  std::vector<PersistentMessage> messages = GetMessagesForGroup(
      tab_group.saved_guid(), PersistentNotificationType::DIRTY_TAB);
  for (auto& message : messages) {
    NotifyDisplayPersistentMessagesForTypes(
        message, {PersistentNotificationType::CHIP,
                  PersistentNotificationType::DIRTY_TAB});
  }

  DisplayOrHideTabGroupDirtyDotForTabGroup(*collaboration_group_id,
                                           tab_group.saved_guid());
}

void MessagingBackendServiceImpl::OnTabGroupClosed(
    const tab_groups::SavedTabGroup& tab_group) {
  // TODO(crbug.com/389948628): Handle hide persistence messages if needed.
}

void MessagingBackendServiceImpl::OnGroupMemberAdded(
    const data_sharing::GroupData& group_data,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if (IsMemberSelfOrOwner(identity_manager_, group_data, member_gaia_id)) {
    return;
  }

  std::optional<tab_groups::SavedTabGroup> tab_group =
      GetTabGroupFromCollaborationId(group_data.group_token.group_id.value());
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
  store_->AddMessage(message);

  if (instant_message_processor_->IsEnabled()) {
    InstantMessage instant_message =
        CreateInstantMessage(message, tab_group, /*tab=*/std::nullopt);
    instant_message.localized_message =
        GetTitleForMemberAddedMessage(instant_message);
    DisplayInstantMessage(
        base::Uuid::ParseLowercase(message.uuid()), instant_message,
        {InstantNotificationLevel::SYSTEM, InstantNotificationLevel::BROWSER});
  }
}

void MessagingBackendServiceImpl::OnGroupMemberRemoved(
    const data_sharing::GroupData& group_data,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if (IsMemberSelfOrOwner(identity_manager_, group_data, member_gaia_id)) {
    return;
  }

  collaboration_pb::Message message =
      CreateMessage(group_data.group_token.group_id,
                    collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                    DirtyType::kNone, event_time);
  message.set_affected_user_gaia_id(member_gaia_id.ToString());
  store_->AddMessage(message);
}

// static
std::u16string MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
    const std::u16string& original_title) {
  return TruncateTabTitle(original_title);
}

void MessagingBackendServiceImpl::ClearPersistentMessage(
    const base::Uuid& message_id,
    PersistentNotificationType type) {
  store_->ClearDirtyMessage(
      message_id, GetDirtyTypeFromPersistentNotificationTypeForQuery(type));
}

void MessagingBackendServiceImpl::RemoveMessages(
    const std::vector<base::Uuid>& message_ids) {
  std::set<std::string> message_uuids;
  for (const base::Uuid& message_id : message_ids) {
    message_uuids.insert(message_id.AsLowercaseString());
  }
  store_->RemoveMessages(message_uuids);
}

void MessagingBackendServiceImpl::AddActivityLogForTesting(
    data_sharing::GroupId collaboration_id,
    const std::vector<ActivityLogItem>& activity_log) {
  CHECK_IS_TEST();
  activity_log_for_testing_.emplace(collaboration_id, activity_log);
}

std::optional<std::string>
MessagingBackendServiceImpl::GetDisplayNameForUserInGroup(
    const data_sharing::GroupId& group_id,
    const GaiaId& gaia_id) {
  std::optional<data_sharing::GroupMemberPartialData> group_member_data =
      data_sharing_service_->GetPossiblyRemovedGroupMember(group_id, gaia_id);
  // Try given name from live data first.
  if (group_member_data && !group_member_data->given_name.empty()) {
    return group_member_data->given_name;
  }

  // Then try display name from live data.
  if (group_member_data && !group_member_data->display_name.empty()) {
    return group_member_data->display_name;
  }

  return std::nullopt;
}

int GetTitleStringRes(CollaborationEvent collaboration_event,
                      bool is_tab_activity) {
  switch (collaboration_event) {
    case CollaborationEvent::TAB_ADDED:
      return is_tab_activity
                 ? IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_ADDED_THIS_TAB
                 : IDS_DATA_SHARING_RECENT_ACTIVITY_TAB_ADDED;
    case CollaborationEvent::TAB_REMOVED:
      return IDS_DATA_SHARING_RECENT_ACTIVITY_TAB_REMOVED;
    case CollaborationEvent::TAB_UPDATED:
      return is_tab_activity
                 ? IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_THIS_TAB
                 : IDS_DATA_SHARING_RECENT_ACTIVITY_TAB_UPDATED;
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
      return IDS_DATA_SHARING_RECENT_ACTIVITY_TAB_GROUP_NAME_UPDATED;
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      return IDS_DATA_SHARING_RECENT_ACTIVITY_TAB_GROUP_COLOR_UPDATED;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      return IDS_DATA_SHARING_RECENT_ACTIVITY_USER_JOINED_GROUP;
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return IDS_DATA_SHARING_RECENT_ACTIVITY_USER_LEFT_GROUP;
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::UNDEFINED:
      CHECK(false) << "No string res for collaboration event "
                   << static_cast<size_t>(collaboration_event);
  }
  return 0;
}

std::optional<ActivityLogItem>
MessagingBackendServiceImpl::ConvertMessageToActivityLogItem(
    const collaboration_pb::Message& message,
    bool is_tab_activity) {
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

  std::optional<std::string> user_name_for_display;
  if (gaia_id) {
    user_name_for_display =
        GetDisplayNameForUserInGroup(collaboration_group_id, *gaia_id);
  }

  bool is_self =
      gaia_id.has_value() && IsMemberCurrentUser(identity_manager_, *gaia_id);
  std::u16string user_to_show =
      is_self ? l10n_util::GetStringUTF16(
                    IDS_DATA_SHARING_RECENT_ACTIVITY_USER_SELF)
              : (user_name_for_display
                     ? base::UTF8ToUTF16(*user_name_for_display)
                     : l10n_util::GetStringUTF16(
                           IDS_DATA_SHARING_RECENT_ACTIVITY_UNKNOWN_USER));

  item.title_text = l10n_util::GetStringFUTF16(
      GetTitleStringRes(item.collaboration_event, is_tab_activity),
      user_to_show);

  // By default, we use an empty description. This is special cased below.
  item.description_text = u"";
  base::TimeDelta time_delta =
      base::Time::Now() - base::Time::FromTimeT(message.event_timestamp());
  item.time_delta_text = GetElapsedTimeText(time_delta);
  item.action =
      GetRecentActivityActionFromCollaborationEvent(item.collaboration_event);

  item.activity_metadata = MessageAttribution();
  item.activity_metadata.id = base::Uuid::ParseLowercase(message.uuid());
  item.activity_metadata.collaboration_id = collaboration_group_id;

  std::optional<tab_groups::SavedTabGroup> tab_group =
      GetTabGroupFromCollaborationId(message.collaboration_id());
  item.activity_metadata.tab_group_metadata =
      CreateTabGroupMessageMetadataFromMessageOrTabGroup(message, tab_group);

  // The code below needs to fill in `activity_metadata`, and optionally
  // `show_favicon` if it is true.
  switch (GetMessageCategory(message)) {
    case MessageCategory::kTab: {
      item.show_favicon = true;
      item.activity_metadata.tab_metadata =
          CreateTabMessageMetadataFromMessageOrTab(
              message, GetTabFromGroup(message, tab_group));
      // We are guaranteed to have a value for `last_known_url`.
      GURL url = GURL(*item.activity_metadata.tab_metadata->last_known_url);
      item.activity_metadata.triggering_user = group_member;
      item.activity_metadata.triggering_user_is_self = is_self;

      item.description_text =
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url);

      break;
    }
    case MessageCategory::kTabGroup: {
      item.activity_metadata.triggering_user = group_member;
      item.activity_metadata.triggering_user_is_self = is_self;

      // Only tab group name changes have specialized description.
      if (message.event_type() == collaboration_pb::TAB_GROUP_NAME_UPDATED) {
        if (item.activity_metadata.tab_group_metadata->last_known_title) {
          item.description_text = base::UTF8ToUTF16(
              *item.activity_metadata.tab_group_metadata->last_known_title);
        }
      }

      break;
    }
    case MessageCategory::kCollaboration:
      item.activity_metadata.affected_user = group_member;
      if (group_member) {
        item.description_text = base::UTF8ToUTF16(group_member->email);
      }
      item.activity_metadata.affected_user_is_self = is_self;
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
    const collaboration_pb::Message& message,
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
  } else {
    tab_group_metadata.last_known_title = message.tab_group_data().title();
  }
  // TODO(crbug.com/395918345): Should we always use title from DB and ignore
  // the tab group?
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
      message, GetTabGroupFromCollaborationId(message.collaboration_id()),
      data_sharing::GroupId(message.collaboration_id()));
}

std::optional<tab_groups::SavedTabGroup>
MessagingBackendServiceImpl::GetTabGroupFromCollaborationId(
    const std::string& collaboration_id) {
  if (collaboration_id.empty()) {
    return std::nullopt;
  }

  syncer::CollaborationId collaboration_group_id(collaboration_id);
  for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
    if (group.collaboration_id().has_value() &&
        group.collaboration_id().value() == collaboration_group_id) {
      return group;
    }
  }

  return std::nullopt;
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
    PersistentNotificationType type) {
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
    PersistentNotificationType type,
    bool allow_dirty_tab_group_message) {
  std::vector<PersistentMessage> persistent_messages;
  std::optional<tab_groups::SavedTabGroup> tab_group =
      GetTabGroupFromCollaborationId(message.collaboration_id());

  // Special case: First handle if it's of type TOMBSTONED.
  bool has_tombstoned =
      message.dirty() & static_cast<int>(DirtyType::kTombstoned);
  bool looking_for_tombstoned = lookup_dirty_type == DirtyType::kAll ||
                                lookup_dirty_type == DirtyType::kTombstoned;
  if (has_tombstoned && looking_for_tombstoned) {
    persistent_messages.push_back(
        CreatePersistentMessage(message, tab_group, std::nullopt,
                                PersistentNotificationType::TOMBSTONED));
    return persistent_messages;
  }

  // Rest of the persistent messages must be related to tabs.
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
  bool add_dirty_tab_messages = type == PersistentNotificationType::UNDEFINED ||
                                type == PersistentNotificationType::DIRTY_TAB;
  bool add_dirty_tab_group_messages =
      allow_dirty_tab_group_message &&
      (type == PersistentNotificationType::UNDEFINED ||
       type == PersistentNotificationType::DIRTY_TAB_GROUP);
  bool has_dirty_tab_messages_in_group =
      !store_
           ->GetDirtyMessagesForGroup(
               data_sharing::GroupId(message.collaboration_id()),
               DirtyType::kDot)
           .empty();

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
    PersistentNotificationType type) {
  PersistentMessage persistent_message;
  persistent_message.collaboration_event =
      ToCollaborationEvent(message.event_type());
  persistent_message.attribution =
      CreateMessageAttributionForTabUpdates(message, tab_group, tab);
  persistent_message.type = type;
  return persistent_message;
}

InstantMessage MessagingBackendServiceImpl::CreateInstantMessage(
    const collaboration_pb::Message& message,
    const std::optional<tab_groups::SavedTabGroup>& tab_group,
    const std::optional<tab_groups::SavedTabGroupTab>& tab) {
  InstantMessage instant_message;
  instant_message.collaboration_event =
      ToCollaborationEvent(message.event_type());
  instant_message.attributions.emplace_back(
      CreateMessageAttributionForTabUpdates(message, tab_group, tab));
  return instant_message;
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
  // We leave the attribution.id field empty for synthetic messages, as we don't
  // have a corresponding stored collaboration_pb::Message available.
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
  attribution.id = base::Uuid::ParseLowercase(message.uuid());

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

  // Look for the member in the provided data.
  auto group_data =
      data_sharing_service_->ReadGroup(attribution.collaboration_id);

  if (group_data) {
    // Set affected user if available.
    if (message.has_affected_user_gaia_id()) {
      GaiaId gaia_id(message.affected_user_gaia_id());
      for (const data_sharing::GroupMember& member :
           group_data.value().members) {
        if (member.gaia_id == gaia_id) {
          attribution.affected_user = member;
          attribution.affected_user_is_self =
              IsMemberCurrentUser(identity_manager_, gaia_id);
          break;
        }
      }
    }

    // Set triggering user if available.
    if (message.has_triggering_user_gaia_id()) {
      for (const data_sharing::GroupMember& member :
           group_data.value().members) {
        GaiaId gaia_id(message.triggering_user_gaia_id());
        if (member.gaia_id == gaia_id) {
          attribution.triggering_user = member;
          attribution.triggering_user_is_self =
              IsMemberCurrentUser(identity_manager_, gaia_id);
          break;
        }
      }
    }
  }

  return attribution;
}

void MessagingBackendServiceImpl::DisplayInstantMessage(
    const base::Uuid& db_message_uuid,
    const InstantMessage& base_message,
    const std::vector<InstantNotificationLevel>& levels) {
  for (InstantNotificationLevel level : levels) {
    InstantMessage instant_message = base_message;
    instant_message.level = level;
    instant_message_processor_->DisplayInstantMessage(instant_message);
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
  // We leave the attribution.id field empty for synthetic messages, as we don't
  // have a corresponding stored collaboration_pb::Message available.

  persistent_message.attribution.collaboration_id = collaboration_group_id;
  if (tab_group) {
    persistent_message.attribution.tab_group_metadata =
        CreateTabGroupMessageMetadata(*tab_group);
  }
  persistent_message.attribution.tab_metadata = CreateTabMessageMetadata(tab);
  return persistent_message;
}

}  // namespace collaboration::messaging
