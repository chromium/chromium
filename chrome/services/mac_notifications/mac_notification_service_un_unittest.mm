// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"
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

  API_AVAILABLE(macos(10.14))
  base::scoped_nsobject<FakeUNNotification> CreateNotification(
      const std::string& notification_id,
      const std::string& profile_id,
      bool incognito) {
    NSString* identifier = base::SysUTF8ToNSString(
        DeriveMacNotificationId(incognito, profile_id, notification_id));

    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.userInfo = @{
      notification_constants::
      kNotificationId : base::SysUTF8ToNSString(notification_id),
      notification_constants::
      kNotificationProfileId : base::SysUTF8ToNSString(profile_id),
      notification_constants::
      kNotificationIncognito : [NSNumber numberWithBool:incognito],
    };

    UNNotificationRequest* request =
        [UNNotificationRequest requestWithIdentifier:identifier
                                             content:content
                                             trigger:nil];

    base::scoped_nsobject<FakeUNNotification> notification(
        [[FakeUNNotification alloc] init]);
    [notification setRequest:request];

    return notification;
  }

  API_AVAILABLE(macos(10.14))
  std::vector<base::scoped_nsobject<FakeUNNotification>> SetupNotifications() {
    std::vector<base::scoped_nsobject<FakeUNNotification>> notifications = {
        CreateNotification("notificationId", "profileId", /*incognito=*/false),
        CreateNotification("notificationId", "profileId2", /*incognito=*/true),
        CreateNotification("notificationId2", "profileId", /*incognito=*/true),
        CreateNotification("notificationId", "profileId", /*incognito=*/true),
    };

    NSMutableArray* notifications_ns =
        [NSMutableArray arrayWithCapacity:notifications.size()];
    for (const auto& notification : notifications)
      [notifications_ns addObject:notification.get()];

    [[[mock_notification_center_ expect] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(NSArray* _Nonnull toasts);
      [invocation getArgument:&callback atIndex:2];
      callback(notifications_ns);
    }] getDeliveredNotificationsWithCompletionHandler:[OCMArg any]];

    return notifications;
  }

  std::vector<notifications::mojom::NotificationIdentifierPtr>
  GetDisplayedNotificationsSync(
      notifications::mojom::ProfileIdentifierPtr profile) {
    base::RunLoop run_loop;
    std::vector<notifications::mojom::NotificationIdentifierPtr> displayed;
    service_remote_->GetDisplayedNotifications(
        std::move(profile),
        base::BindLambdaForTesting(
            [&](std::vector<notifications::mojom::NotificationIdentifierPtr>
                    notifications) {
              displayed = std::move(notifications);
              run_loop.Quit();
            }));
    run_loop.Run();
    return displayed;
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

TEST_F(MacNotificationServiceUNTest, GetDisplayedNotificationsForProfile) {
  if (@available(macOS 10.14, *)) {
    auto notifications = SetupNotifications();
    base::RunLoop run_loop;
    auto profile = notifications::mojom::ProfileIdentifier::New(
        "profileId", /*incognito=*/true);
    auto displayed = GetDisplayedNotificationsSync(std::move(profile));
    ASSERT_EQ(2u, displayed.size());

    std::set<std::string> notification_ids;
    for (const auto& notification : displayed) {
      ASSERT_TRUE(notification->profile);
      EXPECT_EQ("profileId", notification->profile->id);
      EXPECT_TRUE(notification->profile->incognito);
      notification_ids.insert(notification->id);
    }

    ASSERT_EQ(2u, notification_ids.size());
    EXPECT_EQ(1u, notification_ids.count("notificationId"));
    EXPECT_EQ(1u, notification_ids.count("notificationId2"));
  }
}

TEST_F(MacNotificationServiceUNTest, GetAllDisplayedNotifications) {
  if (@available(macOS 10.14, *)) {
    auto notifications = SetupNotifications();
    auto displayed = GetDisplayedNotificationsSync(/*profile=*/nullptr);
    EXPECT_EQ(notifications.size(), displayed.size());
  }
}

TEST_F(MacNotificationServiceUNTest, CloseNotification) {
  if (@available(macOS 10.14, *)) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    NSString* identifier = @"i|profileId|notificationId";
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] removeDeliveredNotificationsWithIdentifiers:@[ identifier ]];

    auto profile_identifier = notifications::mojom::ProfileIdentifier::New(
        "profileId", /*incognito=*/true);
    auto notification_identifier =
        notifications::mojom::NotificationIdentifier::New(
            "notificationId", std::move(profile_identifier));
    service_remote_->CloseNotification(std::move(notification_identifier));

    run_loop.Run();
    [mock_notification_center_ verify];
  }
}

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
