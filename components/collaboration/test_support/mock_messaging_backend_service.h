// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_MESSAGING_BACKEND_SERVICE_H_
#define COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_MESSAGING_BACKEND_SERVICE_H_

#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace collaboration::messaging {

// A mock implementation of MessagingBackendService, for use in tests.
class MockMessagingBackendService : public MessagingBackendService {
 public:
  MockMessagingBackendService();
  ~MockMessagingBackendService() override;

  // MessagingBackendService implementation.
  MOCK_METHOD(void, SetInstantMessageDelegate, (InstantMessageDelegate*));
  MOCK_METHOD(void, AddPersistentMessageObserver, (PersistentMessageObserver*));
  MOCK_METHOD(void,
              RemovePersistentMessageObserver,
              (PersistentMessageObserver*));
  MOCK_METHOD(bool, IsInitialized, ());
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForTab,
              (tab_groups::EitherTabID, PersistentNotificationType));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForGroup,
              (tab_groups::EitherGroupID, PersistentNotificationType));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessages,
              (PersistentNotificationType));
  MOCK_METHOD(std::vector<ActivityLogItem>,
              GetActivityLog,
              (const ActivityLogQueryParams&));
  MOCK_METHOD(void,
              ClearDirtyTabMessagesForGroup,
              (const data_sharing::GroupId&));
  MOCK_METHOD(void,
              ClearPersistentMessage,
              (const base::Uuid&, PersistentNotificationType));
  MOCK_METHOD(void, RemoveMessages, (const std::vector<base::Uuid>&));
  MOCK_METHOD(void,
              AddActivityLogForTesting,
              (data_sharing::GroupId, const std::vector<ActivityLogItem>&));
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_MESSAGING_BACKEND_SERVICE_H_
