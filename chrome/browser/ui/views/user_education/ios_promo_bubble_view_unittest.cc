// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/ios_promo_bubble_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/desktop_to_mobile_promos/desktop_to_mobile_promos_metrics.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/features.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using desktop_to_mobile_promos::BubbleType;
using desktop_to_mobile_promos::PromoType;

namespace {

// Mocking the IOSPromoTriggerService to simulate a synced iOS device.
class MockIOSPromoTriggerService : public IOSPromoTriggerService {
 public:
  explicit MockIOSPromoTriggerService(Profile* profile)
      : IOSPromoTriggerService(profile) {
    // Add a fake iOS device.
    fake_device_info_tracker_.Add(&fake_device_info_);
  }

  const syncer::DeviceInfo* GetIOSDeviceToRemind() override {
    return &fake_device_info_;
  }

  MOCK_METHOD(void,
              SetReminderForIOSDevice,
              (PromoType promo_type, const std::string& device_guid),
              (override));

  const syncer::DeviceInfo* GetFakeDeviceInfo() { return &fake_device_info_; }

 private:
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  syncer::DeviceInfo fake_device_info_{
      "guid",
      "iPhone",
      "Chrome 100",
      "User Agent",
      sync_pb::SyncEnums::TYPE_PHONE,
      syncer::DeviceInfo::OsType::kIOS,
      syncer::DeviceInfo::FormFactor::kPhone,
      "device_id",
      "manufacturer",
      "model",
      "full_hardware_class",
      base::Time::Now(),
      base::TimeDelta(),
      /*send_tab_to_self_receiving_enabled=*/true,
      sync_pb::SyncEnums::SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/"token",
      /*interested_data_types=*/syncer::DataTypeSet::All(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt};
};

std::unique_ptr<KeyedService> CreateMockIOSPromoTriggerService(
    content::BrowserContext* context) {
  return std::make_unique<MockIOSPromoTriggerService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

// Test fixture for IOSPromoBubbleView.
// Uses ChromeViewsTestBase to provide a TestingProfile and support for
// BrowserContextKeyedServiceFactory (needed for IOSPromoTriggerService).
class IOSPromoBubbleViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kMobilePromoOnDesktop, {{kMobilePromoOnDesktopPromoTypeParam, "1"}}},
         {sync_preferences::features::kEnableCrossDevicePrefTracker, {}}},
        {});
    profile_ = std::make_unique<TestingProfile>();

    // Register the mock service.
    IOSPromoTriggerServiceFactory::GetInstance()->SetTestingFactory(
        GetProfile(), base::BindRepeating(&CreateMockIOSPromoTriggerService));

    // Create a dummy widget to serve as the anchor.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();
  }

  void TearDown() override {
    bubble_view_ = nullptr;
    if (bubble_widget_) {
      bubble_widget_->CloseNow();
      bubble_widget_ = nullptr;
    }
    anchor_view_ = nullptr;
    anchor_widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  Profile* GetProfile() { return profile_.get(); }

  // Getter for the histograms tester.
  base::HistogramTester* histograms() { return &histogram_; }

 protected:
  void CreateAndShowBubble(PromoType promo_type = PromoType::kLens,
                           BubbleType bubble_type = BubbleType::kQRCode) {
    auto bubble = std::make_unique<IOSPromoBubbleView>(
        GetProfile(), promo_type, bubble_type, anchor_view_,
        views::BubbleBorder::TOP_RIGHT);
    bubble_view_ = bubble.get();

    user_action_subscription_ =
        bubble_view_->AddUserActionCallback(user_action_callback_.Get());

    views::Widget* widget =
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    bubble_widget_ = widget->GetWeakPtr();
    widget->Show();
  }

  base::HistogramTester histogram_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<views::View> anchor_view_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<IOSPromoBubbleView> bubble_view_ = nullptr;
  base::WeakPtr<views::Widget> bubble_widget_ = nullptr;
  base::MockCallback<IOSPromoBubbleView::UserActionCallback>
      user_action_callback_;
  base::CallbackListSubscription user_action_subscription_;
};

// Tests that closing the bubble (e.g. via dismissal) calls NotifyUserAction
// with kDismiss.
TEST_F(IOSPromoBubbleViewTest, OnDismissalCallsNotifyUserAction_QRCode) {
  CreateAndShowBubble(PromoType::kLens, BubbleType::kQRCode);

  base::RunLoop run_loop;
  EXPECT_CALL(user_action_callback_,
              Run(IOSPromoBubbleView::UserAction::kDismiss))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  bubble_view_ = nullptr;
  bubble_widget_->Close();
  run_loop.Run();

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.QRCode.Action",
      desktop_to_mobile_promos::DesktopPromoActionType::kDismiss, 1);
}

