// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/google_one_offer_iph_tab_helper.h"

#include <memory>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/ash/google_one_offer_iph_tab_helper_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace {
constexpr char kGoogleDriveUrl[] = "https://drive.google.com/";
constexpr char kGooglePhotosUrl[] = "https://photos.google.com/";

class MockNewWindowDelegate : public ash::TestNewWindowDelegate {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class GoogleOneOfferIphTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &GoogleOneOfferIphTabHelperTest::CreateMockTracker));
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

 private:
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

  base::test::ScopedFeatureList scoped_feature_list_{
      feature_engagement::kIPHGoogleOneOfferNotificationFeature};
};

TEST_F(GoogleOneOfferIphTabHelperTest, NotificationOnGoogleDriveClickGetPerk) {
  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents());

  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  NavigateAndCommit(GURL(kGoogleDriveUrl));
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

  std::unique_ptr<MockNewWindowDelegate> new_window_delegate =
      std::make_unique<MockNewWindowDelegate>();
  EXPECT_CALL(
      *new_window_delegate,
      OpenUrl(testing::Eq(GURL(kGoogleOneOfferUrl)), testing::_, testing::_));
  ash::TestNewWindowDelegateProvider delegate_provider(
      std::move(new_window_delegate));

  raw_ptr<feature_engagement::test::MockTracker> mock_tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile()));
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
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kIPHGoogleOneOfferNotificationId,
                                         kGetPerkButtonIndex, absl::nullopt);
  closed_run_loop.Run();
}

TEST_F(GoogleOneOfferIphTabHelperTest, NotificationOnGooglePhotos) {
  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents());

  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  NavigateAndCommit(GURL(kGooglePhotosUrl));
  added_run_loop.Run();
}

TEST_F(GoogleOneOfferIphTabHelperTest, NotificationDismiss) {
  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents());

  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  NavigateAndCommit(GURL(kGoogleDriveUrl));
  added_run_loop.Run();

  std::unique_ptr<MockNewWindowDelegate> new_window_delegate =
      std::make_unique<MockNewWindowDelegate>();
  EXPECT_CALL(
      *new_window_delegate,
      OpenUrl(testing::Eq(GURL(kGoogleOneOfferUrl)), testing::_, testing::_))
      .Times(0);
  ash::TestNewWindowDelegateProvider delegate_provider(
      std::move(new_window_delegate));

  raw_ptr<feature_engagement::test::MockTracker> mock_tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile()));
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
}

TEST_F(GoogleOneOfferIphTabHelperTest, UIStringParams) {
  constexpr char kNotificationDisplaySource[] = "NotificationDisplaySource";
  constexpr char kNotificationTitle[] = "NotificationTitle";
  constexpr char kNotificationMessage[] = "NotificationMessage";
  constexpr char kGetPerkButtonTitle[] = "GetPerkButtonTitle";

  base::FieldTrialParams params;
  params[kNotificationDisplaySourceParamName] = kNotificationDisplaySource;
  params[kNotificationTitleParamName] = kNotificationTitle;
  params[kNotificationMessageParamName] = kNotificationMessage;
  params[kGetPerkButtonTitleParamName] = kGetPerkButtonTitle;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHGoogleOneOfferNotificationFeature, params);

  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents());

  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  NavigateAndCommit(GURL(kGoogleDriveUrl));
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

}  // namespace
