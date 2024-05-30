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
#include "base/test/test_future.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#import "chrome/services/mac_notifications/notification_test_utils_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/services/mac_notifications/un_user_notifications_spi.h"
#include "chrome/services/mac_notifications/unnotification_metrics.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace mac_notifications {

namespace {

struct NotificationActionParams {
  NSString* action_identifier;
  NotificationOperation operation;
  int button_index;
  std::optional<std::u16string> reply;
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

// Returns a block that invokes a closure. Used to invoke a closure when certain
// mock methods are called.
using InvocationBlock = void (^)(NSInvocation* invocation);
InvocationBlock invokeClosure(base::OnceClosure closure) {
  __block auto block_closure = std::move(closure);
  return ^(NSInvocation* invocation) {
    std::move(block_closure).Run();
  };
}

}  // namespace

class MacNotificationServiceUNTest : public testing::Test {
 public:
  MacNotificationServiceUNTest() {
    mock_notification_center_ =
        [OCMockObject mockForClass:[UNUserNotificationCenter class]];
    // Expect the MacNotificationServiceUN ctor to register a delegate with
    // the UNNotificationCenter and ask for notification permissions.
    ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/true);
    id settings = OCMClassMock([UNNotificationSettings class]);
    OCMStub([mock_notification_center_
        getNotificationSettingsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:settings, nil])]);

    // We also synchronize displayed notifications and categories.
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    OCMExpect([mock_notification_center_
        getNotificationCategoriesWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);

    service_ = std::make_unique<MacNotificationServiceUN>(
        handler_receiver_.BindNewPipeAndPassRemote(), base::DoNothing(),
        mock_notification_center_);
    service_->Bind(service_remote_.BindNewPipeAndPassReceiver());
    OCMStub([mock_notification_center_
        setNotificationCategories:[OCMArg checkWithBlock:^BOOL(
                                              NSSet<UNNotificationCategory*>*
                                                  categories) {
          category_count_ = [categories count];
          return YES;
        }]]);
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);

    OCMExpect(
        [mock_notification_center_
            requestAuthorizationWithOptions:0
                          completionHandler:
                              ([OCMArg
                                  invokeBlockWithArgs:[NSNumber
                                                          numberWithBool:YES],
                                                      [NSNull null], nil])])
        .ignoringNonObjectArgs();
    base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
    service_->RequestPermission(permission_result.GetCallback());
    EXPECT_TRUE(permission_result.Wait());
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  ~MacNotificationServiceUNTest() override {
    if (service_) {
      ResetService();
    }
  }

  void ResetService() {
    // Expect the MacNotificationServiceUN dtor to clear the delegate from the
    // UNNotificationCenter.
    ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/false);
    service_.reset();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

 protected:
  void ExpectAndUpdateUNUserNotificationCenterDelegate(bool expect_not_nil) {
    OCMExpect([mock_notification_center_
        setDelegate:[OCMArg checkWithBlock:^BOOL(
                                id<UNUserNotificationCenterDelegate> delegate) {
          EXPECT_EQ(expect_not_nil, delegate != nil);
          notification_center_delegate_ = delegate;
          return YES;
        }]]);
  }

  FakeUNNotification* CreateNotification(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool incognito,
                                         bool display = true,
                                         const std::string& category_id = "",
                                         const GURL& origin = {}) {
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
      kNotificationOrigin : base::SysUTF8ToNSString(origin.spec()),
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

  std::vector<FakeUNNotification*> SetupNotifications() {
    std::vector<FakeUNNotification*> notifications = {
        CreateNotification("notificationId", "profileId", /*incognito=*/false,
                           /*display=*/true, /*category_id=*/"",
                           GURL("https://example.com")),
        CreateNotification("notificationId", "profileId2", /*incognito=*/true,
                           /*display=*/true, /*category_id=*/"",
                           GURL("https://example.com")),
        CreateNotification("notificationId2", "profileId", /*incognito=*/true,
                           /*display=*/true, /*category_id=*/"",
                           GURL("https://example.com")),
        CreateNotification("notificationId", "profileId", /*incognito=*/true,
                           /*display=*/true, /*category_id=*/"",
                           GURL("https://gmail.com")),
    };

    NSMutableArray* notifications_ns =
        [NSMutableArray arrayWithCapacity:notifications.size()];
    for (const auto& notification : notifications) {
      [notifications_ns addObject:notification];
    }

    OCMStub([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockOnQueue:dispatch_get_main_queue()
                               withArgs:notifications_ns, nil])]);

    return notifications;
  }

  static std::vector<mojom::NotificationIdentifierPtr>
  GetDisplayedNotificationsSync(mojom::MacNotificationService* service,
                                mojom::ProfileIdentifierPtr profile,
                                std::optional<GURL> origin = std::nullopt) {
    base::test::TestFuture<std::vector<mojom::NotificationIdentifierPtr>>
        displayed;
    service->GetDisplayedNotifications(std::move(profile), origin,
                                       displayed.GetCallback());
    return displayed.Take();
  }

  std::vector<mojom::NotificationIdentifierPtr> GetDisplayedNotificationsSync(
      mojom::ProfileIdentifierPtr profile,
      std::optional<GURL> origin = std::nullopt) {
    return GetDisplayedNotificationsSync(service_remote_.get(),
                                         std::move(profile), std::move(origin));
  }

  void DisplayNotificationSync(const std::string& notification_id,
                               const std::string& profile_id,
                               bool incognito,
                               bool success = true) {
    base::RunLoop run_loop;

    OCMExpect(
        [mock_notification_center_
            addNotificationRequest:[OCMArg any]
             withCompletionHandler:
                 ([OCMArg
                     invokeBlockWithArgs:(success
                                              ? [NSNull null]
                                              : [NSError errorWithDomain:@""
                                                                    code:0
                                                                userInfo:nil]),
                                         nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));

    // Create and display a new notification.
    auto notification =
        CreateMojoNotification(notification_id, profile_id, incognito);
    service_remote_->DisplayNotification(std::move(notification));

    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  mac_notifications::mojom::NotificationPtr CreateMojoNotification(
      const std::string& notification_id,
      const std::string& profile_id,
      bool incognito,
      bool renotify = true) {
    auto notification_identifier = mojom::NotificationIdentifier::New(
        notification_id, mojom::ProfileIdentifier::New(profile_id, incognito));
    auto meta = mojom::NotificationMetadata::New(
        std::move(notification_identifier), /*type=*/0, /*origin_url=*/GURL(),
        /*user_data_dir=*/"");
    std::vector<mac_notifications::mojom::NotificationActionButtonPtr> buttons;

    return mac_notifications::mojom::Notification::New(
        std::move(meta), u"title", u"subtitle", u"body", renotify,
        /*show_settings_button=*/true, std::move(buttons),
        /*icon=*/gfx::ImageSkia());
  }

  id CreateMockNotificationSettings(UNAuthorizationStatus status) {
    id settings = OCMClassMock([UNNotificationSettings class]);
    OCMStub([settings authorizationStatus]).andReturn(status);
    return settings;
  }

  id CreateMockNotificationCenter(
      id settings,
      NSArray<UNNotification*>* notifications = nil,
      NSArray<UNNotificationCategory*>* categories = nil) {
    id mock_notification_center =
        [OCMockObject mockForClass:[UNUserNotificationCenter class]];

    OCMStub([mock_notification_center setDelegate:[OCMArg any]]);
    OCMStub([mock_notification_center
        getNotificationSettingsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:settings, nil])]);
    OCMStub([mock_notification_center
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:(notifications ? notifications : @[]),
                                         nil])]);
    OCMStub([mock_notification_center
        getNotificationCategoriesWithCompletionHandler:
            ([OCMArg
                invokeBlockWithArgs:(categories ? categories : @[]), nil])]);
    return mock_notification_center;
  }

  // Creates a new service and destroys it immediately. This is used to test any
  // metrics logged during construction of the service. Tests can optionally
  // pass in an |on_create| callback to do further checks before the service is
  // destroyed.
  void CreateAndDestroyService(
      mojom::RequestPermissionResult result,
      NSArray<UNNotification*>* notifications = nil,
      NSArray<UNNotificationCategory*>* categories = nil,
      base::OnceCallback<void(MacNotificationServiceUN*)> on_create =
          base::NullCallback()) {
    UNAuthorizationStatus status =
        result == mojom::RequestPermissionResult::kPermissionPreviouslyDenied
            ? UNAuthorizationStatusDenied
        : result == mojom::RequestPermissionResult::kPermissionPreviouslyGranted
            ? UNAuthorizationStatusAuthorized
            : UNAuthorizationStatusNotDetermined;
    id mock_notification_center = CreateMockNotificationCenter(
        CreateMockNotificationSettings(status), notifications, categories);

    MockNotificationActionHandler mock_handler;
    mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
        &mock_handler};
    mojo::Remote<mojom::MacNotificationService> service_remote;

    if (result != mojom::RequestPermissionResult::kPermissionPreviouslyDenied &&
        result !=
            mojom::RequestPermissionResult::kPermissionPreviouslyGranted) {
      bool granted =
          result == mojom::RequestPermissionResult::kPermissionGranted;
      id error = (result == mojom::RequestPermissionResult::kRequestFailed ||
                  result == mojom::RequestPermissionResult::kPermissionDenied)
                     ? [NSError errorWithDomain:@"" code:0 userInfo:nil]
                     : NSNull.null;
      OCMExpect(
          [mock_notification_center
              requestAuthorizationWithOptions:0
                            completionHandler:
                                ([OCMArg invokeBlockWithArgs:@(granted), error,
                                                             nil])])
          .ignoringNonObjectArgs();
    }

    auto service = std::make_unique<MacNotificationServiceUN>(
        handler_receiver.BindNewPipeAndPassRemote(), base::DoNothing(),
        mock_notification_center);
    service->Bind(service_remote.BindNewPipeAndPassReceiver());
    base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
    service->RequestPermission(permission_result.GetCallback());
    EXPECT_EQ(result, permission_result.Get());
    if (on_create)
      std::move(on_create).Run(service.get());

    service.reset();
    EXPECT_OCMOCK_VERIFY(mock_notification_center);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};
  MockNotificationActionHandler mock_handler_;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver_{
      &mock_handler_};
  mojo::Remote<mojom::MacNotificationService> service_remote_;
  id mock_notification_center_ = nil;
  id<UNUserNotificationCenterDelegate> notification_center_delegate_ = nullptr;
  std::unique_ptr<MacNotificationServiceUN> service_;
  unsigned int category_count_ = 0u;
};

