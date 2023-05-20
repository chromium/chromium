// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include <string>
#include <utility>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#import "chrome/services/mac_notifications/notification_test_utils_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/services/mac_notifications/unnotification_metrics.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace mac_notifications {

namespace {

struct NotificationActionParams {
  NSString* action_identifier;
  NotificationOperation operation;
  int button_index;
  absl::optional<std::u16string> reply;
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

class MacNotificationServiceUNTest : public testing::Test {
 public:
  MacNotificationServiceUNTest() {
    if (@available(macOS 10.14, *)) {
      mock_notification_center_ =
          [OCMockObject mockForClass:[UNUserNotificationCenter class]];
      // Expect the MacNotificationServiceUN ctor to register a delegate with
      // the UNNotificationCenter and ask for notification permissions.
      ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/true);
      [[mock_notification_center_ expect]
          getNotificationSettingsWithCompletionHandler:[OCMArg any]];
      [[[mock_notification_center_ expect] ignoringNonObjectArgs]
          requestAuthorizationWithOptions:0
                        completionHandler:[OCMArg any]];

      // We also synchronize displayed notifications and categories.
      [[[mock_notification_center_ expect] andDo:^(NSInvocation* invocation) {
        __unsafe_unretained void (^callback)(NSArray* _Nonnull notifications);
        [invocation getArgument:&callback atIndex:2];
        callback(@[]);
      }] getDeliveredNotificationsWithCompletionHandler:[OCMArg any]];
      [[[mock_notification_center_ expect] andDo:^(NSInvocation* invocation) {
        __unsafe_unretained void (^callback)(NSArray* _Nonnull categories);
        [invocation getArgument:&callback atIndex:2];
        callback(@[]);
      }] getNotificationCategoriesWithCompletionHandler:[OCMArg any]];

      service_ = std::make_unique<MacNotificationServiceUN>(
          service_remote_.BindNewPipeAndPassReceiver(),
          handler_receiver_.BindNewPipeAndPassRemote(),
          mock_notification_center_);
      [[mock_notification_center_ stub]
          setNotificationCategories:[OCMArg checkWithBlock:^BOOL(
                                                NSSet<UNNotificationCategory*>*
                                                    categories) {
            category_count_ = [categories count];
            return YES;
          }]];
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
  FakeUNNotification* CreateNotification(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool incognito,
                                         bool display = true,
                                         const std::string& category_id = "") {
    NSString* identifier = base::SysUTF8ToNSString(
        DeriveMacNotificationId(mojom::NotificationIdentifier::New(
            notification_id,
            mojom::ProfileIdentifier::New(profile_id, incognito))));

    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.userInfo = @{
      kNotificationId : base::SysUTF8ToNSString(notification_id),
      kNotificationProfileId : base::SysUTF8ToNSString(profile_id),
      kNotificationIncognito : [NSNumber numberWithBool:incognito],
    };
    if (!category_id.empty())
      content.categoryIdentifier = base::SysUTF8ToNSString(category_id);

    UNNotificationRequest* request =
        [UNNotificationRequest requestWithIdentifier:identifier
                                             content:content
                                             trigger:nil];

    FakeUNNotification* notification = [[FakeUNNotification alloc] init];
    notification.request = request;

    // Also call the |service_remote_| to setup the new notification. This will
    // make sure that any internal state is updated as well.
    if (display)
      DisplayNotificationSync(notification_id, profile_id, incognito);

    return notification;
  }

  API_AVAILABLE(macos(10.14))
  std::vector<FakeUNNotification*> SetupNotifications() {
    std::vector<FakeUNNotification*> notifications = {
        CreateNotification("notificationId", "profileId", /*incognito=*/false),
        CreateNotification("notificationId", "profileId2", /*incognito=*/true),
        CreateNotification("notificationId2", "profileId", /*incognito=*/true),
        CreateNotification("notificationId", "profileId", /*incognito=*/true),
    };

    NSMutableArray* notifications_ns =
        [NSMutableArray arrayWithCapacity:notifications.size()];
    for (const auto& notification : notifications)
      [notifications_ns addObject:notification];

    [[[mock_notification_center_ stub] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(NSArray* _Nonnull toasts);
      [invocation getArgument:&callback atIndex:2];
      callback(notifications_ns);
    }] getDeliveredNotificationsWithCompletionHandler:[OCMArg any]];

    return notifications;
  }

