// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_MESSAGING_BACKEND_SERVICE_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace collaboration::messaging {

// The implementation of the MessagingBackendService.
class MessagingBackendServiceImpl : public MessagingBackendService {
 public:
  MessagingBackendServiceImpl(
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

 private:
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
