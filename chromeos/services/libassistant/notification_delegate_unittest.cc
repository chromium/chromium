// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "chromeos/services/libassistant/test_support/libassistant_service_tester.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace libassistant {

namespace {

class NotificationDelegateMock : public mojom::NotificationDelegate {
 public:
  NotificationDelegateMock() = default;
  NotificationDelegateMock(const NotificationDelegateMock&) = delete;
  NotificationDelegateMock& operator=(const NotificationDelegateMock&) = delete;
  ~NotificationDelegateMock() override = default;

  // mojom::NotificationDelegate implementation:
  MOCK_METHOD(void, RemoveAllNotifications, (bool from_server));
  MOCK_METHOD(void,
              RemoveNotificationByGroupingKey,
              (const std::string& grouping_key, bool from_server));

  void Bind(
      mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::NotificationDelegate> receiver_{this};
};

}  // namespace

class NotificationDelegateTest : public ::testing::Test {
 public:
  NotificationDelegateTest() = default;
  NotificationDelegateTest(const NotificationDelegateTest&) = delete;
  NotificationDelegateTest& operator=(const NotificationDelegateTest&) = delete;
  ~NotificationDelegateTest() override = default;

  void SetUp() override {
    delegate_mock_.Bind(
        service_tester_.GetNotificationDelegatePendingReceiver());
    service_tester_.Start();
  }

  assistant_client::AssistantManagerDelegate& assistant_manager_delegate() {
    return *service_tester_.assistant_manager_internal()
                .assistant_manager_delegate();
  }

  NotificationDelegateMock& delegate_mock() { return delegate_mock_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<NotificationDelegateMock> delegate_mock_;
  LibassistantServiceTester service_tester_;
};

TEST_F(NotificationDelegateTest, ShouldInvokeRemoveAllNotifications) {
  EXPECT_CALL(delegate_mock(), RemoveAllNotifications(/*from_server=*/true));

  // Pass in empty |grouping_key| should trigger all notifications being
  // removed.
  assistant_manager_delegate().OnNotificationRemoved(/*grouping_key=*/"");
  delegate_mock().FlushForTesting();
}

TEST_F(NotificationDelegateTest, ShouldInvokeRemoveNotificationByGroupingKey) {
  const std::string grouping_id = "grouping-id";
  EXPECT_CALL(delegate_mock(), RemoveNotificationByGroupingKey(
                                   /*id=*/grouping_id, /*from_server=*/true));

  // Pass in non-empty |grouping_key| will trigger specific group of
  // notifications being removed.
  assistant_manager_delegate().OnNotificationRemoved(grouping_id);
  delegate_mock().FlushForTesting();
}

}  // namespace libassistant
}  // namespace chromeos
