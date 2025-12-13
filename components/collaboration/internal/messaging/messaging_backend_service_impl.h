// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/collaboration/internal/messaging/configuration.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/instant_message_processor.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace collaboration_pb {
class Message;
}  // namespace collaboration_pb

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace collaboration::messaging {
class DataSharingChangeNotifier;
class MessagingBackendStore;

// The implementation of the MessagingBackendService.
class MessagingBackendServiceImpl : public MessagingBackendService,
                                    public TabGroupChangeNotifier::Observer,
                                    public DataSharingChangeNotifier::Observer {
 public:
  MessagingBackendServiceImpl(
      const MessagingBackendConfiguration& configuration,
      std::unique_ptr<TabGroupChangeNotifier> tab_group_change_notifier,
      std::unique_ptr<DataSharingChangeNotifier> data_sharing_change_notifier,
      std::unique_ptr<MessagingBackendStore> messaging_backend_store,
      std::unique_ptr<InstantMessageProcessor> instant_message_processor,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      data_sharing::DataSharingService* data_sharing_service,
      signin::IdentityManager* identity_manager);
  ~MessagingBackendServiceImpl() override;

  // MessagingBackendService implementation.
  void SetInstantMessageDelegate(
      InstantMessageDelegate* instant_message_delegate) override;
  void AddPersistentMessageObserver(
      PersistentMessageObserver* observer) override;
  void RemovePersistentMessageObserver(
      PersistentMessageObserver* observer) override;
  bool IsInitialized() override;
  std::vector<PersistentMessage> GetMessagesForTab(
      tab_groups::EitherTabID tab_id,
      PersistentNotificationType type) override;
  std::vector<PersistentMessage> GetMessagesForGroup(
      tab_groups::EitherGroupID group_id,
      PersistentNotificationType type) override;
  std::vector<PersistentMessage> GetMessages(
      PersistentNotificationType type) override;
  std::vector<ActivityLogItem> GetActivityLog(
      const ActivityLogQueryParams& params) override;
  void ClearDirtyTabMessagesForGroup(
      const data_sharing::GroupId& collaboration_group_id) override;
  void ClearPersistentMessage(const base::Uuid& message_id,
                              PersistentNotificationType type) override;
  void RemoveMessages(const std::vector<base::Uuid>& message_ids) override;
  void AddActivityLogForTesting(
      data_sharing::GroupId collaboration_id,
      const std::vector<ActivityLogItem>& activity_log) override;

  // TabGroupChangeNotifier::Observer.
  void OnTabGroupChangeNotifierInitialized() override;
  void OnSyncDisabled() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& added_group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(tab_groups::SavedTabGroup removed_group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupNameUpdated(const tab_groups::SavedTabGroup& updated_group,
                             tab_groups::TriggerSource source) override;
  void OnTabGroupColorUpdated(const tab_groups::SavedTabGroup& updated_group,
                              tab_groups::TriggerSource source) override;
  void OnTabAdded(const tab_groups::SavedTabGroupTab& added_tab,
                  tab_groups::TriggerSource source) override;
  void OnTabRemoved(tab_groups::SavedTabGroupTab removed_tab,
                    tab_groups::TriggerSource source,
                    bool is_selected) override;
  void OnTabUpdated(const tab_groups::SavedTabGroupTab& before,
                    const tab_groups::SavedTabGroupTab& after,
                    tab_groups::TriggerSource source,
                    bool is_selected) override;
  void OnTabSelectionChanged(const tab_groups::LocalTabID& tab_id,
                             bool is_selected) override;
  void OnTabLastSeenTimeChanged(const base::Uuid& tab_id,
                                tab_groups::TriggerSource source) override;
  void OnTabGroupOpened(const tab_groups::SavedTabGroup& tab_group) override;
  void OnTabGroupClosed(const tab_groups::SavedTabGroup& tab_group) override;

  // DataSharingChangeNotifier::Observer.
  void OnDataSharingChangeNotifierInitialized() override;
  void OnGroupMemberAdded(const data_sharing::GroupData& group_data,
                          const GaiaId& member_gaia_id,
                          const base::Time& event_time) override;
  void OnGroupMemberRemoved(const data_sharing::GroupData& group_data,
                            const GaiaId& member_gaia_id,
                            const base::Time& event_time) override;

  static std::u16string GetTruncatedTabTitleForTesting(
      const std::u16string& original_title);

 private:
  void OnStoreInitialized(bool success);

  void ClearDirtyTabMessagesForGroup(
      const data_sharing::GroupId& collaboration_group_id,
      const std::optional<tab_groups::SavedTabGroup>& tab_group);

  // Uses all available sources to try to retrieve a name that describes the
  // given user.
  std::optional<std::string> GetDisplayNameForUserInGroup(
      const data_sharing::GroupId& group_id,
      const GaiaId& gaia_id);

  // Converts a stored message to an ActivityLogItem for display. Some events
  // should not be part of the activity log and for those std::nullopt is
  // return.
  std::optional<ActivityLogItem> ConvertMessageToActivityLogItem(
      const collaboration_pb::Message& message,
      bool is_tab_activity);

  // Looks for the related collaboration GroupId for the given tab, using the
  // information available in the tab group sync service.
  std::optional<data_sharing::GroupId> GetCollaborationGroupIdForTab(
      const tab_groups::SavedTabGroupTab& tab);

  // Uses the provided data to create TabGroupMessageMetadata.
  TabGroupMessageMetadata CreateTabGroupMessageMetadataFromCollaborationId(
      const collaboration_pb::Message& message,
      std::optional<tab_groups::SavedTabGroup> tab_group,
      std::optional<data_sharing::GroupId> collaboration_group_id);

  // Creates a TabGroupMessageMetadata based on the sources given as input.
  TabGroupMessageMetadata CreateTabGroupMessageMetadataFromMessageOrTabGroup(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group);

  // Tries to retrieve the tab group from CollaborationId.
  std::optional<tab_groups::SavedTabGroup> GetTabGroupFromCollaborationId(
      const std::string& collaboration_id);

  // Uses the available data to look up a GroupMember.
  std::optional<data_sharing::GroupMember> GetGroupMemberFromGaiaId(
      const data_sharing::GroupId& collaboration_group_id,
      std::optional<GaiaId> gaia_id);

  // Retrieves the relevant tab group from the TabGroupSyncService and looks up
  // its collaboration group id.
  std::optional<data_sharing::GroupId> GetCollaborationGroupId(
      tab_groups::EitherGroupID group_id);

  // Looks up the tab from tab group sync service.
  std::optional<tab_groups::SavedTabGroupTab> GetTabFromTabId(
      tab_groups::EitherTabID tab_id);

  // Convert all the provided stored Messages to PersistentMessages.
  std::vector<PersistentMessage> ConvertMessagesToPersistentMessages(
      const std::vector<collaboration_pb::Message>& messages,
      DirtyType lookup_dirty_type,
      PersistentNotificationType type);

  // Convert a single stored Message to PersistentMessages. Each stored message
  // may result in multiple PersistentMessages, e.g. both CHIP and DIRTY_TAB.
  std::vector<PersistentMessage> ConvertMessageToPersistentMessages(
      const collaboration_pb::Message& message,
      DirtyType lookup_dirty_type,
      PersistentNotificationType type,
      bool allow_dirty_tab_group_message);

  // Creates a PersistentMessage based on the provided information.
  PersistentMessage CreatePersistentMessage(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group,
      const std::optional<tab_groups::SavedTabGroupTab>& tab,
      PersistentNotificationType type);

  InstantMessage CreateInstantMessage(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group,
      const std::optional<tab_groups::SavedTabGroupTab>& tab);

  // Creates individual messages based on `base_message` per type, and notifies
  // oservers to display the messages.
  void NotifyDisplayPersistentMessagesForTypes(
      const PersistentMessage& base_message,
      const std::vector<PersistentNotificationType>& types);

  // Creates individual messages based on `base_message` per type, and notifies
  // oservers to hide the messages.
  void NotifyHidePersistentMessagesForTypes(
      const PersistentMessage& base_message,
      const std::vector<PersistentNotificationType>& types);

  // Notifies observers to display or hide the dirty dot for a tab group.
  void DisplayOrHideTabGroupDirtyDotForTabGroup(
      const data_sharing::GroupId& collaboration_group_id,
      base::Uuid shared_tab_group_id);

  // Creates MessageAttribution based on all the provided information.
  MessageAttribution CreateMessageAttributionForTabUpdates(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group,
      const std::optional<tab_groups::SavedTabGroupTab>& tab);

  // Notifies the InstantMessageDelegate to display the message for all the
  // provided levels.
  void DisplayInstantMessage(
      const base::Uuid& db_message_uuid,
      const InstantMessage& base_message,
      const std::vector<InstantNotificationLevel>& levels);

  // Clears the dirty bit for the given DB message ID if `success` is true.
  void ClearMessageDirtyBit(base::Uuid db_message_id, bool success);

  // Creates a PersistentMessage based on tab group and tab for when we do not
  // have a db Message available.
  PersistentMessage CreatePersistentMessageFromTabGroupAndTab(
      const data_sharing::GroupId& collaboration_group_id,
      const tab_groups::SavedTabGroupTab tab,
      CollaborationEvent collaboration_event);

  // A configuration for how the MessagingBackendService should behave.
  MessagingBackendConfiguration configuration_;

  // Provides functionality to go from observing the TabGroupSyncService to
  // a delta based observer API.
  std::unique_ptr<TabGroupChangeNotifier> tab_group_change_notifier_;

  // Provides functionality to go from observing the DataSharingService to a
  // smaller API surface and delta observation.
  std::unique_ptr<DataSharingChangeNotifier> data_sharing_change_notifier_;

  // Store for reading and writing messages:
  std::unique_ptr<MessagingBackendStore> store_;

  // Scoped observers for our delta change notifiers.
  base::ScopedObservation<TabGroupChangeNotifier,
                          TabGroupChangeNotifier::Observer>
      tab_group_change_notifier_observer_{this};
  base::ScopedObservation<DataSharingChangeNotifier,
                          DataSharingChangeNotifier::Observer>
      data_sharing_change_notifier_observer_{this};

  // Whether initialization has completed.
  bool initialized_ = false;

  // A callback invoked when we are ready to flush all the events from the
  // data sharing service.
  DataSharingChangeNotifier::FlushCallback data_sharing_flush_callback_;

  // Queues and processes instant messages. Invokes the delegate to ask UI to
  // show the instant message.
  std::unique_ptr<InstantMessageProcessor> instant_message_processor_;

  // Service providing information about tabs and tab groups.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about people groups.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // Service providing information about sign in.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The list of observers for any changes to persistent messages.
  base::ObserverList<PersistentMessageObserver> persistent_message_observers_;

  // Test-only mock activity log, keyed by collaboration id.
  std::unordered_map<data_sharing::GroupId, const std::vector<ActivityLogItem>&>
      activity_log_for_testing_;

  base::WeakPtrFactory<MessagingBackendServiceImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
