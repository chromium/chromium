// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace {
constexpr char kGoogleDriveUrl[] = "https://drive.google.com/";
constexpr char kGooglePhotosUrl[] = "https://photos.google.com/";

constexpr char kNotificationDisplaySource[] = "NotificationDisplaySource";
constexpr char kNotificationTitle[] = "NotificationTitle";
constexpr char kNotificationMessage[] = "NotificationMessage";
constexpr char kGetPerkButtonTitle[] = "GetPerkButtonTitle";

using TestEnvironment =
    ::ash::CustomizableTestEnvBrowserTestBase::TestEnvironment;
using UserSessionType =
    ::ash::CustomizableTestEnvBrowserTestBase::UserSessionType;

class GoogleOneOfferIphTabHelperTest
    : public ash::CustomizableTestEnvBrowserTestBase {
 public:
  // ash::CustomizableTestEnvBrowserTestBase:
  void SetUp() override {
    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(&SetMockTrackerFactory));

    ash::CustomizableTestEnvBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    // `ash::CustomizableTestEnvBrowserTestBase::SetUpOnMainThread` must be
    // called before our `SetUpOnMainThread` as login happens in the method,
    // i.e. profile is not available before it.
    ash::CustomizableTestEnvBrowserTestBase::SetUpOnMainThread();
    CHECK(browser()->profile());

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<feature_engagement::test::ScopedIphFeatureList>
      scoped_iph_feature_list_;

 private:
  static void SetMockTrackerFactory(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &GoogleOneOfferIphTabHelperTest::CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* browser_context) {
    auto mock_tracker =
        std::make_unique<feature_engagement::test::MockTracker>();
    ON_CALL(*mock_tracker,
            ShouldTriggerHelpUI(testing::Ref(
                feature_engagement::kIPHGoogleOneOfferNotificationFeature)))
        .WillByDefault(testing::Return(true));
    return mock_tracker;
  }

  base::CallbackListSubscription subscription_;
};

