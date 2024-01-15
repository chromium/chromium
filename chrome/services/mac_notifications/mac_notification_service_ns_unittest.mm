// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/NSUserNotification.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#import "chrome/services/mac_notifications/mac_notification_service_ns.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// This class implements the Chromium interface to a deprecated API. It is in
// the process of being replaced, and warnings about its deprecation are not
// helpful. https://crbug.com/1127306
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Make dynamic properties accessible for OCMock.
@implementation NSUserNotificationCenter (Testing)
- (id<NSUserNotificationCenterDelegate>)delegate {
  return nil;
}
- (void)setDelegate:(id<NSUserNotificationCenterDelegate>)delegate {
}
- (NSArray*)deliveredNotifications {
  return nil;
}
@end

@implementation NSUserNotification (Testing)
- (NSDictionary*)userInfo {
  return nil;
}
- (NSUserNotificationActivationType)activationType {
  return NSUserNotificationActivationTypeNone;
}
- (NSArray*)_alternateActionButtonTitles {
  return nil;
}
- (NSNumber*)_alternateActionIndex {
  return nil;
}
@end

namespace mac_notifications {

namespace {

struct NotificationActionParams {
  NSUserNotificationActivationType activation_type;
  NSNumber* has_settings_button;
  NSArray* action_button_titles;
  NSNumber* alternate_action_index;
  NotificationOperation operation;
  int button_index;
};

class MockNotificationActionHandler
    : public mojom::MacNotificationActionHandler {
 public:
  // mojom::MacNotificationActionHandler:
  MOCK_METHOD(void,
              OnNotificationAction,
              (mojom::NotificationActionInfoPtr),
              (override));
};

}  // namespace

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

  NSUserNotification* CreateNotification(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool incognito,
                                         const GURL& origin) {
    NSUserNotification* toast = [[NSUserNotification alloc] init];
    toast.userInfo = @{
      kNotificationId : base::SysUTF8ToNSString(notification_id),
      kNotificationProfileId : base::SysUTF8ToNSString(profile_id),
      kNotificationIncognito : [NSNumber numberWithBool:incognito],
      kNotificationOrigin : base::SysUTF8ToNSString(origin.spec()),
    };
    return toast;
  }

  std::vector<NSUserNotification*> SetupNotifications() {
    std::vector<NSUserNotification*> notifications = {
        CreateNotification("notificationId", "profileId", /*incognito=*/false,
                           GURL("https://example.com")),
        CreateNotification("notificationId", "profileId2", /*incognito=*/true,
                           GURL("https://example.com")),
        CreateNotification("notificationId2", "profileId", /*incognito=*/true,
                           GURL("https://example.com")),
        CreateNotification("notificationId", "profileId", /*incognito=*/true,
                           GURL("https://gmail.com")),
    };

    NSMutableArray* notifications_ns =
        [NSMutableArray arrayWithCapacity:notifications.size()];
    for (const auto& notification : notifications)
      [notifications_ns addObject:notification];

    [[[mock_notification_center_ expect] andReturn:notifications_ns]
        deliveredNotifications];

    return notifications;
  }

  std::vector<mojom::NotificationIdentifierPtr> GetDisplayedNotificationsSync(
      mojom::ProfileIdentifierPtr profile,
      std::optional<GURL> origin = std::nullopt) {
    base::test::TestFuture<std::vector<mojom::NotificationIdentifierPtr>>
        displayed;
    service_remote_->GetDisplayedNotifications(std::move(profile), origin,
                                               displayed.GetCallback());
    return displayed.Take();
  }

  mojom::NotificationPtr CreateMojoNotification() {
    auto profile_identifier =
        mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
    auto notification_identifier = mojom::NotificationIdentifier::New(
        "notificationId", std::move(profile_identifier));
    auto meta = mojom::NotificationMetadata::New(
        std::move(notification_identifier), /*type=*/0, /*origin_url=*/GURL(),
        /*user_data_dir=*/"");

    std::vector<mojom::NotificationActionButtonPtr> buttons;
    return mojom::Notification::New(
        std::move(meta), u"title", u"subtitle", u"body", /*renotify=*/true,
        /*show_settings_button=*/true, std::move(buttons),
        /*icon=*/gfx::ImageSkia());
  }

  void DisplayNotificationSync() {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] deliverNotification:[OCMArg any]];

    service_remote_->DisplayNotification(CreateMojoNotification());
    run_loop.Run();
    [mock_notification_center_ verify];
  }

  base::test::TaskEnvironment task_environment_;
  MockNotificationActionHandler mock_handler_;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver_{
      &mock_handler_};
  mojo::Remote<mojom::MacNotificationService> service_remote_;
  id mock_notification_center_ = nil;
  id<NSUserNotificationCenterDelegate> notification_center_delegate_ = nullptr;
  std::unique_ptr<MacNotificationServiceNS> service_;
};

TEST_F(MacNotificationServiceNSTest, DisplayNotification) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  // Verify notification content.
  [[mock_notification_center_ expect]
      deliverNotification:[OCMArg checkWithBlock:^BOOL(
                                      NSUserNotification* notification) {
        EXPECT_NSEQ(@"i|profileId|notificationId", [notification identifier]);
        NSDictionary* user_info = [notification userInfo];
        EXPECT_NSEQ(@"notificationId",
                    [user_info objectForKey:kNotificationId]);
        EXPECT_NSEQ(@"profileId",
                    [user_info objectForKey:kNotificationProfileId]);
        EXPECT_TRUE(
            [[user_info objectForKey:kNotificationIncognito] boolValue]);

        EXPECT_NSEQ(@"title", [notification title]);
        EXPECT_NSEQ(@"subtitle", [notification subtitle]);
        EXPECT_NSEQ(@"body", [notification informativeText]);
        quit_closure.Run();
        return YES;
      }]];

  // Create and display a new notification.
  service_remote_->DisplayNotification(CreateMojoNotification());

  run_loop.Run();
  [mock_notification_center_ verify];
}

