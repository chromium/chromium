// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/collaboration/internal/messaging/configuration.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

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
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      data_sharing::DataSharingService* data_sharing_service);
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
      std::optional<PersistentNotificationType> type) override;
  std::vector<PersistentMessage> GetMessagesForGroup(
      tab_groups::EitherGroupID group_id,
      std::optional<PersistentNotificationType> type) override;
  std::vector<PersistentMessage> GetMessages(
      std::optional<PersistentNotificationType> type) override;
  std::vector<ActivityLogItem> GetActivityLog(
      const ActivityLogQueryParams& params) override;

  // TabGroupChangeNotifier::Observer.
  void OnTabGroupChangeNotifierInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& added_group) override;
  void OnTabGroupRemoved(tab_groups::SavedTabGroup removed_group) override;
  void OnTabGroupNameUpdated(
      const tab_groups::SavedTabGroup& updated_group) override;
  void OnTabGroupColorUpdated(
      const tab_groups::SavedTabGroup& updated_group) override;
  void OnTabAdded(const tab_groups::SavedTabGroupTab& added_tab) override;
  void OnTabRemoved(tab_groups::SavedTabGroupTab removed_tab) override;
  void OnTabUpdated(const tab_groups::SavedTabGroupTab& updated_tab) override;
  void OnTabSelected(
      std::optional<tab_groups::SavedTabGroupTab> selected_tab) override;

  // DataSharingChangeNotifier::Observer.
  void OnDataSharingChangeNotifierInitialized() override;
  void OnGroupAdded(const data_sharing::GroupId& group_id,
                    const std::optional<data_sharing::GroupData>& group_data,
                    const base::Time& event_time) override;
  void OnGroupRemoved(const data_sharing::GroupId& group_id,
                      const std::optional<data_sharing::GroupData>& group_data,
                      const base::Time& event_time) override;
  void OnGroupMemberAdded(const data_sharing::GroupData& group_data,
                          const GaiaId& member_gaia_id,
                          const base::Time& event_time) override;
  void OnGroupMemberRemoved(const data_sharing::GroupData& group_data,
                            const GaiaId& member_gaia_id,
                            const base::Time& event_time) override;

 private:
  void OnStoreInitialized(bool success);

  // We need to be able to find the currently selected tab on startup so we know
  // what changed in OnTabSelected.
  void SetCurrentlySelectedTabOnStartup();

  // Uses all available sources to try to retrieve a name that describes the
  // given user.
  std::optional<std::string> GetDisplayNameForUserInGroup(
      const data_sharing::GroupId& group_id,
      const GaiaId& gaia_id,
      const std::optional<data_sharing::GroupData>& group_data,
      const std::optional<collaboration_pb::Message>& db_message);

  // Converts a stored message to an ActivityLogItem for display. Some events
  // should not be part of the activity log and for those std::nullopt is
  // return.
  std::optional<ActivityLogItem> ConvertMessageToActivityLogItem(
      const collaboration_pb::Message& message);

  // Looks for the related collaboration GroupId for the given tab, using the
  // information available in the tab group sync service.
  std::optional<data_sharing::GroupId> GetCollaborationGroupIdForTab(
      const tab_groups::SavedTabGroupTab& tab);

  // Uses the provided data to create TabGroupMessageMetadata.
  TabGroupMessageMetadata CreateTabGroupMessageMetadataFromCollaborationId(
      std::optional<tab_groups::SavedTabGroup> tab_group,
      std::optional<data_sharing::GroupId> collaboration_group_id);

  // Creates a TabGroupMessageMetadata based on the sources given as input.
  TabGroupMessageMetadata CreateTabGroupMessageMetadataFromMessageOrTabGroup(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group);

  // Tries to retrieve the correct tab group based on data in the Message.
  std::optional<tab_groups::SavedTabGroup> GetTabGroupFromMessage(
      const collaboration_pb::Message& message);

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
      const std::optional<PersistentNotificationType>& type);

  // Convert a single stored Message to PersistentMessages. Each stored message
  // may result in multiple PersistentMessages, e.g. both CHIP and DIRTY_TAB.
  std::vector<PersistentMessage> ConvertMessageToPersistentMessages(
      const collaboration_pb::Message& message,
      DirtyType lookup_dirty_type,
      const std::optional<PersistentNotificationType>& type,
      bool allow_dirty_tab_group_message);

  // Creates a PersistentMessage based on the provided information.
  PersistentMessage CreatePersistentMessage(
      const collaboration_pb::Message& message,
      const std::optional<tab_groups::SavedTabGroup>& tab_group,
      const std::optional<tab_groups::SavedTabGroupTab>& tab,
      const std::optional<PersistentNotificationType>& type);

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

  // The last tab the user selected, or `std::nullopt` if it was outside a
  // shared tab group.
  std::optional<tab_groups::SavedTabGroupTab> last_selected_tab_;

  // Service providing information about tabs and tab groups.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about people groups.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // The single delegate for when we need to inform the UI about instant
  // (one-off) messages.
  raw_ptr<InstantMessageDelegate> instant_message_delegate_;

  // The list of observers for any changes to persistent messages.
  base::ObserverList<PersistentMessageObserver> persistent_message_observers_;

  base::WeakPtrFactory<MessagingBackendServiceImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
