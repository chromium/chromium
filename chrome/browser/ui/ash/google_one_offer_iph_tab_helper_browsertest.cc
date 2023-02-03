// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/google_one_offer_iph_tab_helper_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace {
constexpr char kOwnerEmail[] = "test@example.com";

constexpr char kGoogleDriveUrl[] = "https://drive.google.com/";
constexpr char kGooglePhotosUrl[] = "https://photos.google.com/";

constexpr char kNotificationDisplaySource[] = "NotificationDisplaySource";
constexpr char kNotificationTitle[] = "NotificationTitle";
constexpr char kNotificationMessage[] = "NotificationMessage";
constexpr char kGetPerkButtonTitle[] = "GetPerkButtonTitle";

enum UserSessionType {
  kGuest,
  kChild,
  kChildOwner,
  kRegular,
  kRegularNonOwner,
  kManaged
};

class GoogleOneOfferIphTabHelperTest : public MixinBasedInProcessBrowserTest {
 public:
  // MixinBasedInProcessBrowserTest:
  void SetUp() override {
    device_state_mixin_ =
        std::make_unique<ash::DeviceStateMixin>(&mixin_host_, device_state_);

    switch (user_session_type_) {
      case kGuest:
        guest_session_mixin_ =
            std::make_unique<ash::GuestSessionMixin>(&mixin_host_);
        break;
      case kChild:
        logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
            &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
            embedded_test_server(), this, /*should_launch_browser=*/false);
        break;
      case kChildOwner:
        logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
            &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
            embedded_test_server(), this, /*should_launch_browser=*/false);
        owner_user_email_ =
            logged_in_user_mixin_->GetAccountId().GetUserEmail();
        break;
      case kManaged:
        logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
            &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
            embedded_test_server(), this, /*should_launch_browser=*/false,
            AccountId::FromUserEmailGaiaId(
                FakeGaiaMixin::kEnterpriseUser1,
                FakeGaiaMixin::kEnterpriseUser1GaiaId));

        // If a device is not enrolled, simulate a case where a device is owned
        // by the managed account. Note that an account cannot be an onwer of a
        // device if a device is enrolled.
        if (device_state_ ==
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED) {
          owner_user_email_ =
              logged_in_user_mixin_->GetAccountId().GetUserEmail();
        }
        break;
      case kRegular:
        logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
            &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
            embedded_test_server(), this);
        owner_user_email_ =
            logged_in_user_mixin_->GetAccountId().GetUserEmail();
        break;
      case kRegularNonOwner:
        logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
            &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
            embedded_test_server(), this);

        CHECK(kOwnerEmail !=
              logged_in_user_mixin_->GetAccountId().GetUserEmail());
        owner_user_email_ = kOwnerEmail;
        break;
    }

    if (!owner_user_email_.empty()) {
      scoped_testing_cros_settings_.device_settings()->Set(
          ash::kDeviceOwner, base::Value(owner_user_email_));
    }

    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(&SetMockTrackerFactory));

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (logged_in_user_mixin_) {
      logged_in_user_mixin_->LogInUser();
    }

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  ash::DeviceStateMixin::State device_state_ =
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED;
  UserSessionType user_session_type_ = kRegular;
  std::string owner_user_email_;

  std::unique_ptr<ash::GuestSessionMixin> guest_session_mixin_;
  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
  std::unique_ptr<ash::DeviceStateMixin> device_state_mixin_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<feature_engagement::test::ScopedIphFeatureList>
      scoped_iph_feature_list_;

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

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

  absl::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->display_source(),
            base::UTF8ToUTF16(base::StringPiece(kNotificationDisplaySource)));
  EXPECT_EQ(notification->title(),
            base::UTF8ToUTF16(base::StringPiece(kNotificationTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(base::StringPiece(kNotificationMessage)));
  ASSERT_EQ(notification->rich_notification_data().buttons.size(), 1ul);
  EXPECT_EQ(notification->rich_notification_data().buttons[0].title,
            base::UTF8ToUTF16(base::StringPiece(kGetPerkButtonTitle)));
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
  absl::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(
      notification->display_source(),
      base::UTF8ToUTF16(base::StringPiece(kFallbackNotificationDisplaySource)));
  EXPECT_EQ(notification->title(),
            base::UTF8ToUTF16(base::StringPiece(kFallbackNotificationTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(base::StringPiece(kFallbackNotificationMessage)));
  ASSERT_EQ(notification->rich_notification_data().buttons.size(), 1ul);
  EXPECT_EQ(notification->rich_notification_data().buttons[0].title,
            base::UTF8ToUTF16(base::StringPiece(kFallbackGetPerkButtonTitle)));

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
                                         kGetPerkButtonIndex, absl::nullopt);
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

// Test param for `GoogleOneOfferIphTabHelperTestParameterized`. This allows you
// to run a test in various device and account set up states.
class TestEnvironment {
 public:
  TestEnvironment(ash::DeviceStateMixin::State device_state,
                  UserSessionType user_session_type)
      : device_state_(device_state), user_session_type_(user_session_type) {}

  ash::DeviceStateMixin::State device_state() const { return device_state_; }
  UserSessionType session_state() const { return user_session_type_; }

  std::string GetTestName() const {
    std::string test_name;
    switch (device_state_) {
      case ash::DeviceStateMixin::State::BEFORE_OOBE:
        test_name += "BEFORE_OOBE";
        break;
      case ash::DeviceStateMixin::State::
          OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
        test_name += "OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED";
        break;
      case ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
        test_name += "OOBE_COMPLETED_CLOUD_ENROLLED";
        break;
      case ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
        test_name += "OOBE_COMPLETED_CONSUMER_OWNED";
        break;
      case ash::DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
        test_name += "OOBE_COMPLETED_PERMANENTLY_UNOWNED";
        break;
      case ash::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
        test_name += "OOBE_COMPLETED_UNOWNED";
        break;
      case ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
        test_name += "OOBE_COMPLETED_DEMO_MODE";
        break;
    }

    switch (user_session_type_) {
      case kRegular:
        test_name += "_REGULAR";
        break;
      case kRegularNonOwner:
        test_name += "_REGULAR_NON_OWNER";
        break;
      case kGuest:
        test_name += "_GUEST";
        break;
      case kChild:
        test_name += "_CHILD";
        break;
      case kChildOwner:
        test_name += "_CHILD_OWNER";
        break;
      case kManaged:
        test_name += "_MANAGED";
        break;
    }
    return test_name;
  }

 private:
  ash::DeviceStateMixin::State device_state_;
  UserSessionType user_session_type_;
};

class GoogleOneOfferIphTabHelperTestParameterized
    : public GoogleOneOfferIphTabHelperTest,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  void SetUp() override {
    TestEnvironment test_environment = GetParam();
    device_state_ = test_environment.device_state();
    user_session_type_ = test_environment.session_state();

    GoogleOneOfferIphTabHelperTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    DoNotShowNotification,
    GoogleOneOfferIphTabHelperTestParameterized,
    testing::Values(
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED,
            kRegular),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            kGuest),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            kChild),
        // A test case where a child profile is an owner of a device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            kChildOwner),
        // A Test case where a managed account is an owner of an un-enrolled
        // device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            kManaged),
        // A test case where we do not show a notification if a profile is not
        // an owner profile.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            kRegularNonOwner)),
    [](testing::TestParamInfo<TestEnvironment> param_info) {
      return param_info.param.GetTestName();
    });

IN_PROC_BROWSER_TEST_P(GoogleOneOfferIphTabHelperTestParameterized,
                       NoNotification) {
  ASSERT_TRUE(nullptr !=
              ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDriveUrl)));

  absl::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(
          kIPHGoogleOneOfferNotificationId);
  EXPECT_EQ(absl::nullopt, notification);
}

}  // namespace
