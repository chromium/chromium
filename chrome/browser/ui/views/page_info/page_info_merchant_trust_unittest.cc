// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/page_info/merchant_trust_service_factory.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "components/page_info/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {

class MockMerchantTrustService : public page_info::MerchantTrustService {
 public:
  MockMerchantTrustService()
      : MerchantTrustService(nullptr, nullptr, false, nullptr) {}
  MOCK_METHOD(void,
              GetMerchantTrustInfo,
              (const GURL&, page_info::MerchantDataCallback),
              (const, override));
};

std::unique_ptr<KeyedService> BuildMockMerchantTrustService(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockMerchantTrustService>>();
}

}  // namespace

class PageInfoMerchantTrustControllerUnitTest : public ChromeViewsTestBase {
 protected:
  PageInfoMerchantTrustControllerUnitTest() : ChromeViewsTestBase() {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    mock_merchant_trust_service_ = static_cast<MockMerchantTrustService*>(
        MerchantTrustServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&BuildMockMerchantTrustService)));

    content_view_ = std::make_unique<PageInfoMerchantTrustContentView>();
    controller_ = std::make_unique<PageInfoMerchantTrustController>(
        content_view_.get(), web_contents());
  }

  void TearDown() override {
    mock_merchant_trust_service_ = nullptr;
    controller_.reset();
    content_view_.reset();
    ChromeViewsTestBase::TearDown();
  }

  PageInfoMerchantTrustController* controller() { return controller_.get(); }

  content::WebContents* web_contents() { return test_web_contents_.get(); }

  PrefService* prefs() { return profile_.GetPrefs(); }

 private:
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;

  std::unique_ptr<PageInfoMerchantTrustContentView> content_view_;
  std::unique_ptr<PageInfoMerchantTrustController> controller_;
  raw_ptr<MockMerchantTrustService> mock_merchant_trust_service_;
};

TEST_F(PageInfoMerchantTrustControllerUnitTest, InteractionRecorded) {
  EXPECT_EQ(prefs()->GetTime(prefs::kMerchantTrustUiLastInteractionTime),
            base::Time());
  task_environment()->AdvanceClock(
      page_info::kMerchantTrustRequiredInteractionDuration.Get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(prefs()->GetTime(prefs::kMerchantTrustUiLastInteractionTime),
            base::Time::Now());
}

TEST_F(PageInfoMerchantTrustControllerUnitTest, InteractionNotRecorded) {
  EXPECT_EQ(prefs()->GetTime(prefs::kMerchantTrustUiLastInteractionTime),
            base::Time());
  task_environment()->AdvanceClock(
      page_info::kMerchantTrustRequiredInteractionDuration.Get() -
      base::Seconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(prefs()->GetTime(prefs::kMerchantTrustUiLastInteractionTime),
            base::Time());
}
