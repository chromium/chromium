// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_un.h"

#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

#include <string>
#include <utility>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
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
    MacNotificationServiceUN::SetOpenSystemSettingsCallbackForTesting(
        open_system_settings_future_.GetRepeatingCallback());

    mock_notification_center_ =
        [OCMockObject mockForClass:[UNUserNotificationCenter class]];
    // Expect the MacNotificationServiceUN ctor to register a delegate with
    // the UNNotificationCenter and ask for notification permissions.
    ExpectAndUpdateUNUserNotificationCenterDelegate(/*expect_not_nil=*/true);
    id settings = OCMClassMock([UNNotificationSettings class]);
    UNAuthorizationStatus& status_ref = authorization_status_;
    OCMStub([settings authorizationStatus]).andDo(^(NSInvocation* invocation) {
      [invocation setReturnValue:(void*)(&status_ref)];
    });
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
        handler_receiver_.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([this](mojom::PermissionStatus status) {
          reported_permission_statuses_.push_back(status);
        }),
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
    EXPECT_TRUE(open_system_settings_future_.Wait());
    open_system_settings_future_.Clear();
    authorization_status_ = UNAuthorizationStatusAuthorized;
    reported_permission_statuses_.clear();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  ~MacNotificationServiceUNTest() override {
    if (service_) {
      ResetService();
    }
    MacNotificationServiceUN::SetOpenSystemSettingsCallbackForTesting(
        base::NullCallback());
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
                               bool success = true,
                               const GURL& origin_url = GURL()) {
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
    auto notification = CreateMojoNotification(
        notification_id, profile_id, incognito, /*renotify=*/true, origin_url);
    base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
    service_remote_->DisplayNotification(std::move(notification),
                                         result_future.GetCallback());

    run_loop.Run();
    EXPECT_EQ(result_future.Get(),
              success ? mojom::DisplayNotificationResult::kSuccess
                      : mojom::DisplayNotificationResult::kDeliveryFailed);
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  mac_notifications::mojom::NotificationPtr CreateMojoNotification(
      const std::string& notification_id,
      const std::string& profile_id,
      bool incognito,
      bool renotify = true,
      const GURL& origin_url = GURL()) {
    auto notification_identifier = mojom::NotificationIdentifier::New(
        notification_id, mojom::ProfileIdentifier::New(profile_id, incognito));
    auto meta = mojom::NotificationMetadata::New(
        std::move(notification_identifier), /*type=*/0, origin_url,
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
    open_system_settings_future_.Clear();
    base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
    service->RequestPermission(permission_result.GetCallback());
    EXPECT_EQ(result, permission_result.Get());
    if (result ==
        mojom::RequestPermissionResult::kPermissionPreviouslyGranted) {
      EXPECT_FALSE(open_system_settings_future_.IsReady());
    } else {
      EXPECT_TRUE(open_system_settings_future_.Wait());
      open_system_settings_future_.Clear();
    }
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
  base::test::TestFuture<void> open_system_settings_future_;
  UNAuthorizationStatus authorization_status_ =
      UNAuthorizationStatusNotDetermined;
  std::vector<mojom::PermissionStatus> reported_permission_statuses_;
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
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:[NSNull null], nil])])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  // Create and display a new notification.
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());

  run_loop.Run();
  EXPECT_EQ(result_future.Get(), mojom::DisplayNotificationResult::kSuccess);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Expect a new notification category for this notification.
  EXPECT_EQ(1u, category_count_);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_PermissionNotDetermined) {
  authorization_status_ = UNAuthorizationStatusNotDetermined;
  NSError* not_allowed_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];
  OCMExpect([mock_notification_center_
      addNotificationRequest:[OCMArg any]
       withCompletionHandler:([OCMArg
                                 invokeBlockWithArgs:not_allowed_error, nil])]);

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());
  EXPECT_EQ(result_future.Get(),
            mojom::DisplayNotificationResult::kPermissionNotDetermined);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(MacNotificationServiceUNTest, DisplayNotification_PermissionDenied) {
  authorization_status_ = UNAuthorizationStatusDenied;
  NSError* not_allowed_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];
  OCMExpect([mock_notification_center_
      addNotificationRequest:[OCMArg any]
       withCompletionHandler:([OCMArg
                                 invokeBlockWithArgs:not_allowed_error, nil])]);

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());
  EXPECT_EQ(result_future.Get(),
            mojom::DisplayNotificationResult::kPermissionDenied);
  EXPECT_THAT(reported_permission_statuses_,
              testing::ElementsAre(mojom::PermissionStatus::kDenied));
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // A subsequent successful delivery should immediately report the transition
  // back to `kGranted`.
  authorization_status_ = UNAuthorizationStatusAuthorized;
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true,
                          /*success=*/true);
  EXPECT_THAT(reported_permission_statuses_,
              testing::ElementsAre(mojom::PermissionStatus::kDenied,
                                   mojom::PermissionStatus::kGranted));
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_PermissionPromptPending) {
  authorization_status_ = UNAuthorizationStatusNotDetermined;
  __block void (^saved_auth_handler)(BOOL, NSError* _Nullable) = nil;
  base::RunLoop prompt_loop;
  OCMExpect(
      [mock_notification_center_
          requestAuthorizationWithOptions:0
                        completionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                              ^handler)(BOOL,
                                                        NSError* _Nullable)) {
                          saved_auth_handler = [handler copy];
                          return YES;
                        }]])
      .ignoringNonObjectArgs()
      .andDo(invokeClosure(prompt_loop.QuitClosure()));

  base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
  service_->RequestPermission(permission_result.GetCallback());
  prompt_loop.Run();

  NSError* not_allowed_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];
  OCMExpect([mock_notification_center_
      addNotificationRequest:[OCMArg any]
       withCompletionHandler:([OCMArg
                                 invokeBlockWithArgs:not_allowed_error, nil])]);

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());
  EXPECT_EQ(result_future.Get(),
            mojom::DisplayNotificationResult::kPermissionPromptPending);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  ASSERT_TRUE(saved_auth_handler);
  saved_auth_handler(YES, nil);
  saved_auth_handler = nil;
  EXPECT_TRUE(permission_result.Wait());
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_DestroyedDuringRenotifyFalseSynchronization) {
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/true);

  base::RunLoop sync_started_loop;
  OCMExpect([mock_notification_center_
                getDeliveredNotificationsWithCompletionHandler:[OCMArg any]])
      .andDo(invokeClosure(sync_started_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  notification->renotify = false;
  service_remote_->DisplayNotification(std::move(notification),
                                       base::DoNothing());
  sync_started_loop.Run();

  // Destroying the service while a `renotify = false` DisplayNotification
  // callback is queued in `synchronize_notifications_done_callbacks_` should
  // not trigger a Mojo `DCHECK(!connected)` crash.
  ResetService();
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_DestroyedWhileAddRequestInFlight) {
  __block void (^saved_completion_handler)(NSError* _Nullable) = nil;
  base::RunLoop add_started_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_completion_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(add_started_loop.QuitClosure()));

  service_remote_->DisplayNotification(
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/true),
      base::DoNothing());
  add_started_loop.Run();

  // Destroying `service_` while `saved_completion_handler` outlives
  // `~MacNotificationServiceUN()` (holding `DisplayNotificationCallback`) must
  // disconnect `binding_` first so dropping the block later does not trigger a
  // Mojo `DCHECK(!connected)` crash.
  EXPECT_TRUE(saved_completion_handler);
  ResetService();
  saved_completion_handler = nil;
}