// Tests that clicking the Cancel button calls NotifyUserAction with kCancel.
TEST_F(IOSPromoBubbleViewTest, CancelCallsNotifyUserAction_QRCode) {
  CreateAndShowBubble(PromoType::kLens, BubbleType::kQRCode);

  EXPECT_CALL(user_action_callback_,
              Run(IOSPromoBubbleView::UserAction::kCancel));

  bubble_view_->Cancel();

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.QRCode.Action",
      desktop_to_mobile_promos::DesktopPromoActionType::kCancel, 1);

  bubble_view_ = nullptr;
  bubble_widget_->CloseNow();
}

// Tests the multi-stage behavior of the Reminder promo:
// 1. First "Accept" ("Remind me") sends the reminder and transitions to
// confirmation state.
// 2. Second "Accept" ("Got it") closes the bubble.
TEST_F(IOSPromoBubbleViewTest, AcceptShowsConfirmation_Reminder) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kMobilePromoOnDesktop));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      sync_preferences::features::kEnableCrossDevicePrefTracker));

  auto* service = static_cast<MockIOSPromoTriggerService*>(
      IOSPromoTriggerServiceFactory::GetForProfile(GetProfile()));
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->GetIOSDeviceToRemind());

  // Expect the trigger service to be called with the correct promo type and
  // device GUID.
  EXPECT_CALL(*service, SetReminderForIOSDevice(PromoType::kLens, "guid"));

  CreateAndShowBubble(PromoType::kLens, BubbleType::kReminder);

  histograms()->ExpectBucketCount(
      "UserEducation.DesktopToIOSPromo.Lens.BubbleView.Created",
      desktop_to_mobile_promos::DesktopPromoBubbleType::kReminder, 1);

  // The bubble should be dismissed after the second accept.
  base::RunLoop run_loop;
  EXPECT_CALL(user_action_callback_,
              Run(IOSPromoBubbleView::UserAction::kDismiss))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  // Click Accept (First time - "Remind me"). Expect it to return false (keep
  // bubble open).
  EXPECT_FALSE(bubble_view_->Accept());
  EXPECT_FALSE(bubble_widget_->IsClosed());

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.Reminder.Action",
      desktop_to_mobile_promos::DesktopPromoActionType::kAccept, 1);

  histograms()->ExpectBucketCount(
      "UserEducation.DesktopToIOSPromo.Lens.BubbleView.Created",
      desktop_to_mobile_promos::DesktopPromoBubbleType::kReminderConfirmation,
      1);

  // Click Accept (Second time - "Got it"). Now it should be in
  // kReminderConfirmation state. Expect it to return true (close bubble).
  EXPECT_TRUE(bubble_view_->Accept());

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.ReminderConfirmation.Action",
      desktop_to_mobile_promos::DesktopPromoActionType::kAccept, 1);

  // Close triggers dismissal.
  bubble_view_ = nullptr;
  bubble_widget_->Close();
  run_loop.Run();
}

// Tests that clicking the Accept button for the QR Code promo closes the bubble
// and attempts to open the correct URL.
TEST_F(IOSPromoBubbleViewTest, AcceptOpensUrl_QRCode) {
  CreateAndShowBubble(PromoType::kLens, BubbleType::kQRCode);

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.BubbleView.Created",
      desktop_to_mobile_promos::DesktopPromoBubbleType::kQRCode, 1);

  base::MockCallback<IOSPromoBubbleView::OpenUrlCallback> mock_callback;
  bubble_view_->SetOpenUrlCallbackForTesting(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(testing::_))
      .WillOnce([](const content::OpenURLParams& params) {
        EXPECT_EQ(params.url.host(), "www.google.com");
        EXPECT_EQ(params.url.path(), "/chrome/go-mobile/");
        EXPECT_TRUE(base::Contains(params.url.query(), "ios-campaign"));
        EXPECT_TRUE(base::Contains(params.url.query(), "android-campaign"));
        EXPECT_EQ(params.disposition,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB);
      });

  // Click Accept. Expect it to return true (close bubble).
  EXPECT_TRUE(bubble_view_->Accept());

  histograms()->ExpectUniqueSample(
      "UserEducation.DesktopToIOSPromo.Lens.QRCode.Action",
      desktop_to_mobile_promos::DesktopPromoActionType::kAccept, 1);

  // Expect dismissal on close.
  base::RunLoop run_loop;
  EXPECT_CALL(user_action_callback_,
              Run(IOSPromoBubbleView::UserAction::kDismiss))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  bubble_view_ = nullptr;
  bubble_widget_->Close();
  run_loop.Run();
}
