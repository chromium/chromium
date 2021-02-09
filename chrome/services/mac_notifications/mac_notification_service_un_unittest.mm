// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class MockNotificationActionHandler
    : public notifications::mojom::MacNotificationActionHandler {
 public:
  // notifications::mojom::MacNotificationActionHandler:
  MOCK_METHOD(void,
              OnNotificationAction,
              (notifications::mojom::NotificationActionInfoPtr),
              (override));
};

}  // namespace

class MacNotificationServiceUNTest : public testing::Test {
 public:
  MacNotificationServiceUNTest() {
    if (@available(macOS 10.14, *)) {
      mock_notification_center_ =
          [OCMockObject mockForClass:[UNUserNotificationCenter class]];
      // Expect the MacNotificationServiceUN ctor to register a delegate with
      // the UNNotificationCenter and ask for notification permissions.
      ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/true);
      [[[mock_notification_center_ expect] ignoringNonObjectArgs]
          requestAuthorizationWithOptions:0
                        completionHandler:[OCMArg any]];
      service_ = std::make_unique<MacNotificationServiceUN>(
          service_remote_.BindNewPipeAndPassReceiver(),
          handler_receiver_.BindNewPipeAndPassRemote(),
          mock_notification_center_);
      [mock_notification_center_ verify];
    }
  }

  ~MacNotificationServiceUNTest() override {
    if (@available(macOS 10.14, *)) {
      // Expect the MacNotificationServiceUN dtor to clear the delegate from the
      // UNNotificationCenter.
      ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/false);
      service_.reset();
      [mock_notification_center_ verify];
    }
  }

 protected:
  void ExpectAndUpdateUNUserNotificationCenterDelegate(bool expect_not_nil)
      API_AVAILABLE(macos(10.14)) {
    [[mock_notification_center_ expect]
        setDelegate:[OCMArg checkWithBlock:^BOOL(
                                id<UNUserNotificationCenterDelegate> delegate) {
          EXPECT_EQ(expect_not_nil, delegate != nil);
          notification_center_delegate_ = delegate;
          return YES;
        }]];
  }

  base::test::TaskEnvironment task_environment_;
  MockNotificationActionHandler mock_handler_;
  mojo::Receiver<notifications::mojom::MacNotificationActionHandler>
      handler_receiver_{&mock_handler_};
  mojo::Remote<notifications::mojom::MacNotificationService> service_remote_;
  id mock_notification_center_ = nil;
  API_AVAILABLE(macos(10.14))
  id<UNUserNotificationCenterDelegate> notification_center_delegate_ = nullptr;
  API_AVAILABLE(macos(10.14))
  std::unique_ptr<MacNotificationServiceUN> service_;
};

TEST_F(MacNotificationServiceUNTest, CloseAllNotifications) {
  if (@available(macOS 10.14, *)) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] removeAllDeliveredNotifications];
    service_remote_->CloseAllNotifications();
    run_loop.Run();
    [mock_notification_center_ verify];
  }
}

TEST_F(MacNotificationServiceUNTest, OnNotificationAction) {
  if (@available(macOS 10.14, *)) {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_handler_, OnNotificationAction)
        .WillOnce(
            [&](notifications::mojom::NotificationActionInfoPtr action_info) {
              // TODO(knollr): verify properties of |action_info| once we set
              // them.
              run_loop.Quit();
            });

    // Simulate a notification action and wait until we acknowledge it.
    base::RunLoop inner_run_loop;
    base::RepeatingClosure inner_quit_closure = inner_run_loop.QuitClosure();
    UNNotificationResponse* response =
        [OCMockObject mockForClass:[UNNotificationResponse class]];
    [notification_center_delegate_
                userNotificationCenter:mock_notification_center_
        didReceiveNotificationResponse:response
                 withCompletionHandler:^() {
                   inner_quit_closure.Run();
                 }];
    inner_run_loop.Run();
    run_loop.Run();
  }
}