TEST_F(MacNotificationServiceUNTest, DisplayNotificationWithIcon) {
  base::RunLoop run_loop;
  __block base::FilePath icon_path;
  __block void (^saved_completion_handler)(NSError* _Nullable) = nil;

  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                             UNNotificationRequest* request) {
            NSArray<UNNotificationAttachment*>* attachments =
                request.content.attachments;
            EXPECT_EQ(1u, attachments.count);
            if (attachments.count == 1u) {
              icon_path = base::apple::NSURLToFilePath(attachments[0].URL);
            }
            return YES;
          }]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_completion_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  service_remote_->DisplayNotification(std::move(notification),
                                       base::DoNothing());

  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  ASSERT_FALSE(icon_path.empty());
  EXPECT_TRUE(base::PathExists(icon_path));
  // The path must be canonicalized (no symlinks such as /var -> /private/var).
  EXPECT_EQ(icon_path, base::MakeAbsoluteFilePath(icon_path));

  // Even after 30 seconds, the file should still exist while the request is in
  // flight.
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_TRUE(base::PathExists(icon_path));

  // Once the completion handler is invoked, the temporary file is deleted.
  ASSERT_TRUE(saved_completion_handler);
  saved_completion_handler(nil);
  saved_completion_handler = nil;
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(icon_path));
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotificationWithIcon_RetriesWithoutAttachmentOnError) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  __block base::FilePath icon_path;

  NSError* attachment_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeAttachmentMoveIntoDataStoreFailed
                      userInfo:nil];

  // First attempt has the icon attachment and fails with an attachment error.
  OCMExpect([mock_notification_center_
      addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                         UNNotificationRequest* request) {
        NSArray<UNNotificationAttachment*>* attachments =
            request.content.attachments;
        if (attachments.count != 1u) {
          return NO;
        }
        icon_path = base::apple::NSURLToFilePath(attachments[0].URL);
        return YES;
      }]
       withCompletionHandler:([OCMArg
                                 invokeBlockWithArgs:attachment_error, nil])]);

  // Second attempt retries without attachments.
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                             UNNotificationRequest* request) {
            EXPECT_NSEQ(@"i|profileId|notificationId", request.identifier);
            EXPECT_NSEQ(@"title", request.content.title);
            return request.content.attachments.count == 0u;
          }]
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:[NSNull null], nil])])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());

  run_loop.Run();
  EXPECT_EQ(result_future.Get(), mojom::DisplayNotificationResult::kSuccess);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  ASSERT_FALSE(icon_path.empty());
  EXPECT_FALSE(base::PathExists(icon_path));
  histogram_tester.ExpectUniqueSample(
      "Notifications.macOS.DeliveryResult.Banner",
      UNErrorCodeAttachmentMoveIntoDataStoreFailed, 1);
  histogram_tester.ExpectUniqueSample(
      "Notifications.macOS.DeliveryResult.RetryWithoutAttachment.Banner", 0, 1);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotificationWithIcon_DoesNotRetryOnNonAttachmentError) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  __block base::FilePath icon_path;

  NSError* non_attachment_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];

  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                             UNNotificationRequest* request) {
            NSArray<UNNotificationAttachment*>* attachments =
                request.content.attachments;
            if (attachments.count != 1u) {
              return NO;
            }
            icon_path = base::apple::NSURLToFilePath(attachments[0].URL);
            return YES;
          }]
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:non_attachment_error,
                                                         nil])])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());

  run_loop.Run();
  EXPECT_EQ(result_future.Get(),
            mojom::DisplayNotificationResult::kDeliveryFailed);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  ASSERT_FALSE(icon_path.empty());
  EXPECT_FALSE(base::PathExists(icon_path));
  histogram_tester.ExpectUniqueSample(
      "Notifications.macOS.DeliveryResult.Banner",
      UNErrorCodeNotificationsNotAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "Notifications.macOS.DeliveryResult.RetryWithoutAttachment.Banner", 0);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotificationWithIcon_DoesNotRetryIfClosedBeforeCompletion) {
  base::HistogramTester histogram_tester;
  base::RunLoop display_loop;
  __block void (^saved_completion_handler)(NSError* _Nullable) = nil;

  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                             UNNotificationRequest* request) {
            return request.content.attachments.count == 1u;
          }]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_completion_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(display_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  service_remote_->DisplayNotification(std::move(notification),
                                       base::DoNothing());

  display_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Close the notification before the first attempt's completion handler runs.
  base::RunLoop close_loop;
  OCMExpect(
      [mock_notification_center_ removeDeliveredNotificationsWithIdentifiers:@[
        @"i|profileId|notificationId"
      ]])
      .andDo(invokeClosure(close_loop.QuitClosure()));
  service_remote_->CloseNotification(mojom::NotificationIdentifier::New(
      "notificationId",
      mojom::ProfileIdentifier::New("profileId", /*incognito=*/true)));
  close_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Now invoke the completion handler with an attachment error; it should not
  // retry since the notification was already closed.
  ASSERT_TRUE(saved_completion_handler);
  NSError* attachment_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeAttachmentMoveIntoDataStoreFailed
                      userInfo:nil];
  saved_completion_handler(attachment_error);
  saved_completion_handler = nil;
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  histogram_tester.ExpectUniqueSample(
      "Notifications.macOS.DeliveryResult.Banner",
      UNErrorCodeAttachmentMoveIntoDataStoreFailed, 1);
  histogram_tester.ExpectTotalCount(
      "Notifications.macOS.DeliveryResult.RetryWithoutAttachment.Banner", 0);
}