TEST_F(MacNotificationServiceUNTest, DisplayNotification) {
  base::RunLoop run_loop;

  // Verify notification content.
  OCMExpect(
      [mock_notification_center_
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
            return YES;
          }]
           withCompletionHandler:[OCMArg any]])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  // Create and display a new notification.
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  service_remote_->DisplayNotification(std::move(notification));

  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Expect a new notification category for this notification.
  EXPECT_EQ(1u, category_count_);
}

TEST_F(MacNotificationServiceUNTest, RedisplayNotification) {
  auto notification_no_renotify =
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false, /*renotify=*/false);
  auto notification_with_renotify =
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false, /*renotify=*/true);

  auto verify_notification_content = [](UNNotificationContent* content) {
    NSDictionary* user_info = content.userInfo;
    EXPECT_NSEQ(@"notificationId", [user_info objectForKey:kNotificationId]);
    EXPECT_NSEQ(@"profileId", [user_info objectForKey:kNotificationProfileId]);
    EXPECT_FALSE([[user_info objectForKey:kNotificationIncognito] boolValue]);

    EXPECT_NSEQ(@"title", content.title);
    EXPECT_NSEQ(@"subtitle", content.subtitle);
    EXPECT_NSEQ(@"body", content.body);
  };

  {
    // Display a new notification.
    base::RunLoop run_loop;

    OCMExpect(
        [mock_notification_center_
            addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                               UNNotificationRequest* request) {
              EXPECT_NSEQ(@"r|profileId|notificationId", request.identifier);
              verify_notification_content(request.content);
              return YES;
            }]
             withCompletionHandler:[OCMArg any]])
        .andDo(invokeClosure(run_loop.QuitClosure()));

    service_remote_->DisplayNotification(notification_no_renotify.Clone());

    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  {
    // Now display the same notification again, with redisplay set to false.
    // This should query the currently displayed notifications, and only cause
    // the contents to be replaced if the notification is still delivered.
    base::RunLoop run_loop;

    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[
              CreateNotification("notificationId", "profileId",
                                 /*incognito=*/false, /*display=*/false)
            ],
                                         nil])]);
    OCMExpect(
        [mock_notification_center_
            replaceContentForRequestWithIdentifier:@"r|profileId|notificationId"
                                replacementContent:
                                    [OCMArg
                                        checkWithBlock:^BOOL(
                                            UNNotificationContent* content) {
                                          verify_notification_content(content);
                                          return YES;
                                        }]
                                 completionHandler:[OCMArg any]])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_no_renotify.Clone());

    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  {
    // Now display the notification with renotify set to true.
    base::RunLoop run_loop;

    OCMExpect(
        [mock_notification_center_
            addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                               UNNotificationRequest* request) {
              EXPECT_NSEQ(@"r|profileId|notificationId", request.identifier);
              verify_notification_content(request.content);
              return YES;
            }]
             withCompletionHandler:[OCMArg any]])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_with_renotify.Clone());

    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  {
    // Finally, display the notification with renotify set to false, but with
    // the notification having been closed by the OS in the meantime,
    base::RunLoop run_loop;

    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    OCMExpect(
        [mock_notification_center_
            addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                               UNNotificationRequest* request) {
              EXPECT_NSEQ(@"r|profileId|notificationId", request.identifier);
              verify_notification_content(request.content);
              return YES;
            }]
             withCompletionHandler:[OCMArg any]])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_no_renotify.Clone());

    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }
}

