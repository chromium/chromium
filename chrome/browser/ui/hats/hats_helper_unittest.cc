// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_helper.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

class HatsHelperTest : public testing::Test {
 public:
  HatsHelperTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_))
        .WillRepeatedly(testing::Return(true));

    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    hats_helper_ = std::make_unique<HatsHelper>(web_contents_.get());
  }

 protected:
  TestingProfile* profile() { return &profile_; }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  MockTrustSafetySentimentService* mock_sentiment_service() {
    return mock_sentiment_service_;
  }
  content::WebContentsTester* test_web_contents() {
    return content::WebContentsTester::For(web_contents_.get());
  }

  content::RenderFrameHost* main_rfh() {
    return web_contents_->GetPrimaryMainFrame();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<HatsHelper> hats_helper_;
  raw_ptr<MockHatsService> mock_hats_service_;
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;
};

TEST_F(HatsHelperTest, SentimentServiceInformed) {
  // Check that the helper correctly informs the sentiment service that the
  // user has visited the NTP.
  EXPECT_CALL(*mock_sentiment_service(), OpenedNewTabPage());
  test_web_contents()->NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Navigations to non-NTP pages should not inform the service.
  EXPECT_CALL(*mock_sentiment_service(), OpenedNewTabPage()).Times(0);
  test_web_contents()->NavigateAndCommit(GURL("https://unrelated.com"));
}