TEST_F(MacNotificationServiceUNTest,
       RedisplayNotificationWithIcon_RetriesWithoutAttachmentOnError) {
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/false,
                                             /*renotify=*/false);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  {
    OCMExpect([mock_notification_center_
        addNotificationRequest:[OCMArg any]
         withCompletionHandler:([OCMArg
                                   invokeBlockWithArgs:[NSNull null], nil])]);
    base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
    service_remote_->DisplayNotification(notification.Clone(),
                                         result_future.GetCallback());
    EXPECT_EQ(result_future.Get(), mojom::DisplayNotificationResult::kSuccess);
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    NSError* attachment_error =
        [NSError errorWithDomain:UNErrorDomain
                            code:UNErrorCodeAttachmentMoveIntoDataStoreFailed
                        userInfo:nil];

    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[
              CreateNotification("notificationId", "profileId",
                                 /*incognito=*/false, /*display=*/false)
            ],
                                         nil])]);

    // First replacement attempt includes the attachment and fails.
    OCMExpect([mock_notification_center_
        replaceContentForRequestWithIdentifier:@"r|profileId|notificationId"
                            replacementContent:
                                [OCMArg checkWithBlock:^BOOL(
                                            UNNotificationContent* content) {
                                  return content.attachments.count == 1u;
                                }]
                             completionHandler:
                                 ([OCMArg invokeBlockWithArgs:attachment_error,
                                                              nil])]);

    // Second replacement attempt retries without attachments.
    OCMExpect(
        [mock_notification_center_
            replaceContentForRequestWithIdentifier:@"r|profileId|notificationId"
                                replacementContent:
                                    [OCMArg
                                        checkWithBlock:^BOOL(
                                            UNNotificationContent* content) {
                                          return content.attachments.count ==
                                                 0u;
                                        }]
                                 completionHandler:
                                     ([OCMArg invokeBlockWithArgs:[NSNull null],
                                                                  nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));

    base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
    service_remote_->DisplayNotification(notification.Clone(),
                                         result_future.GetCallback());
    run_loop.Run();
    EXPECT_EQ(result_future.Get(), mojom::DisplayNotificationResult::kSuccess);
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    histogram_tester.ExpectUniqueSample(
        "Notifications.macOS.DeliveryResult.Banner",
        UNErrorCodeAttachmentMoveIntoDataStoreFailed, 1);
    histogram_tester.ExpectUniqueSample(
        "Notifications.macOS.DeliveryResult.RetryWithoutAttachment.Banner", 0,
        1);
  }
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
             withCompletionHandler:([OCMArg invokeBlockWithArgs:[NSNull null],
                                                                nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));

    service_remote_->DisplayNotification(notification_no_renotify.Clone(),
                                         base::DoNothing());

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
                                 completionHandler:
                                     ([OCMArg invokeBlockWithArgs:[NSNull null],
                                                                  nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_no_renotify.Clone(),
                                         base::DoNothing());

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
             withCompletionHandler:([OCMArg invokeBlockWithArgs:[NSNull null],
                                                                nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_with_renotify.Clone(),
                                         base::DoNothing());

    run_loop.Run();
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  {
    // Finally, display the notification with renotify set to false, but with
    // the notification having been closed by the OS in the meantime. Replacing
    // a notification whose previous toast was already dismissed must not emit a
    // synthetic kClose action to Chrome (which would delete the replacement
    // notification from the NotificationDatabase).
    base::RunLoop run_loop;
    EXPECT_CALL(mock_handler_, OnNotificationAction).Times(0);

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
             withCompletionHandler:([OCMArg invokeBlockWithArgs:[NSNull null],
                                                                nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    service_remote_->DisplayNotification(notification_no_renotify.Clone(),
                                         base::DoNothing());

    run_loop.Run();
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    testing::Mock::VerifyAndClearExpectations(&mock_handler_);
  }

  {
    // If the replaced notification is subsequently closed by the user before
    // the next synchronization pass, kClose should still be reported normally.
    base::RunLoop run_loop;
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    EXPECT_CALL(mock_handler_, OnNotificationAction)
        .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
          EXPECT_EQ(NotificationOperation::kClose, action_info->operation);
          EXPECT_EQ("notificationId", action_info->meta->id->id);
          run_loop.Quit();
        });
    task_environment_.FastForwardBy(
        MacNotificationServiceUN::kSynchronizationInterval);
    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    testing::Mock::VerifyAndClearExpectations(&mock_handler_);
  }
}

TEST_F(MacNotificationServiceUNTest, Rebind) {
  base::RunLoop run_loop;

  // Reconnnect to the same MacNotificationServiceUNTest instance.
  service_remote_.reset();
  service_->Bind(service_remote_.BindNewPipeAndPassReceiver());

  // Verify notification is created..
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:[NSNull null], nil])])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  // Create and display a new notification.
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/true);
  service_remote_->DisplayNotification(std::move(notification),
                                       base::DoNothing());

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

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_FailedDeliveryDoesNotEmitCloseOnSynchronize) {
  EXPECT_CALL(mock_handler_, OnNotificationAction).Times(0);

  // Simulate addNotificationRequest failing (e.g. when OS notification
  // permission is not granted).
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/false,
                          /*success=*/false);
  task_environment_.RunUntilIdle();

  // The category allocated for the failed notification should be released and
  // the subsequent synchronization pass must not synthesize a phantom kClose.
  EXPECT_EQ(0u, category_count_);
  base::RunLoop sync_loop;
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])])
      .andDo(invokeClosure(sync_loop.QuitClosure()));
  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  sync_loop.Run();
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_FailedAttachmentRetryDoesNotEmitCloseOnSynchronize) {
  EXPECT_CALL(mock_handler_, OnNotificationAction).Times(0);

  base::RunLoop run_loop;
  NSError* attachment_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeAttachmentMoveIntoDataStoreFailed
                      userInfo:nil];
  NSError* retry_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];

  OCMExpect([mock_notification_center_
      addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                         UNNotificationRequest* request) {
        return request.content.attachments.count == 1u;
      }]
       withCompletionHandler:([OCMArg
                                 invokeBlockWithArgs:attachment_error, nil])]);
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg checkWithBlock:^BOOL(
                                             UNNotificationRequest* request) {
            return request.content.attachments.count == 0u;
          }]
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:retry_error, nil])])
      .andDo(invokeClosure(run_loop.QuitClosure()));

  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/false);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseARGB(255, 100, 150, 200);
  notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
  service_remote_->DisplayNotification(std::move(notification),
                                       result_future.GetCallback());

  run_loop.Run();
  EXPECT_EQ(result_future.Get(),
            mojom::DisplayNotificationResult::kDeliveryFailed);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  EXPECT_EQ(0u, category_count_);

  base::RunLoop sync_loop;
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])])
      .andDo(invokeClosure(sync_loop.QuitClosure()));

  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  sync_loop.Run();
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_FailedReplaceContentKeepsTrackedNotification) {
  auto notification = CreateMojoNotification("notificationId", "profileId",
                                             /*incognito=*/false,
                                             /*renotify=*/false);
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/false,
                          /*success=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, category_count_);

  // Replace the notification in place (`renotify = false` while the previous
  // notification is still delivered), and simulate
  // `replaceContentForRequestWithIdentifier:` failing with a non-attachment
  // error.
  {
    base::RunLoop run_loop;
    NSError* replace_error =
        [NSError errorWithDomain:UNErrorDomain
                            code:UNErrorCodeNotificationsNotAllowed
                        userInfo:nil];
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
                                replacementContent:[OCMArg any]
                                 completionHandler:
                                     ([OCMArg invokeBlockWithArgs:replace_error,
                                                                  nil])])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    base::test::TestFuture<mojom::DisplayNotificationResult> result_future;
    service_remote_->DisplayNotification(notification.Clone(),
                                         result_future.GetCallback());
    run_loop.Run();
    EXPECT_EQ(result_future.Get(),
              mojom::DisplayNotificationResult::kDeliveryFailed);
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Because the existing notification remained on screen, it should still be
  // tracked and emit `kClose` when later dismissed.
  EXPECT_EQ(1u, category_count_);
  base::RunLoop close_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
        EXPECT_EQ(NotificationOperation::kClose, action_info->operation);
        EXPECT_EQ("notificationId", action_info->meta->id->id);
        close_loop.Quit();
      });
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  close_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);
}

TEST_F(MacNotificationServiceUNTest,
       DisplayNotification_StaleFailedDeliveryDoesNotEraseNewerNotification) {
  __block void (^first_completion_handler)(NSError* _Nullable) = nil;
  base::test::TestFuture<mojom::DisplayNotificationResult> first_result;
  {
    base::RunLoop run_loop;
    OCMExpect([mock_notification_center_
                  addNotificationRequest:[OCMArg any]
                   withCompletionHandler:[OCMArg checkWithBlock:^BOOL(
                                                     void (^completion_handler)(
                                                         NSError* _Nullable)) {
                     first_completion_handler = [completion_handler copy];
                     return YES;
                   }]])
        .andDo(invokeClosure(run_loop.QuitClosure()));
    auto notification = CreateMojoNotification("notificationId", "profileId",
                                               /*incognito=*/false);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(64, 64);
    bitmap.eraseARGB(255, 100, 150, 200);
    notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    service_remote_->DisplayNotification(std::move(notification),
                                         first_result.GetCallback());
    run_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Re-display the same notification ID before the first attempt's completion
  // handler runs, and let the second attempt succeed.
  DisplayNotificationSync("notificationId", "profileId", /*incognito=*/false,
                          /*success=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, category_count_);

  // Now invoke the first attempt's completion handler with an attachment error.
  // Because a newer display of the same notification ID is now tracked, the
  // stale failure must neither trigger an attachment-free retry nor erase the
  // newer notification, while still reporting the delivery failure.
  ASSERT_TRUE(first_completion_handler);
  first_completion_handler([NSError
      errorWithDomain:UNErrorDomain
                 code:UNErrorCodeAttachmentMoveIntoDataStoreFailed
             userInfo:nil]);
  first_completion_handler = nil;
  EXPECT_EQ(first_result.Get(),
            mojom::DisplayNotificationResult::kDeliveryFailed);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  EXPECT_EQ(1u, category_count_);

  // When the second notification is later dismissed by the user, synchronize
  // should still report its kClose action.
  base::RunLoop close_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
        EXPECT_EQ(NotificationOperation::kClose, action_info->operation);
        EXPECT_EQ("notificationId", action_info->meta->id->id);
        close_loop.Quit();
      });
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  close_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);
}

TEST_F(
    MacNotificationServiceUNTest,
    SynchronizesNotifications_DoesNotCloseNotificationWhileDeliveryInFlight) {
  EXPECT_CALL(mock_handler_, OnNotificationAction).Times(0);

  // Start displaying a notification, holding its `addNotificationRequest:`
  // completion handler so delivery remains in flight during synchronization.
  __block void (^saved_add_handler)(NSError* _Nullable) = nil;
  base::RunLoop display_started_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_add_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(display_started_loop.QuitClosure()));
  service_remote_->DisplayNotification(
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false),
      base::DoNothing());
  display_started_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  ASSERT_TRUE(saved_add_handler);
  EXPECT_EQ(1u, category_count_);

  // Run a periodic synchronization pass while delivery is still in flight; an
  // empty delivered list must not treat the in-flight notification as closed.
  base::RunLoop sync_loop;
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])])
      .andDo(invokeClosure(sync_loop.QuitClosure()));
  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  sync_loop.Run();
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  EXPECT_EQ(1u, category_count_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);

  // Once delivery finishes (`pending_content` is cleared), a subsequent
  // synchronization pass with an empty delivered list should report `kClose`.
  saved_add_handler(nil);
  saved_add_handler = nil;
  task_environment_.RunUntilIdle();

  base::RunLoop close_loop;
  EXPECT_CALL(mock_handler_, OnNotificationAction)
      .WillOnce([&](mojom::NotificationActionInfoPtr action_info) {
        EXPECT_EQ(NotificationOperation::kClose, action_info->operation);
        EXPECT_EQ("notificationId", action_info->meta->id->id);
        close_loop.Quit();
      });
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  task_environment_.FastForwardBy(
      MacNotificationServiceUN::kSynchronizationInterval);
  close_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  testing::Mock::VerifyAndClearExpectations(&mock_handler_);
}

TEST_F(
    MacNotificationServiceUNTest,
    GetDisplayedNotifications_IncludesInFlightAndBeingReplacedNotifications) {
  // 1. Display a notification whose `addNotificationRequest:` completes
  // immediately (`pending_content == nil && !is_being_replaced`), and another
  // notification that we will replace with `renotify = false`.
  DisplayNotificationSync("completedId", "profileId", /*incognito=*/false);
  DisplayNotificationSync("beingReplacedId", "profileId", /*incognito=*/false,
                          /*success=*/true, GURL("https://example.com"));
  task_environment_.RunUntilIdle();

  // Start replacing `beingReplacedId` with `renotify = false`, holding the
  // pre-replacement `SynchronizeNotifications` callback so `is_being_replaced`
  // remains true while `pending_content` is still nil.
  __block void (^saved_sync_handler)(NSArray<UNNotification*>* _Nonnull) = nil;
  base::test::TestFuture<mojom::DisplayNotificationResult> replace_result;
  {
    base::RunLoop sync_started_loop;
    OCMExpect([mock_notification_center_
                  getDeliveredNotificationsWithCompletionHandler:
                      [OCMArg checkWithBlock:^BOOL(void (^completion_handler)(
                          NSArray<UNNotification*>* _Nonnull)) {
                        saved_sync_handler = [completion_handler copy];
                        return YES;
                      }]])
        .andDo(invokeClosure(sync_started_loop.QuitClosure()));
    service_remote_->DisplayNotification(
        CreateMojoNotification("beingReplacedId", "profileId",
                               /*incognito=*/false, /*renotify=*/false,
                               GURL("https://example.com")),
        replace_result.GetCallback());
    sync_started_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    ASSERT_TRUE(saved_sync_handler);
  }

  // 2. Start two new notifications, holding their `addNotificationRequest:`
  // completion handlers so both remain in flight (`pending_content != nil`).
  __block void (^saved_add_handler1)(NSError* _Nullable) = nil;
  base::test::TestFuture<mojom::DisplayNotificationResult> add_result1;
  {
    base::RunLoop display_started_loop;
    OCMExpect([mock_notification_center_
                  addNotificationRequest:[OCMArg any]
                   withCompletionHandler:[OCMArg checkWithBlock:^BOOL(
                                                     void (^completion_handler)(
                                                         NSError* _Nullable)) {
                     saved_add_handler1 = [completion_handler copy];
                     return YES;
                   }]])
        .andDo(invokeClosure(display_started_loop.QuitClosure()));
    service_remote_->DisplayNotification(
        CreateMojoNotification("inFlightId1", "profileId", /*incognito=*/false,
                               /*renotify=*/true, GURL("https://example.com")),
        add_result1.GetCallback());
    display_started_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    ASSERT_TRUE(saved_add_handler1);
  }

  __block void (^saved_add_handler2)(NSError* _Nullable) = nil;
  base::test::TestFuture<mojom::DisplayNotificationResult> add_result2;
  {
    base::RunLoop display_started_loop;
    OCMExpect([mock_notification_center_
                  addNotificationRequest:[OCMArg any]
                   withCompletionHandler:[OCMArg checkWithBlock:^BOOL(
                                                     void (^completion_handler)(
                                                         NSError* _Nullable)) {
                     saved_add_handler2 = [completion_handler copy];
                     return YES;
                   }]])
        .andDo(invokeClosure(display_started_loop.QuitClosure()));
    service_remote_->DisplayNotification(
        CreateMojoNotification("inFlightId2", "profileId", /*incognito=*/true,
                               /*renotify=*/true, GURL("https://gmail.com")),
        add_result2.GetCallback());
    display_started_loop.Run();
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
    ASSERT_TRUE(saved_add_handler2);
  }

  auto to_mac_ids =
      [](const std::vector<mojom::NotificationIdentifierPtr>& notifications) {
        std::vector<std::string> ids;
        for (const auto& notification : notifications) {
          ids.push_back(DeriveMacNotificationId(notification));
        }
        return ids;
      };

  // Query all displayed notifications when `UNUserNotificationCenter` returns
  // an empty array: `beingReplacedId`, `inFlightId1`, and `inFlightId2` must
  // be included, while `completedId` (no longer in flight and absent from OS)
  // is not.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> all_displayed =
        GetDisplayedNotificationsSync(/*profile=*/nullptr);
    EXPECT_THAT(to_mac_ids(all_displayed),
                testing::UnorderedElementsAre("r|profileId|beingReplacedId",
                                              "r|profileId|inFlightId1",
                                              "i|profileId|inFlightId2"));
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Filter by non-incognito `profileId`: only `beingReplacedId` and
  // `inFlightId1` match.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> profile_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("profileId", /*incognito=*/false));
    EXPECT_THAT(to_mac_ids(profile_displayed),
                testing::UnorderedElementsAre("r|profileId|beingReplacedId",
                                              "r|profileId|inFlightId1"));
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Filter by non-incognito `profileId` and matching origin: both
  // `beingReplacedId` and `inFlightId1` match.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> profile_and_origin_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("profileId", /*incognito=*/false),
            GURL("https://example.com"));
    EXPECT_THAT(to_mac_ids(profile_and_origin_displayed),
                testing::UnorderedElementsAre("r|profileId|beingReplacedId",
                                              "r|profileId|inFlightId1"));
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Filter by an unrelated profile ID: none match.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> other_profile_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("otherProfile", /*incognito=*/false));
    EXPECT_TRUE(other_profile_displayed.empty());
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Filter by incognito `profileId` and a non-matching origin: none match.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> wrong_origin_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("profileId", /*incognito=*/true),
            GURL("https://example.com"));
    EXPECT_TRUE(wrong_origin_displayed.empty());
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Filter by incognito `profileId` and matching origin: `inFlightId2` matches.
  {
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> matching_origin_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("profileId", /*incognito=*/true),
            GURL("https://gmail.com"));
    EXPECT_THAT(to_mac_ids(matching_origin_displayed),
                testing::UnorderedElementsAre("i|profileId|inFlightId2"));
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // If `UNUserNotificationCenter` already returns `inFlightId1` while its
  // completion callback is still pending on `sequence_checker_`, it must be
  // deduplicated rather than returned twice.
  {
    FakeUNNotification* os_delivered = CreateNotification(
        "inFlightId1", "profileId", /*incognito=*/false, /*display=*/false,
        /*category_id=*/"", GURL("https://example.com"));
    OCMExpect([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            ([OCMArg invokeBlockWithArgs:@[ os_delivered ], nil])]);
    std::vector<mojom::NotificationIdentifierPtr> deduplicated_displayed =
        GetDisplayedNotificationsSync(
            mojom::ProfileIdentifier::New("profileId", /*incognito=*/false));
    EXPECT_THAT(to_mac_ids(deduplicated_displayed),
                testing::UnorderedElementsAre("r|profileId|beingReplacedId",
                                              "r|profileId|inFlightId1"));
    EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  }

  // Complete all pending handlers cleanly.
  saved_add_handler1(nil);
  saved_add_handler1 = nil;
  saved_add_handler2(nil);
  saved_add_handler2 = nil;
  base::RunLoop replace_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:([OCMArg
                                     invokeBlockWithArgs:[NSNull null], nil])])
      .andDo(invokeClosure(replace_loop.QuitClosure()));
  saved_sync_handler(@[]);
  saved_sync_handler = nil;
  replace_loop.Run();
  EXPECT_EQ(add_result1.Get(), mojom::DisplayNotificationResult::kSuccess);
  EXPECT_EQ(add_result2.Get(), mojom::DisplayNotificationResult::kSuccess);
  EXPECT_EQ(replace_result.Get(), mojom::DisplayNotificationResult::kSuccess);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(MacNotificationServiceUNTest,
       OkayToTerminateService_FalseWhileDeliveryInFlight) {
  __block void (^saved_add_handler)(NSError* _Nullable) = nil;
  base::RunLoop display_started_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_add_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(display_started_loop.QuitClosure()));

  base::test::TestFuture<mojom::DisplayNotificationResult> display_result;
  service_remote_->DisplayNotification(
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false),
      display_result.GetCallback());
  display_started_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  ASSERT_TRUE(saved_add_handler);

  // While `addNotificationRequest:` is in flight (`pending_content != nil`),
  // `OkayToTerminateService` queries `GetDisplayedNotifications`, which merges
  // the in-flight entry and causes `OkayToTerminateService` to return false.
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  base::test::TestFuture<bool> can_terminate_in_flight;
  service_remote_->OkayToTerminateService(
      can_terminate_in_flight.GetCallback());
  EXPECT_FALSE(can_terminate_in_flight.Get());
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Complete the delivery and simulate the notification subsequently being
  // dismissed from the OS notification center.
  saved_add_handler(nil);
  saved_add_handler = nil;
  EXPECT_EQ(display_result.Get(), mojom::DisplayNotificationResult::kSuccess);
  task_environment_.RunUntilIdle();

  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  base::test::TestFuture<bool> can_terminate_after_completion;
  service_remote_->OkayToTerminateService(
      can_terminate_after_completion.GetCallback());
  EXPECT_TRUE(can_terminate_after_completion.Get());
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(MacNotificationServiceUNTest,
       OkayToTerminateService_FalseWhenDisplayArrivesDuringCheck) {
  __block void (^saved_get_delivered_handler)(
      NSArray<UNNotification*>* _Nonnull) = nil;
  base::RunLoop get_delivered_started_loop;
  OCMExpect([mock_notification_center_
                getDeliveredNotificationsWithCompletionHandler:
                    [OCMArg checkWithBlock:^BOOL(void (^completion_handler)(
                        NSArray<UNNotification*>* _Nonnull)) {
                      saved_get_delivered_handler = [completion_handler copy];
                      return YES;
                    }]])
      .andDo(invokeClosure(get_delivered_started_loop.QuitClosure()));

  base::test::TestFuture<bool> can_terminate_future;
  service_remote_->OkayToTerminateService(can_terminate_future.GetCallback());
  get_delivered_started_loop.Run();
  ASSERT_TRUE(saved_get_delivered_handler);

  // Dispatch a notification while `OkayToTerminateService`'s
  // `GetDisplayedNotifications` query is in flight.
  __block void (^saved_add_handler)(NSError* _Nullable) = nil;
  base::RunLoop display_started_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_add_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(display_started_loop.QuitClosure()));
  base::test::TestFuture<mojom::DisplayNotificationResult> display_result;
  service_remote_->DisplayNotification(
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false),
      display_result.GetCallback());
  display_started_loop.Run();
  ASSERT_TRUE(saved_add_handler);

  // Even though `UNUserNotificationCenter` returns an empty array for the
  // earlier query, `OkayToTerminateService` must return false because a
  // delivery is now in flight.
  saved_get_delivered_handler(@[]);
  saved_get_delivered_handler = nil;
  EXPECT_FALSE(can_terminate_future.Get());

  saved_add_handler(nil);
  saved_add_handler = nil;
  EXPECT_EQ(display_result.Get(), mojom::DisplayNotificationResult::kSuccess);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // If `service_` is destroyed while `OkayToTerminateService`'s
  // `GetDisplayedNotifications` query is in flight, the reply callback falls
  // back to `notifications.empty()` without dereferencing the invalidated weak
  // pointer.
  OCMExpect([mock_notification_center_
      getDeliveredNotificationsWithCompletionHandler:
          [OCMArg checkWithBlock:^BOOL(void (^completion_handler)(
              NSArray<UNNotification*>* _Nonnull)) {
            saved_get_delivered_handler = [completion_handler copy];
            return YES;
          }]]);
  base::test::TestFuture<bool> can_terminate_after_destroy;
  service_->OkayToTerminateService(can_terminate_after_destroy.GetCallback());
  ASSERT_TRUE(saved_get_delivered_handler);
  ResetService();
  saved_get_delivered_handler(@[]);
  saved_get_delivered_handler = nil;
  EXPECT_TRUE(can_terminate_after_destroy.Get());
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(MacNotificationServiceUNTest,
       OkayToTerminateService_FalseWhileFailedDeliveryResultInFlight) {
  authorization_status_ = UNAuthorizationStatusDenied;

  __block void (^saved_get_delivered_handler)(
      NSArray<UNNotification*>* _Nonnull) = nil;
  base::RunLoop get_delivered_started_loop;
  OCMExpect([mock_notification_center_
                getDeliveredNotificationsWithCompletionHandler:
                    [OCMArg checkWithBlock:^BOOL(void (^completion_handler)(
                        NSArray<UNNotification*>* _Nonnull)) {
                      saved_get_delivered_handler = [completion_handler copy];
                      return YES;
                    }]])
      .andDo(invokeClosure(get_delivered_started_loop.QuitClosure()));

  base::test::TestFuture<bool> can_terminate_future;
  service_remote_->OkayToTerminateService(can_terminate_future.GetCallback());
  get_delivered_started_loop.Run();
  ASSERT_TRUE(saved_get_delivered_handler);

  __block void (^saved_add_handler)(NSError* _Nullable) = nil;
  base::RunLoop display_started_loop;
  OCMExpect(
      [mock_notification_center_
          addNotificationRequest:[OCMArg any]
           withCompletionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                     ^completion_handler)(NSError* _Nullable)) {
             saved_add_handler = [completion_handler copy];
             return YES;
           }]])
      .andDo(invokeClosure(display_started_loop.QuitClosure()));

  base::test::TestFuture<mojom::DisplayNotificationResult> display_result;
  service_remote_->DisplayNotification(
      CreateMojoNotification("notificationId", "profileId",
                             /*incognito=*/false),
      display_result.GetCallback());
  display_started_loop.Run();
  ASSERT_TRUE(saved_add_handler);

  // Fail `addNotificationRequest:` right before `getDeliveredNotifications`
  // completes. `OnNotificationDelivered` runs first, removing the failed
  // notification from `delivered_notifications_` and posting the settings
  // lookup reply behind `OkayToTerminateService`'s continuation. Even though
  // `delivered_notifications_` is now empty, `OkayToTerminateService` must
  // still return false until `DisplayNotification`'s result callback has run.
  NSError* not_allowed_error =
      [NSError errorWithDomain:UNErrorDomain
                          code:UNErrorCodeNotificationsNotAllowed
                      userInfo:nil];
  saved_add_handler(not_allowed_error);
  saved_add_handler = nil;
  saved_get_delivered_handler(@[]);
  saved_get_delivered_handler = nil;

  EXPECT_FALSE(can_terminate_future.Get());
  EXPECT_EQ(display_result.Get(),
            mojom::DisplayNotificationResult::kPermissionDenied);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  // Once the `DisplayNotification` callback has completed and no notifications
  // are displayed, `OkayToTerminateService` returns true.
  OCMExpect(
      [mock_notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                     ([OCMArg invokeBlockWithArgs:@[], nil])]);
  base::test::TestFuture<bool> can_terminate_after_failure;
  service_remote_->OkayToTerminateService(
      can_terminate_after_failure.GetCallback());
  EXPECT_TRUE(can_terminate_after_failure.Get());
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
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
  EXPECT_TRUE(open_system_settings_future_.Wait());
}

