// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/NSUserNotification.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#import "chrome/services/mac_notifications/mac_notification_service_ns.h"
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

// Make dynamic properties accessible for OCMock.
@implementation NSUserNotificationCenter (Testing)
- (id<NSUserNotificationCenterDelegate>)delegate {
  return nil;
}
- (void)setDelegate:(id<NSUserNotificationCenterDelegate>)delegate {
}
@end

class MacNotificationServiceNSTest : public testing::Test {
 public:
  MacNotificationServiceNSTest() {
    mock_notification_center_ =
        [OCMockObject mockForClass:[NSUserNotificationCenter class]];
    // Expect the MacNotificationServiceNS ctor to register a delegate with the
    // NSUserNotificationCenter.
    ExpectAndUpdateNSUserNotificationCenterDelegate(/*expect_not_nil=*/true);
    service_ = std::make_unique<MacNotificationServiceNS>(
        service_remote_.BindNewPipeAndPassReceiver(),
        handler_receiver_.BindNewPipeAndPassRemote(),
        mock_notification_center_);
    [mock_notification_center_ verify];
  }

  ~MacNotificationServiceNSTest() override {
    // Expect the MacNotificationServiceUN dtor to clear the delegate from the
    // UNNotificationCenter.
    ExpectAndUpdateNSUserNotificationCenterDelegate(/*expect_not_nil=*/false);
    service_.reset();
    [mock_notification_center_ verify];
  }

 protected:
  void ExpectAndUpdateNSUserNotificationCenterDelegate(bool expect_not_nil) {
    [[mock_notification_center_ expect]
        setDelegate:[OCMArg checkWithBlock:^BOOL(
                                id<NSUserNotificationCenterDelegate> delegate) {
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
  id<NSUserNotificationCenterDelegate> notification_center_delegate_ = nullptr;
  std::unique_ptr<MacNotificationServiceNS> service_;
};

TEST_F(MacNotificationServiceNSTest, CloseAllNotifications) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
    quit_closure.Run();
  }] removeAllDeliveredNotifications];
  service_remote_->CloseAllNotifications();
  run_loop.Run();
  [mock_notification_center_ verify];
}

TEST_F(MacNotificationServiceNSTest, OnNotificationAction) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce(
          [&](notifications::mojom::NotificationActionInfoPtr action_info) {
            // TODO(knollr): verify properties of |action_info| once we set
            // them.
            run_loop.Quit();
          });

  // Simulate a notification action and wait until we acknowledge it.
  NSUserNotification* notification =
      [OCMockObject mockForClass:[NSUserNotification class]];
  [notification_center_delegate_
       userNotificationCenter:mock_notification_center_
      didActivateNotification:notification];
  run_loop.Run();
}
