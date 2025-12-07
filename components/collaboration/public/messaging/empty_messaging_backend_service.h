// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_EMPTY_MESSAGING_BACKEND_SERVICE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_EMPTY_MESSAGING_BACKEND_SERVICE_H_

#include <memory>
#include <vector>

#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging {
// An empty implementation of the MessagingBackendService.
class EmptyMessagingBackendService : public MessagingBackendService {
 public:
  EmptyMessagingBackendService();
  ~EmptyMessagingBackendService() override;

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
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_EMPTY_MESSAGING_BACKEND_SERVICE_H_