  static std::vector<mojom::NotificationIdentifierPtr>
  GetDisplayedNotificationsSync(mojom::MacNotificationService* service,
                                mojom::ProfileIdentifierPtr profile) {
    base::RunLoop run_loop;
    std::vector<mojom::NotificationIdentifierPtr> displayed;
    service->GetDisplayedNotifications(
        std::move(profile),
        base::BindLambdaForTesting(
            [&](std::vector<mojom::NotificationIdentifierPtr> notifications) {
              displayed = std::move(notifications);
              run_loop.Quit();
            }));
    run_loop.Run();
    return displayed;
  }

  std::vector<mojom::NotificationIdentifierPtr> GetDisplayedNotificationsSync(
      mojom::ProfileIdentifierPtr profile) {
    return GetDisplayedNotificationsSync(service_remote_.get(),
                                         std::move(profile));
  }

  API_AVAILABLE(macos(10.14))
  void DisplayNotificationSync(const std::string& notification_id,
                               const std::string& profile_id,
                               bool incognito,
                               bool success = true) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    [[[mock_notification_center_ expect] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(NSError* error);
      [invocation getArgument:&callback atIndex:3];
      callback(success ? nil
                       : [NSError errorWithDomain:@"" code:0 userInfo:nil]);
      quit_closure.Run();
    }] addNotificationRequest:[OCMArg any] withCompletionHandler:[OCMArg any]];

    // Create and display a new notification.
    auto notification =
        CreateMojoNotification(notification_id, profile_id, incognito);
    service_remote_->DisplayNotification(std::move(notification));

    run_loop.Run();
    [mock_notification_center_ verify];
  }

  mac_notifications::mojom::NotificationPtr CreateMojoNotification(
      const std::string& notification_id,
      const std::string& profile_id,
      bool incognito) {
    auto notification_identifier = mojom::NotificationIdentifier::New(
        notification_id, mojom::ProfileIdentifier::New(profile_id, incognito));
    auto meta = mojom::NotificationMetadata::New(
        std::move(notification_identifier), /*type=*/0, /*origin_url=*/GURL(),
        /*creator_pid=*/0);
    std::vector<mac_notifications::mojom::NotificationActionButtonPtr> buttons;

    return mac_notifications::mojom::Notification::New(
        std::move(meta), u"title", u"subtitle", u"body",
        /*renotify=*/true,
        /*show_settings_button=*/true, std::move(buttons),
        /*icon=*/gfx::ImageSkia());
  }

  // Creates a new service and destroys it immediately. This is used to test any
  // metrics logged during construction of the service. Tests can optionally
  // pass in an |on_create| callback to do further checks before the service is
  // destroyed.
  API_AVAILABLE(macos(10.14))
  void CreateAndDestroyService(
      UNNotificationRequestPermissionResult result,
      NSArray<UNNotification*>* notifications = nil,
      NSArray<UNNotificationCategory*>* categories = nil,
      base::OnceCallback<void(MacNotificationServiceUN*)> on_create =
          base::NullCallback()) {
    id mock_notification_center =
        [OCMockObject mockForClass:[UNUserNotificationCenter class]];
    MockNotificationActionHandler mock_handler;
    mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
        &mock_handler};
    mojo::Remote<mojom::MacNotificationService> service_remote;