TEST_F(MacNotificationServiceUNTest, Rebind) {
  base::RunLoop run_loop;

  // Reconnnect to the same MacNotificationServiceUNTest instance.
  service_remote_.reset();
  service_->Bind(service_remote_.BindNewPipeAndPassReceiver());

  // Verify notification is created..
  OCMExpect([mock_notification_center_ addNotificationRequest:[OCMArg any]
                                        withCompletionHandler:[OCMArg any]])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  // Create and display a new notification.
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  service_remote_->DisplayNotification(std::move(notification));

  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(MacNotificationServiceUNTest, GetDisplayedNotificationsForProfile) {
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

TEST_F(MacNotificationServiceUNTest,
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

TEST_F(MacNotificationServiceUNTest, GetAllDisplayedNotifications) {
  auto notifications = SetupNotifications();
  auto displayed = GetDisplayedNotificationsSync(/*profile=*/nullptr);
  EXPECT_EQ(notifications.size(), displayed.size());
}

TEST_F(MacNotificationServiceUNTest, CloseNotification) {
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true);
  EXPECT_EQ(1u, category_count_);

  base::RunLoop run_loop;

  NSString* identifier = @"i|profileId|notificationId";
  OCMExpect([mock_notification_center_
                removeDeliveredNotificationsWithIdentifiers:@[ identifier ]])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto profile_identifier =
      mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
  auto notification_identifier = mojom::NotificationIdentifier::New(
      "notificationId", std::move(profile_identifier));
  service_remote_->CloseNotification(std::move(notification_identifier));

  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Expect closing the notification to remove the category as well.
  EXPECT_EQ(0u, category_count_);
}

TEST_F(MacNotificationServiceUNTest, SynchronizesNotifications) {
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

TEST_F(MacNotificationServiceUNTest, CloseProfileNotifications) {
  auto notifications = SetupNotifications();
  // Even though we created 3 notifications, all of them share the same
  // category as they have the same actions.
  EXPECT_EQ(1u, category_count_);

  base::RunLoop run_loop;

  NSArray* identifiers = @[
    @"i|profileId|notificationId2",
    @"i|profileId|notificationId",
  ];
  OCMExpect([mock_notification_center_
                removeDeliveredNotificationsWithIdentifiers:identifiers])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto profile_identifier =
      mojom::ProfileIdentifier::New("profileId", /*incognito=*/true);
  service_->CloseNotificationsForProfile(std::move(profile_identifier));

  // Reset `service_` to verify that this still works if `service_` gets
  // destroyed while waiting for UNUserNotificationCenter to reply back with
  // the currently displaying notifications.
  ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/false);
  service_.reset();

  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  // Verify that we closed two notifications but one still holds a reverence
  // to the common category.
  EXPECT_EQ(1u, category_count_);
}

TEST_F(MacNotificationServiceUNTest, CloseAllNotifications) {
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true);
  EXPECT_EQ(1u, category_count_);
  base::RunLoop run_loop;
  OCMExpect([mock_notification_center_ removeAllDeliveredNotifications])
      .andDo(invokeClosure(run_loop.QuitClosure()));
  service_remote_->CloseAllNotifications();
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  EXPECT_EQ(0u, category_count_);
}

TEST_F(MacNotificationServiceUNTest, LogsMetricsForAlerts) {
  base::HistogramTester histogram_tester;
  id mainBundleMock =
      [OCMockObject partialMockForObject:base::apple::MainBundle()];

  // Mock the alert style to "alert" and verify we log the correct metrics.
  OCMStub([mainBundleMock infoDictionary]).andReturn(@{
    @"NSUserNotificationAlertStyle" : @"alert"
  });

  // Test does not include kRequestFailed, as currently there is no code path
  // that would result in that error.
  for (auto result :
       {mojom::RequestPermissionResult::kPermissionDenied,
        mojom::RequestPermissionResult::kPermissionGranted,
        mojom::RequestPermissionResult::kPermissionPreviouslyDenied,
        mojom::RequestPermissionResult::kPermissionPreviouslyGranted}) {
    CreateAndDestroyService(result);
    histogram_tester.ExpectBucketCount(
        "Notifications.Permissions.UNNotification.Alert.PermissionRequest",
        /*sample=*/result, /*expected_count=*/1);
  }

  [mainBundleMock stopMocking];
}

TEST_F(MacNotificationServiceUNTest, LogsMetricsForBanners) {
  base::HistogramTester histogram_tester;
  id mainBundleMock =
      [OCMockObject partialMockForObject:base::apple::MainBundle()];

  // Mock the alert style to "banner" and verify we log the correct metrics.
  OCMStub([mainBundleMock infoDictionary]).andReturn(@{
    @"NSUserNotificationAlertStyle" : @"banner"
  });

  // Test does not include kRequestFailed, as currently there is no code path
  // that would result in that error.
  for (auto result :
       {mojom::RequestPermissionResult::kPermissionDenied,
        mojom::RequestPermissionResult::kPermissionGranted,
        mojom::RequestPermissionResult::kPermissionPreviouslyDenied,
        mojom::RequestPermissionResult::kPermissionPreviouslyGranted}) {
    CreateAndDestroyService(result);
    histogram_tester.ExpectBucketCount(
        "Notifications.Permissions.UNNotification.Banner.PermissionRequest",
        /*sample=*/result, /*expected_count=*/1);
  }

  [mainBundleMock stopMocking];
}

TEST_F(MacNotificationServiceUNTest, InitializeDeliveredNotifications) {
  // Create an existing notification with a category that exist before
  // creating a new service.
  UNNotificationCategory* category_ns =
      NotificationCategoryManager::CreateCategory(
          {{{u"Action", /*reply=*/std::nullopt}}, /*settings_button=*/true});
  std::string category_id = base::SysNSStringToUTF8(category_ns.identifier);
  FakeUNNotification* notification =
      CreateNotification("notificationId", "profileId",
                         /*incognito=*/false, /*display=*/false, category_id);
  auto notification_ns = static_cast<UNNotification*>(notification);

  // Expect the service to initialize internal state based on the existing
  // notifications and categories.
  CreateAndDestroyService(
      mojom::RequestPermissionResult::kPermissionGranted, @[ notification_ns ],
      @[ category_ns ], base::BindOnce([](MacNotificationServiceUN* service) {
        auto notifications =
            GetDisplayedNotificationsSync(service, /*profile=*/nullptr);
        ASSERT_EQ(1u, notifications.size());
        EXPECT_EQ("notificationId", notifications[0]->id);
      }));
}

TEST_F(MacNotificationServiceUNTest, OnNotificationAction) {
  // We can't use TEST_P and INSTANTIATE_TEST_SUITE_P as we can't access
  // UNNotificationDefaultActionIdentifier etc. outside an @available block.
  NotificationActionParams kNotificationActionParams[] = {
      {UNNotificationDismissActionIdentifier, NotificationOperation::kClose,
       kNotificationInvalidButtonIndex, /*reply=*/std::nullopt},
      {UNNotificationDefaultActionIdentifier, NotificationOperation::kClick,
       kNotificationInvalidButtonIndex, /*reply=*/std::nullopt},
      {kNotificationButtonOne, NotificationOperation::kClick,
       /*button_index=*/0, /*reply=*/std::nullopt},
      {kNotificationButtonTwo, NotificationOperation::kClick,
       /*button_index=*/1, /*reply=*/std::nullopt},
      {kNotificationSettingsButtonTag, NotificationOperation::kSettings,
       kNotificationInvalidButtonIndex, /*reply=*/std::nullopt},
      {kNotificationButtonOne, NotificationOperation::kClick,
       /*button_index=*/0, u"reply"},
  };

  int i = 0;
  for (const auto& params : kNotificationActionParams) {
    std::string notification_id = base::StringPrintf("notificationId%d", i++);
    FakeUNNotification* notification =
        CreateNotification(notification_id, "profileId",
                           /*incognito=*/false);

    base::RunLoop run_loop;
    EXPECT_CALL(mock_handler_, OnNotificationAction)
        .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
          EXPECT_EQ(notification_id, action_info->meta->id->id);
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
    OCMStub([response actionIdentifier]).andReturn(params.action_identifier);
    OCMStub([response notification]).andReturn(notification);

    if (params.reply) {
      OCMStub([response userText])
          .andReturn(base::SysUTF16ToNSString(*params.reply));
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

TEST_F(MacNotificationServiceUNTest, DidRecentlyHandledClickAction) {
  EXPECT_FALSE(service_->DidRecentlyHandleClickAction());

  // Simulate a notification click.
  FakeUNNotification* notification =
      CreateNotification("notificationId", "profileId",
                         /*incognito=*/false);
  id response = [OCMockObject mockForClass:[UNNotificationResponse class]];
  OCMStub([response actionIdentifier])
      .andReturn(UNNotificationDefaultActionIdentifier);
  OCMStub([response notification]).andReturn(notification);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
        run_loop.Quit();
      });

  [notification_center_delegate_
              userNotificationCenter:mock_notification_center_
      didReceiveNotificationResponse:response
               withCompletionHandler:^(){
               }];

  EXPECT_TRUE(service_->DidRecentlyHandleClickAction());
  run_loop.Run();
  EXPECT_TRUE(service_->DidRecentlyHandleClickAction());
  task_environment_.FastForwardBy(base::Milliseconds(250));
  EXPECT_FALSE(service_->DidRecentlyHandleClickAction());
}

TEST_F(MacNotificationServiceUNTest,
       PermissionStateChangedCallback_RequestPermission) {
  id mock_notification_center = CreateMockNotificationCenter(
      CreateMockNotificationSettings(UNAuthorizationStatusNotDetermined));

  MockNotificationActionHandler mock_handler;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
      &mock_handler};

  base::test::TestFuture<mojom::PermissionStatus> status;
  auto service = std::make_unique<MacNotificationServiceUN>(
      handler_receiver.BindNewPipeAndPassRemote(),
      status.GetRepeatingCallback(), mock_notification_center);
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kNotDetermined);

  bool granted = true;
  OCMExpect(
      [mock_notification_center
          requestAuthorizationWithOptions:0
                        completionHandler:([OCMArg
                                              invokeBlockWithArgs:@(granted),
                                                                  NSNull.null,
                                                                  nil])])
      .ignoringNonObjectArgs();
  base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
  service->RequestPermission(permission_result.GetCallback());
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kPromptPending);
  EXPECT_EQ(permission_result.Get(),
            mojom::RequestPermissionResult::kPermissionGranted);
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kGranted);
}

