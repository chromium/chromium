// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

}  // namespace

class WhatsNewHandlerTest : public testing::Test {
 public:
  WhatsNewHandlerTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {
    feature_list_.InitWithFeatures(
        {}, {user_education::features::kWhatsNewVersion2});
  }
  ~WhatsNewHandlerTest() override = default;

  void SetUp() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey)
        .WillRepeatedly(testing::Return(true));

    handler_ = std::make_unique<WhatsNewHandler>(
        mojo::PendingReceiver<whats_new::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(), web_contents_,
        base::Time::Now());
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

 protected:
  MockHatsService* mock_hats_service() { return mock_hats_service_; }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList feature_list_;

  // NOTE: The initialization order of these members matters.
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockHatsService> mock_hats_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<WhatsNewHandler> handler_;
};

TEST_F(WhatsNewHandlerTest, GetServerUrl) {
  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;

  const GURL expected_url = GURL(base::StringPrintf(
      "https://www.google.com/chrome/whats-new/?version=%d&internal=true",
      CHROME_VERSION_MAJOR));

  EXPECT_CALL(callback, Run)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](GURL actual_url) { EXPECT_EQ(actual_url, expected_url); }));

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}

TEST_F(WhatsNewHandlerTest, HistogramsAreEmitted) {
  handler_->RecordTimeToLoadContent(base::Time::Now());
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeToLoadContent",
                                     1);

  handler_->RecordVersionPageLoaded(false);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("UserEducation.WhatsNew.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.VersionShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ShownByManualNavigation"));

  user_action_tester_.ResetCounts();
  handler_->RecordEditionPageLoaded("NewEdition", false);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("UserEducation.WhatsNew.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.EditionShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.EditionShown.NewEdition"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ShownByManualNavigation"));

  handler_->RecordModuleImpression(
      "MyFeature", whats_new::mojom::ModulePosition::kSpotlight1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown.MyFeature"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ModuleShown.MyFeature", 1);

  handler_->RecordExploreMoreToggled(false);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ExploreMoreExpanded", 1);

  handler_->RecordScrollDepth(whats_new::mojom::ScrollDepth::k25);
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.ScrollDepth", 1);

  handler_->RecordTimeOnPage(base::TimeDelta());
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeOnPage", 1);

  handler_->RecordModuleLinkClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked.AnotherFeature"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ModuleLinkClicked.AnotherFeature", 1);
}

class WhatsNewHandlerTestWithCountry
    : public WhatsNewHandlerTest,
      public testing::WithParamInterface<absl::string_view> {
 public:
  WhatsNewHandlerTestWithCountry() = default;
  ~WhatsNewHandlerTestWithCountry() override = default;

  bool IsActiveCountry(std::string_view country) {
    return std::find(active_countries_.begin(), active_countries_.end(),
                     country) != active_countries_.end();
  }

 private:
  std::vector<std::string_view> active_countries_ = {"us", "de", "jp"};
};

TEST_P(WhatsNewHandlerTestWithCountry, SurveyIsTriggeredInActiveCountries) {
  auto country = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          features::kHappinessTrackingSurveysForDesktopWhatsNew,
          {{"whats-new-time", "20s"}})},
      {});

  handler_->set_override_latest_country_for_testing(country);

  // Set activation threshold to trigger for
  handler_->set_override_threshold_for_testing_(0);
  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  if (IsActiveCountry(country)) {
    EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents)
        .Times(1);
  } else {
    // Any threshold value will fail when the country is not set or not
    // active.
    EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents)
        .Times(0);
  }

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}

TEST_P(WhatsNewHandlerTestWithCountry,
       AlternateSurveyIsTriggeredInActiveCountries) {
  // Avoid creating actual url with WhatsNewRegistry
  whats_new::DisableRemoteContentForTests();

  auto country = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
           user_education::features::kWhatsNewVersion2, {}),
       base::test::FeatureRefAndParams(
           features::kHappinessTrackingSurveysForDesktopWhatsNew,
           {{"whats-new-time", "20s"}})},
      {});

  handler_->set_override_latest_country_for_testing(country);

  // Set activation threshold to trigger for
  handler_->set_override_threshold_for_testing_(0);
  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  if (IsActiveCountry(country)) {
    EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents)
        .Times(1);
  } else {
    // Any threshold value will fail when the country is not set or not
    // active.
    EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents)
        .Times(0);
  }

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}

constexpr std::string_view test_countries[] = {"", "fr", "us", "de", "jp"};
INSTANTIATE_TEST_SUITE_P(All,
                         WhatsNewHandlerTestWithCountry,
                         testing::ValuesIn(test_countries),
                         [&](const testing::TestParamInfo<std::string_view>&
                                 country) -> std::string {
                           if (country.param == "") {
                             return "NoCountry";
                           }
                           return std::string(country.param);
                         });