    [[mock_notification_center expect] setDelegate:[OCMArg any]];
    [[mock_notification_center expect]
        getNotificationSettingsWithCompletionHandler:[OCMArg any]];
    [[[mock_notification_center stub] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(NSArray* _Nonnull notifications);
      [invocation getArgument:&callback atIndex:2];
      callback(notifications ? notifications : @[]);
    }] getDeliveredNotificationsWithCompletionHandler:[OCMArg any]];
    [[[mock_notification_center stub] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(NSArray* _Nonnull categories);
      [invocation getArgument:&callback atIndex:2];
      callback(categories ? categories : @[]);
    }] getNotificationCategoriesWithCompletionHandler:[OCMArg any]];

    [[[[mock_notification_center expect]
        ignoringNonObjectArgs] andDo:^(NSInvocation* invocation) {
      __unsafe_unretained void (^callback)(BOOL granted, NSError* error);
      [invocation getArgument:&callback atIndex:3];

      bool granted =
          result == UNNotificationRequestPermissionResult::kPermissionGranted;
      NSError* error =
          result == UNNotificationRequestPermissionResult::kRequestFailed
              ? [NSError errorWithDomain:@"" code:0 userInfo:nil]
              : nil;

      callback(granted, error);
    }] requestAuthorizationWithOptions:0 completionHandler:[OCMArg any]];

    auto service = std::make_unique<MacNotificationServiceUN>(
        service_remote.BindNewPipeAndPassReceiver(),
        handler_receiver.BindNewPipeAndPassRemote(), mock_notification_center);
    if (on_create)
      std::move(on_create).Run(service.get());

    [[mock_notification_center expect] setDelegate:[OCMArg any]];
    service.reset();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockNotificationActionHandler mock_handler_;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver_{
      &mock_handler_};
  mojo::Remote<mojom::MacNotificationService> service_remote_;
  id mock_notification_center_ = nil;
  API_AVAILABLE(macos(10.14))
  id<UNUserNotificationCenterDelegate> notification_center_delegate_ = nullptr;
  API_AVAILABLE(macos(10.14))
  std::unique_ptr<MacNotificationServiceUN> service_;
  unsigned int category_count_ = 0u;
};

TEST_F(MacNotificationServiceUNTest, DisplayNotification) {
  if (@available(macOS 10.14, *)) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    // Verify notification content.
    [[mock_notification_center_ expect]
        addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                           UNNotificationRequest* request) {
          EXPECT_NSEQ(@"i|profileId|notificationId", [request identifier]);
          NSDictionary* user_info = [[request content] userInfo];
          EXPECT_NSEQ(@"notificationId",
                      [user_info objectForKey:kNotificationId]);
          EXPECT_NSEQ(@"profileId",
                      [user_info objectForKey:kNotificationProfileId]);
          EXPECT_TRUE(
              [[user_info objectForKey:kNotificationIncognito] boolValue]);

          EXPECT_NSEQ(@"title", [[request content] title]);
          EXPECT_NSEQ(@"subtitle", [[request content] subtitle]);
          EXPECT_NSEQ(@"body", [[request content] body]);

          quit_closure.Run();
          return YES;
        }]
         withCompletionHandler:[OCMArg any]];

    // Create and display a new notification.
    auto notification = CreateMojoNotification("notificationId", "profileId",
                                               /*incognito=*/true);
    service_remote_->DisplayNotification(std::move(notification));

    run_loop.Run();
    [mock_notification_center_ verify];

    // Expect a new notification category for this notification.
    EXPECT_EQ(1u, category_count_);
  }
}

TEST_F(MacNotificationServiceUNTest, GetDisplayedNotificationsForProfile) {
  if (@available(macOS 10.14, *)) {
    auto notifications = SetupNotifications();
    base::RunLoop run_loop;
    auto profile =
        mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
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
    DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true);
    EXPECT_EQ(1u, category_count_);

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    NSString* identifier = @"i|profileId|notificationId";
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] removeDeliveredNotificationsWithIdentifiers:@[ identifier ]];

    auto profile_identifier =
        mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
    auto notification_identifier = mojom::NotificationIdentifier::New(
        "notificationId", std::move(profile_identifier));
    service_remote_->CloseNotification(std::move(notification_identifier));

    run_loop.Run();
    [mock_notification_center_ verify];

    // Expect closing the notification to remove the category as well.
    EXPECT_EQ(0u, category_count_);
  }
}