TEST_F(MacNotificationServiceUNTest,
       PermissionStateChangedCallback_RequestPermissionPreviouslyDenied) {
  id mock_notification_center = CreateMockNotificationCenter(
      CreateMockNotificationSettings(UNAuthorizationStatusDenied));

  MockNotificationActionHandler mock_handler;
  mojo::Receiver<mojom::MacNotificationActionHandler> handler_receiver{
      &mock_handler};

  mojom::PermissionStatus status = mojom::PermissionStatus::kNotDetermined;
  auto service = std::make_unique<MacNotificationServiceUN>(
      handler_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::PermissionStatus new_status) {
        EXPECT_NE(new_status, mojom::PermissionStatus::kPromptPending);
        status = new_status;
      }),
      mock_notification_center);

  // Call `RequestPermission` immediately while the initial
  // `SynchronizePermissionStatus` reply from the constructor is still pending
  // on the task runner (so the sync reply arrives while
  // `pending_permission_requests_` is non-empty and before the
  // `RequestPermission` settings callback). `kPromptPending` should not be
  // reported.
  {
    base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
    service->RequestPermission(permission_result.GetCallback());
    EXPECT_EQ(permission_result.Get(),
              mojom::RequestPermissionResult::kPermissionPreviouslyDenied);
    EXPECT_EQ(status, mojom::PermissionStatus::kDenied);
    EXPECT_TRUE(open_system_settings_future_.Wait());
    open_system_settings_future_.Clear();
  }

  // Calling `RequestPermission` again after the status is already synchronized
  // should also not report `kPromptPending`.
  {
    base::test::TestFuture<mojom::RequestPermissionResult> permission_result;
    service->RequestPermission(permission_result.GetCallback());
    EXPECT_EQ(permission_result.Get(),
              mojom::RequestPermissionResult::kPermissionPreviouslyDenied);
    EXPECT_EQ(status, mojom::PermissionStatus::kDenied);
    EXPECT_TRUE(open_system_settings_future_.Wait());
    open_system_settings_future_.Clear();
  }
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

TEST_F(MacNotificationServiceUNTest,
       PermissionStateChangedCallback_ApplicationDidBecomeActive) {
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

  // Becoming active without a status change should not run the callback.
  // Only advance mock time by a second (well below the 10 minute periodic
  // synchronization interval), so only the activation can trigger a poll.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSApplicationDidBecomeActiveNotification
                    object:nil];
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(status.IsReady());

  // A status change is picked up as soon as the app becomes active.
  current_status = UNAuthorizationStatusDenied;
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSApplicationDidBecomeActiveNotification
                    object:nil];
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(status.IsReady());
  EXPECT_EQ(status.Take(), mojom::PermissionStatus::kDenied);

  // After the service is destroyed, becoming active is a no-op.
  service.reset();
  current_status = UNAuthorizationStatusAuthorized;
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSApplicationDidBecomeActiveNotification
                    object:nil];
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(status.IsReady());
}

}  // namespace mac_notifications
