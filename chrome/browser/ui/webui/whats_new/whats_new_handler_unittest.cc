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
#include "components/user_education/webui/whats_new_registry.h"
#include "components/user_education/webui/whats_new_storage_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using whats_new::WhatsNewRegistry;

namespace {

// Modules
BASE_FEATURE(kTestEdition, "TestEdition", base::FEATURE_DISABLED_BY_DEFAULT);

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

class MockWhatsNewStorageService : public whats_new::WhatsNewStorageService {
 public:
  MockWhatsNewStorageService() = default;
  MOCK_METHOD(const base::Value::List&, ReadModuleData, (), (const override));
  MOCK_METHOD(const base::Value::Dict&, ReadEditionData, (), (const, override));
  MOCK_METHOD(int,
              GetModuleQueuePosition,
              (const std::string_view),
              (const, override));
  MOCK_METHOD(std::optional<int>,
              GetUsedVersion,
              (std::string_view edition_name),
              (const override));
  MOCK_METHOD(std::optional<std::string_view>,
              FindEditionForCurrentVersion,
              (),
              (const, override));
  MOCK_METHOD(bool, IsUsedEdition, (const std::string_view), (const, override));
  MOCK_METHOD(void, SetModuleEnabled, (const std::string_view), (override));
  MOCK_METHOD(void, ClearModule, (const std::string_view), (override));
  MOCK_METHOD(void, SetEditionUsed, (const std::string_view), (override));
  MOCK_METHOD(void, ClearEdition, (const std::string_view), (override));
  MOCK_METHOD(void, Reset, (), (override));
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

    // Setup mock storage service for tests that use the registry.
    auto mock_storage_service =
        std::make_unique<testing::NiceMock<MockWhatsNewStorageService>>();
    mock_storage_service_ = mock_storage_service.get();
    EXPECT_CALL(*mock_storage_service, ReadModuleData)
        .WillRepeatedly(testing::ReturnRef(mock_module_data_));

    whats_new_registry_ =
        std::make_unique<WhatsNewRegistry>(std::move(mock_storage_service));

    handler_ = std::make_unique<WhatsNewHandler>(
        mojo::PendingReceiver<whats_new::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(), web_contents_,
        base::Time::Now(), whats_new_registry_.get());
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  void TearDown() override {
    mock_storage_service_ = nullptr;
    testing::Test::TearDown();
  }

 protected:
  MockHatsService* mock_hats_service() { return mock_hats_service_; }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList feature_list_;
  base::Value::List mock_module_data_;
  raw_ptr<MockWhatsNewStorageService> mock_storage_service_;

  // NOTE: The initialization order of these members matters.
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockHatsService> mock_hats_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<WhatsNewRegistry> whats_new_registry_;
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

  handler_->RecordBrowserCommandExecuted();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.BrowserCommandExecuted"));

  handler_->RecordModuleVideoStarted(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.VideoStarted.AnotherFeature", 1);

  handler_->RecordModuleVideoEnded(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.VideoEnded.AnotherFeature", 1);

  handler_->RecordModulePlayClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.PlayClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.PlayClicked.AnotherFeature", 1);

  handler_->RecordModulePauseClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.PauseClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.PauseClicked.AnotherFeature", 1);

  handler_->RecordModuleRestartClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.RestartClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.RestartClicked.AnotherFeature", 1);
}

TEST_F(WhatsNewHandlerTest, SurveyIsTriggered) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          features::kHappinessTrackingSurveysForDesktopWhatsNew,
          {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerWhatsNew, _,
                                                _, _, _, _, _, _, _, _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}

TEST_F(WhatsNewHandlerTest, SurveyIsTriggeredWithOverride) {
  const std::string survey_override_id = "my-survey-id";
  whats_new_registry_->RegisterEdition(
      whats_new::WhatsNewEdition(kTestEdition, ""));

  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{user_education::features::kWhatsNewVersion2, {{}}},
       {kTestEdition, {{whats_new::kSurveyParam, survey_override_id}}},
       base::test::FeatureRefAndParams(
           features::kHappinessTrackingSurveysForDesktopWhatsNew,
           {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerWhatsNew, _, _, _, _, _, _, _,
                  std::optional<std::string>(survey_override_id), _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}

TEST_F(WhatsNewHandlerTest, SurveyIsNotTriggeredForPreviouslyUsedEdition) {
  const std::string survey_override_id = "my-survey-id";
  whats_new_registry_->RegisterEdition(
      whats_new::WhatsNewEdition(kTestEdition, ""));

  // Mark the registered edition as previously used.
  EXPECT_CALL(*mock_storage_service_, IsUsedEdition)
      .WillRepeatedly(testing::Return(true));

  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{user_education::features::kWhatsNewVersion2, {{}}},
       {kTestEdition, {{whats_new::kSurveyParam, survey_override_id}}},
       base::test::FeatureRefAndParams(
           features::kHappinessTrackingSurveysForDesktopWhatsNew,
           {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerWhatsNew, _,
                                                _, _, _, _, _, _, _, _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
  mock_page_.FlushForTesting();
}