TEST_F(MacNotificationServiceUNTest, SynchronizesNotifications) {
  if (@available(macOS 10.14, *)) {
    // Setup 4 notifications that are being returned by system APIs too.
    auto notifications = SetupNotifications();
    ASSERT_EQ(4u, GetDisplayedNotificationsSync(/*profile=*/nullptr).size());

    // Display a notification which won't be reflected in the system APIs.
    DisplayNotificationSync("notificationId3", "profileId", /*incognito=*/true);
    ASSERT_EQ(4u, GetDisplayedNotificationsSync(/*profile=*/nullptr).size());

    // Wait until the notification synchronization timer kicks in and expect it
    // to detect the missing notification.
    base::RunLoop run_loop;
    base::Time start_time = base::Time::Now();
    EXPECT_CALL(mock_handler_, OnNotificationAction)
        .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
          EXPECT_EQ(NotificationOperation::kClose, action_info->operation);
          EXPECT_EQ(kNotificationInvalidButtonIndex, action_info->button_index);
          EXPECT_EQ("notificationId3", action_info->meta->id->id);
          EXPECT_EQ("profileId", action_info->meta->id->profile->id);
          EXPECT_EQ(MacNotificationServiceUN::kSynchronizationInterval,
                    base::Time::Now() - start_time);
          run_loop.Quit();
        });
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_handler_);
  }
}

TEST_F(MacNotificationServiceUNTest, CloseProfileNotifications) {
  if (@available(macOS 10.14, *)) {
    auto notifications = SetupNotifications();
    // Even though we created 3 notifications, all of them share the same
    // category as they have the same actions.
    EXPECT_EQ(1u, category_count_);

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    NSArray* identifiers = @[
      @"i|profileId|notificationId2",
      @"i|profileId|notificationId",
    ];
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] removeDeliveredNotificationsWithIdentifiers:identifiers];

    auto profile_identifier =
        mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
    service_remote_->CloseNotificationsForProfile(
        std::move(profile_identifier));

    run_loop.Run();
    [mock_notification_center_ verify];
    // Verify that we closed two notifications but one still holds a reverence
    // to the common category.
    EXPECT_EQ(1u, category_count_);
  }
}

TEST_F(MacNotificationServiceUNTest, CloseAllNotifications) {
  if (@available(macOS 10.14, *)) {
    DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true);
    EXPECT_EQ(1u, category_count_);
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    [[[mock_notification_center_ expect] andDo:^(NSInvocation*) {
      quit_closure.Run();
    }] removeAllDeliveredNotifications];
    service_remote_->CloseAllNotifications();
    run_loop.Run();
    [mock_notification_center_ verify];
    EXPECT_EQ(0u, category_count_);
  }
}

TEST_F(MacNotificationServiceUNTest, LogsMetricsForAlerts) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;
    id mainBundleMock =
        [OCMockObject partialMockForObject:base::apple::MainBundle()];

    // Mock the alert style to "alert" and verify we log the correct metrics.
    [[[mainBundleMock stub]
        andReturn:@{@"NSUserNotificationAlertStyle" : @"alert"}]
        infoDictionary];

    for (auto result :
         {UNNotificationRequestPermissionResult::kRequestFailed,
          UNNotificationRequestPermissionResult::kPermissionDenied,
          UNNotificationRequestPermissionResult::kPermissionGranted}) {
      CreateAndDestroyService(result);
      histogram_tester.ExpectBucketCount(
          "Notifications.Permissions.UNNotification.Alert.PermissionRequest",
          /*sample=*/result, /*expected_count=*/1);
    }

    [mainBundleMock stopMocking];
  }
}

TEST_F(MacNotificationServiceUNTest, LogsMetricsForBanners) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;
    id mainBundleMock =
        [OCMockObject partialMockForObject:base::apple::MainBundle()];

    // Mock the alert style to "banner" and verify we log the correct metrics.
    [[[mainBundleMock stub]
        andReturn:@{@"NSUserNotificationAlertStyle" : @"banner"}]
        infoDictionary];

    for (auto result :
         {UNNotificationRequestPermissionResult::kRequestFailed,
          UNNotificationRequestPermissionResult::kPermissionDenied,
          UNNotificationRequestPermissionResult::kPermissionGranted}) {
      CreateAndDestroyService(result);
      histogram_tester.ExpectBucketCount(
          "Notifications.Permissions.UNNotification.Banner.PermissionRequest",
          /*sample=*/result, /*expected_count=*/1);
    }

    [mainBundleMock stopMocking];
  }
}

