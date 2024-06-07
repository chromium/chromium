// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

class MockPage : public whats_new::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<whats_new::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<whats_new::mojom::Page> receiver_{this};
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  return profile;
}

}  // namespace

class WhatsNewHandlerTest : public testing::Test {
 public:
  WhatsNewHandlerTest()
      : profile_(MakeTestingProfile()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  ~WhatsNewHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<WhatsNewHandler>(
        mojo::PendingReceiver<whats_new::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(), web_contents_,
        base::Time::Now());
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  std::unique_ptr<WhatsNewHandler> handler_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

 private:
  raw_ptr<MockHatsService> mock_hats_service_;
};

TEST_F(WhatsNewHandlerTest, GetServerUrl) {
  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;

  const GURL expected_url = GURL(base::StringPrintf(
      "https://www.google.com/chrome/whats-new/?version=%d&internal=true",
      CHROME_VERSION_MAJOR));

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](GURL actual_url) { EXPECT_EQ(actual_url, expected_url); }));

  handler_->GetServerUrl(callback.Get());
  mock_page_.FlushForTesting();
}

TEST_F(WhatsNewHandlerTest, SurveyIsTriggered) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          features::kHappinessTrackingSurveysForDesktopWhatsNew,
          {{"whats-new-time", "20"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(1);

  handler_->GetServerUrl(callback.Get());
  mock_page_.FlushForTesting();
}

TEST_F(WhatsNewHandlerTest, HistogramsAreEmitted) {
  handler_->RecordTimeToLoadContent(101);
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeToLoadContent",
                                     1);

  handler_->RecordVersionPageLoaded(false);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("UserEducation.WhatsNew.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ShownByManualNavigation"));

  handler_->RecordModuleImpression(std::string("MyFeature"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown.MyFeature"));

  handler_->RecordExploreMoreToggled(false);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ExploreMoreExpanded", 1);

  handler_->RecordScrollDepth(whats_new::mojom::ScrollDepth::k25);
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.ScrollDepth", 1);

  handler_->RecordTimeOnPage(base::TimeDelta());
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeOnPage", 1);

  handler_->RecordModuleLinkClicked("AnotherFeature");
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked.AnotherFeature"));
}