class GoogleOneOfferIphTabHelperTestWithUIStringParams
    : public GoogleOneOfferIphTabHelperTest {
 public:
  void SetUp() override {
    base::FieldTrialParams params;
    params[kNotificationDisplaySourceParamName] = kNotificationDisplaySource;
    params[kNotificationTitleParamName] = kNotificationTitle;
    params[kNotificationMessageParamName] = kNotificationMessage;
    params[kGetPerkButtonTitleParamName] = kGetPerkButtonTitle;

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHGoogleOneOfferNotificationFeature, params);

    GoogleOneOfferIphTabHelperTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GoogleOneOfferIphTabHelperTestWithUIStringParams,
                       UIStringParams) {
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  ASSERT_TRUE(nullptr !=
              ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDriveUrl)));
  added_run_loop.Run();

  std::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->display_source(),
            base::UTF8ToUTF16(std::string_view(kNotificationDisplaySource)));
  EXPECT_EQ(notification->title(),
            base::UTF8ToUTF16(std::string_view(kNotificationTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(std::string_view(kNotificationMessage)));
  ASSERT_EQ(notification->rich_notification_data().buttons.size(), 1ul);
  EXPECT_EQ(notification->rich_notification_data().buttons[0].title,
            base::UTF8ToUTF16(std::string_view(kGetPerkButtonTitle)));
}

IN_PROC_BROWSER_TEST_F(GoogleOneOfferIphTabHelperTest,
                       NotificationOnGoogleDriveClickGetPerk) {
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDriveUrl)) !=
              nullptr);
  added_run_loop.Run();

  // Make sure that fallback texts are set if UI strings are not provided via
  // params. Note that UI strings should be provided via params on prod and
  // fallback texts should not be used. This is to test fail-safe case.
  std::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(
      notification->display_source(),
      base::UTF8ToUTF16(std::string_view(kFallbackNotificationDisplaySource)));
  EXPECT_EQ(notification->title(),
            base::UTF8ToUTF16(std::string_view(kFallbackNotificationTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(std::string_view(kFallbackNotificationMessage)));
  ASSERT_EQ(notification->rich_notification_data().buttons.size(), 1ul);
  EXPECT_EQ(notification->rich_notification_data().buttons[0].title,
            base::UTF8ToUTF16(std::string_view(kFallbackGetPerkButtonTitle)));

  EXPECT_EQ(notification->notifier_id().id, kIPHGoogleOneOfferNotifierId);

  raw_ptr<feature_engagement::test::MockTracker> mock_tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              browser()->profile()));
  EXPECT_CALL(
      *mock_tracker,
      NotifyEvent(testing::Eq(kIPHGoogleOneOfferNotificationDismissEventName)))
      .Times(0);
  EXPECT_CALL(
      *mock_tracker,
      NotifyEvent(testing::Eq(kIPHGoogleOneOfferNotificationGetPerkEventName)));
  EXPECT_CALL(*mock_tracker,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHGoogleOneOfferNotificationFeature)));

  base::RunLoop closed_run_loop;
  display_service_tester_->SetNotificationClosedClosure(
      closed_run_loop.QuitClosure());
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kIPHGoogleOneOfferNotificationId,
                                         kGetPerkButtonIndex, std::nullopt);
  closed_run_loop.Run();
  tab_added_waiter.Wait();
  EXPECT_EQ(GURL(kGoogleOneOfferUrl),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(GoogleOneOfferIphTabHelperTest,
                       NotificationOnGooglePhotos) {
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  ASSERT_TRUE(nullptr !=
              ui_test_utils::NavigateToURL(browser(), GURL(kGooglePhotosUrl)));
  added_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GoogleOneOfferIphTabHelperTest, NotificationDismiss) {
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  ASSERT_TRUE(nullptr !=
              ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDriveUrl)));
  added_run_loop.Run();

  raw_ptr<feature_engagement::test::MockTracker> mock_tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              browser()->profile()));
  EXPECT_CALL(
      *mock_tracker,
      NotifyEvent(testing::Eq(kIPHGoogleOneOfferNotificationGetPerkEventName)))
      .Times(0);
  EXPECT_CALL(
      *mock_tracker,
      NotifyEvent(testing::Eq(kIPHGoogleOneOfferNotificationDismissEventName)));
  EXPECT_CALL(*mock_tracker,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHGoogleOneOfferNotificationFeature)));

  // Remove a notification as a user event. `RemoveNotification` does not
  // trigger notification closed closure which can be set with
  // `NotificationDisplayServiceTester::SetNotificationClosedClosure`.
  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::TRANSIENT, kIPHGoogleOneOfferNotificationId,
      /*by_user=*/true);
  EXPECT_EQ(GURL(kGoogleDriveUrl),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

class GoogleOneOfferIphTabHelperTestParameterized
    : public GoogleOneOfferIphTabHelperTest,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  void SetUp() override {
    SetTestEnvironment(GetParam());

    GoogleOneOfferIphTabHelperTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    DoNotShowNotification,
    GoogleOneOfferIphTabHelperTestParameterized,
    testing::Values(
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED,
            UserSessionType::kManaged),
        // A test case where a regular profile on a managed device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED,
            UserSessionType::kRegular),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kGuest),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kChild),
        // A test case where a child profile is an owner of a device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kChildOwner),
        // A Test case where a managed account is an owner of an un-enrolled
        // device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kManaged),
        // A test case where we do not show a notification if a profile is not
        // an owner profile.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kRegularNonOwner)),
    &TestEnvironment::GenerateTestName);

IN_PROC_BROWSER_TEST_P(GoogleOneOfferIphTabHelperTestParameterized,
                       NoNotification) {
  ASSERT_TRUE(nullptr !=
              ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDriveUrl)));

  std::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  EXPECT_EQ(std::nullopt, notification);
}

}  // namespace