TEST_F(MacNotificationServiceUNTest, InitializeDeliveredNotifications) {
  if (@available(macOS 10.14, *)) {
    // Create an existing notification with a category that exist before
    // creating a new service.
    UNNotificationCategory* category_ns =
        NotificationCategoryManager::CreateCategory(
            {{{u"Action", /*reply=*/absl::nullopt}}, /*settings_button=*/true});
    std::string category_id = base::SysNSStringToUTF8(category_ns.identifier);
    FakeUNNotification* notification =
        CreateNotification("notificationId", "profileId",
                           /*incognito=*/false, /*display=*/false, category_id);
    auto notification_ns = static_cast<UNNotification*>(notification);

    // Expect the service to initialize internal state based on the existing
    // notifications and categories.
    CreateAndDestroyService(
        UNNotificationRequestPermissionResult::kPermissionGranted,
        @[ notification_ns ], @[ category_ns ],
        base::BindOnce([](MacNotificationServiceUN* service) {
          auto notifications =
              GetDisplayedNotificationsSync(service, /*profile=*/nullptr);
          ASSERT_EQ(1u, notifications.size());
          EXPECT_EQ("notificationId", notifications[0]->id);
        }));
  }
}

TEST_F(MacNotificationServiceUNTest, OnNotificationAction) {
  if (@available(macOS 10.14, *)) {
    // We can't use TEST_P and INSTANTIATE_TEST_SUITE_P as we can't access
    // UNNotificationDefaultActionIdentifier etc. outside an @available block.
    NotificationActionParams kNotificationActionParams[] = {
        {UNNotificationDismissActionIdentifier, NotificationOperation::kClose,
         kNotificationInvalidButtonIndex, /*reply=*/absl::nullopt},
        {UNNotificationDefaultActionIdentifier, NotificationOperation::kClick,
         kNotificationInvalidButtonIndex, /*reply=*/absl::nullopt},
        {kNotificationButtonOne, NotificationOperation::kClick,
         /*button_index=*/0, /*reply=*/absl::nullopt},
        {kNotificationButtonTwo, NotificationOperation::kClick,
         /*button_index=*/1, /*reply=*/absl::nullopt},
        {kNotificationSettingsButtonTag, NotificationOperation::kSettings,
         kNotificationInvalidButtonIndex, /*reply=*/absl::nullopt},
        {kNotificationButtonOne, NotificationOperation::kClick,
         /*button_index=*/0, u"reply"},
    };

    for (const auto& params : kNotificationActionParams) {
      FakeUNNotification* notification =
          CreateNotification("notificationId", "profileId",
                             /*incognito=*/false);

      base::RunLoop run_loop;
      EXPECT_CALL(mock_handler_, OnNotificationAction)
          .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
            EXPECT_EQ("notificationId", action_info->meta->id->id);
            EXPECT_EQ("profileId", action_info->meta->id->profile->id);
            EXPECT_FALSE(action_info->meta->id->profile->incognito);
            EXPECT_EQ(params.operation, action_info->operation);
            EXPECT_EQ(params.button_index, action_info->button_index);
            EXPECT_EQ(params.reply, action_info->reply);
            run_loop.Quit();
          });

      // Simulate a notification action and wait until we acknowledge it.
      base::RunLoop inner_run_loop;
      base::RepeatingClosure inner_quit_closure = inner_run_loop.QuitClosure();

      id response = [OCMockObject
          mockForClass:params.reply ? [UNTextInputNotificationResponse class]
                                    : [UNNotificationResponse class]];
      [[[response stub] andReturn:params.action_identifier] actionIdentifier];
      [[[response stub] andReturn:notification] notification];

      if (params.reply) {
        [[[response stub] andReturn:base::SysUTF16ToNSString(*params.reply)]
            userText];
      }

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
}

}  // namespace mac_notifications