TEST_F(MacNotificationServiceUNTest,
       PermissionStateChangedCallback_RequestPermissionPreviouslyDenied) {
  id mock_notification_center = CreateMockNotificationCenter(
      CreateMockNotificationSettings(UNAuthorizationStatusDenied));

  MockNotificationActionHandler mock_handler;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
      &mock_handler};

  mojom::PermissionStatus status = mojom::PermissionStatus::kNotDetermined;
  base::RunLoop loop;
  auto service = std::make_unique<MacNotificationServiceUN>(
      handler_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::PermissionStatus new_status) {
        status = new_status;
        loop.Quit();
      }),
      mock_notification_center);
  loop.Run();
  EXPECT_EQ(status, mojom::PermissionStatus::kDenied);

  base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
  service->RequestPermission(permission_result.GetCallback());
  EXPECT_EQ(permission_result.Get(),
            mojom::RequestPermissionResult::kPermissionPreviouslyDenied);
  EXPECT_EQ(status, mojom::PermissionStatus::kDenied);
}

TEST_F(MacNotificationServiceUNTest,
       PermissionStateChangedCallback_Synchronization) {
  // Reset the service created by the test harness so it doesn't interfere with
  // the one created in this test.
  ResetService();

  UNAuthorizationStatus current_status = UNAuthorizationStatusAuthorized;
  auto& status_ref = current_status;
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([settings authorizationStatus]).andDo(^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)(&status_ref)];
  });
  id mock_notification_center = CreateMockNotificationCenter(settings);

  MockNotificationActionHandler mock_handler;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
      &mock_handler};

  base::test::TestFuture<mojom::PermissionStatus> status;
  auto service = std::make_unique<MacNotificationServiceUN>(
      handler_receiver.BindNewPipeAndPassRemote(),
      status.GetRepeatingCallback(), mock_notification_center);
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kGranted);

  current_status = UNAuthorizationStatusDenied;
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kDenied);

  current_status = UNAuthorizationStatusNotDetermined;
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kNotDetermined);

  current_status = UNAuthorizationStatusAuthorized;
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kGranted);
}

}  // namespace mac_notifications
