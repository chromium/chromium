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
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

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

 private:
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

  // Whether the TabGroupChangeNotifier has been initialized.
  bool tab_group_change_notifier_initialized_ = false;

  // Whether the DataSharingChangeNotifier has been initialized.
  bool data_sharing_change_notifier_initialized_ = false;

  // Service providing information about tabs and tab groups.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about people groups.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // The single delegate for when we need to inform the UI about instant
  // (one-off) messages.
  raw_ptr<InstantMessageDelegate> instant_message_delegate_;

  // The list of observers for any changes to persistent messages.
  base::ObserverList<PersistentMessageObserver> persistent_message_observers_;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
