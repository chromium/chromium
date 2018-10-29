// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_bubble_controller.h"

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#include "chrome/browser/ui/desktop_ios_promotion/sms_service.h"
#include "chrome/browser/ui/desktop_ios_promotion/sms_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class FakeSMSService : public SMSService {
 public:
  FakeSMSService() : SMSService(nullptr, nullptr) {}
  ~FakeSMSService() override {}
  MOCK_METHOD1(QueryPhoneNumber, void(const PhoneNumberCallback&));
  MOCK_METHOD2(SendSMS,
               void(const std::string&,
                    const SMSService::PhoneNumberCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSMSService);
};

std::unique_ptr<KeyedService> BuildFakeSMSService(
    content::BrowserContext* profile) {
  return std::make_unique<FakeSMSService>();
}

}  // namespace

class DesktopIOSPromotionBubbleControllerTest : public testing::Test {
 public:
  DesktopIOSPromotionBubbleControllerTest() {}
  ~DesktopIOSPromotionBubbleControllerTest() override {}

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
            new TestingPrefStore(), new TestingPrefStore(),
            new TestingPrefStore(), new TestingPrefStore(),
            new user_prefs::PrefRegistrySyncable(), new PrefNotifierImpl());
    RegisterUserProfilePrefs(pref_service_->registry());
    TestingProfile::Builder builder;
    builder.SetPrefService(std::move(pref_service_));
    builder.AddTestingFactory(SMSServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeSMSService));
    profile_ = builder.Build();
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
    desktop_ios_promotion::RegisterLocalPrefs(local_state_->registry());
    sms_service_ = static_cast<FakeSMSService*>(
        SMSServiceFactory::GetForProfile(profile_.get()));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    controller_.reset();
    profile_.reset();
  }

  void InitController(desktop_ios_promotion::PromotionEntryPoint entry_point) {
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(sms_service_));
    EXPECT_CALL(*sms_service_, QueryPhoneNumber(_));
    controller_ = std::make_unique<DesktopIOSPromotionBubbleController>(
        profile_.get(), nullptr, entry_point);
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 protected:
  FakeSMSService* sms_service_ = nullptr;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<DesktopIOSPromotionBubbleController> controller_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopIOSPromotionBubbleControllerTest);
};

TEST_F(DesktopIOSPromotionBubbleControllerTest, ClickSendSMS) {
  InitController(
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE);
  EXPECT_CALL(*sms_service_, SendSMS(_, _));
  controller_->OnSendSMSClicked();
  EXPECT_EQ(desktop_ios_promotion::PromotionDismissalReason::SEND_SMS,
            controller_->dismissal_reason());
  EXPECT_EQ(1, prefs()->GetInteger(prefs::kIOSPromotionSMSEntryPoint));
}

TEST_F(DesktopIOSPromotionBubbleControllerTest, PromotionShown) {
  const char kHistogram[] = "DesktopIOSPromotion.ImpressionFromEntryPoint";
  base::HistogramTester histograms;
  desktop_ios_promotion::PromotionEntryPoint entry_point =
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE;
  InitController(entry_point);

  EXPECT_EQ(0, local_state_->GetInteger(
                   prefs::kNumberSavePasswordsBubbleIOSPromoShown));
  EXPECT_EQ(0, prefs()->GetInteger(prefs::kIOSPromotionShownEntryPoints));
  controller_->OnPromotionShown();
  // Impressions increase.
  EXPECT_EQ(1, local_state_->GetInteger(
                   prefs::kNumberSavePasswordsBubbleIOSPromoShown));
  double lst_impr = prefs()->GetDouble(prefs::kIOSPromotionLastImpression);
  // last impression time updated correctly.
  EXPECT_LT(base::Time::Now() - base::Time::FromDoubleT(lst_impr),
            TestTimeouts::action_timeout());
  // We reset the SMS entry point as the user is is still eligible and haven't
  // seen the promotion for 7 days.
  EXPECT_EQ(0, prefs()->GetInteger(prefs::kIOSPromotionSMSEntryPoint));
  // Check if the impression is logged to histograms.
  histograms.ExpectUniqueSample(kHistogram, static_cast<int>(entry_point), 1);
  // Check if the bit for this entry point was set in profile prefs.
  EXPECT_EQ(1 << static_cast<int>(entry_point),
            prefs()->GetInteger(prefs::kIOSPromotionShownEntryPoints));
  int shown_promotions =
      prefs()->GetInteger(prefs::kIOSPromotionShownEntryPoints);

  controller_->OnPromotionShown();
  EXPECT_EQ(2, local_state_->GetInteger(
                   prefs::kNumberSavePasswordsBubbleIOSPromoShown));
  histograms.ExpectUniqueSample(kHistogram, static_cast<int>(entry_point), 2);

  // Check different entry point.
  entry_point = desktop_ios_promotion::PromotionEntryPoint::BOOKMARKS_BUBBLE;
  InitController(entry_point);
  controller_->OnPromotionShown();
  histograms.ExpectBucketCount(kHistogram, static_cast<int>(entry_point), 1);
  histograms.ExpectTotalCount(kHistogram, 3);

  // Check if the bit for this entry point was set in profile prefs.
  EXPECT_EQ(shown_promotions | (1 << static_cast<int>(entry_point)),
            prefs()->GetInteger(prefs::kIOSPromotionShownEntryPoints));
}

TEST_F(DesktopIOSPromotionBubbleControllerTest, ClickNoThanks) {
  InitController(
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE);
  controller_->OnNoThanksClicked();
  EXPECT_EQ(desktop_ios_promotion::PromotionDismissalReason::NO_THANKS,
            controller_->dismissal_reason());
  EXPECT_TRUE(
      local_state_->GetBoolean(prefs::kSavePasswordsBubbleIOSPromoDismissed));
}