TEST_F(MacNotificationServiceNSTest, GetDisplayedNotificationsForProfile) {
  auto notifications = SetupNotifications();
  auto profile = mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
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

TEST_F(MacNotificationServiceNSTest,
       GetDisplayedNotificationsForProfileAndOrigin) {
  auto notifications = SetupNotifications();
  auto profile = mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
  auto displayed = GetDisplayedNotificationsSync(std::move(profile),
                                                 GURL("https://example.com"));
  ASSERT_EQ(1u, displayed.size());
  const auto& notification = *displayed.begin();
  ASSERT_TRUE(notification->profile);
  EXPECT_EQ("profileId", notification->profile->id);
  EXPECT_TRUE(notification->profile->incognito);
  EXPECT_EQ("notificationId2", notification->id);
}

TEST_F(MacNotificationServiceNSTest, GetAllDisplayedNotifications) {
  auto notifications = SetupNotifications();
  auto displayed = GetDisplayedNotificationsSync(/*profile=*/nullptr);
  EXPECT_EQ(notifications.size(), displayed.size());
}

TEST_F(MacNotificationServiceNSTest, CloseNotification) {
  auto notifications = SetupNotifications();
  NSUserNotification* expected = notifications.back();

  // Expect to close the expected notification.
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
    quit_closure.Run();
  }] removeDeliveredNotification:expected];

  auto profile_identifier =
      mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
  auto notification_identifier = mojom::NotificationIdentifier::New(
      "notificationId", std::move(profile_identifier));
  service_remote_->CloseNotification(std::move(notification_identifier));

  run_loop.Run();
  [mock_notification_center_ verify];
}

TEST_F(MacNotificationServiceNSTest, CloseProfileNotifications) {
  auto notifications = SetupNotifications();

  // Expect to close the expected notifications.
  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(/*num_closures=*/2, run_loop.QuitClosure());
  [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
    barrier.Run();
  }] removeDeliveredNotification:notifications[2]];
  [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
    barrier.Run();
  }] removeDeliveredNotification:notifications[3]];

  auto profile_identifier =
      mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
  service_remote_->CloseNotificationsForProfile(std::move(profile_identifier));

  run_loop.Run();
  [mock_notification_center_ verify];
}

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

const NotificationActionParams kNotificationActionParams[] = {
    {NSUserNotificationActivationTypeNone,
     /*has_settings_button=*/@NO, @[ @"A", @"B" ],
     /*alternate_action_index=*/@0, NotificationOperation::kClose,
     kNotificationInvalidButtonIndex},
    {NSUserNotificationActivationTypeContentsClicked,
     /*has_settings_button=*/@NO, @[ @"A", @"B" ],
     /*alternate_action_index=*/@0, NotificationOperation::kClick,
     kNotificationInvalidButtonIndex},
    {NSUserNotificationActivationTypeActionButtonClicked,
     /*has_settings_button=*/@NO, @[ @"A", @"B" ],
     /*alternate_action_index=*/@0, NotificationOperation::kClick,
     /*button_index=*/0},
    {NSUserNotificationActivationTypeActionButtonClicked,
     /*has_settings_button=*/@YES, @[ @"A", @"B", @"Settings" ],
     /*alternate_action_index=*/@1, NotificationOperation::kClick,
     /*button_index=*/1},
    {NSUserNotificationActivationTypeActionButtonClicked,
     /*has_settings_button=*/@YES, @[ @"A", @"B", @"Settings" ],
     /*alternate_action_index=*/@2, NotificationOperation::kSettings,
     kNotificationInvalidButtonIndex},
};

class MacNotificationServiceNSTestNotificationAction
    : public MacNotificationServiceNSTest,
      public testing::WithParamInterface<NotificationActionParams> {
 public:
  MacNotificationServiceNSTestNotificationAction() = default;
  ~MacNotificationServiceNSTestNotificationAction() override = default;
};

TEST_P(MacNotificationServiceNSTestNotificationAction, OnNotificationAction) {
  const NotificationActionParams& params = GetParam();
  base::RunLoop run_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
        EXPECT_EQ(params.operation, action_info->operation);
        EXPECT_EQ(params.button_index, action_info->button_index);
        run_loop.Quit();
      });

  // Simulate a notification action and wait until we acknowledge it.
  id notification = [OCMockObject mockForClass:[NSUserNotification class]];
  [[[notification stub] andReturn:@{
    kNotificationHasSettingsButton : params.has_settings_button,
  }] userInfo];
  [[[notification stub] andReturnValue:OCMOCK_VALUE(params.activation_type)]
      activationType];
  [[[notification stub] andReturn:params.action_button_titles]
      valueForKey:@"_alternateActionButtonTitles"];
  [[[notification stub] andReturn:params.alternate_action_index]
      valueForKey:@"_alternateActionIndex"];

  [notification_center_delegate_
       userNotificationCenter:mock_notification_center_
      didActivateNotification:notification];
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         MacNotificationServiceNSTestNotificationAction,
                         testing::ValuesIn(kNotificationActionParams));

}  // namespace mac_notifications

#pragma clang diagnostic pop
