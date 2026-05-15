// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_hats_survey_controller.h"

#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;

namespace {
constexpr base::TimeDelta kMinSessionDuration = base::Seconds(10);
constexpr base::TimeDelta kUsageHistoryWindow = base::Days(14);
constexpr size_t kMinUsages = 3;
}  // namespace

class ReadAnythingHatsSurveyControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    testing_profile_ = std::make_unique<TestingProfile>();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    web_contents_ =
        web_contents_factory_->CreateWebContents(testing_profile_.get());

    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents_));
    ON_CALL(*mock_tab_, IsInNormalWindow())
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            testing_profile_.get(),
            base::BindRepeating(&BuildMockHatsService)));

    side_panel_registry_ = std::make_unique<SidePanelRegistry>(mock_tab_.get());

    scoped_feature_list_.InitWithFeatures(
        {features::kImmersiveReadAnything, features::kHatsReadingModeSurvey},
        {});

    controller_ = std::make_unique<ReadAnythingController>(
        mock_tab_.get(), side_panel_registry_.get());
    survey_controller_ = std::make_unique<ReadAnythingHatsSurveyController>(
        controller_.get(), mock_tab_.get());
  }

  void TearDown() override {
    if (controller_ && survey_controller_) {
      controller_->RemoveObserver(survey_controller_.get());
    }
    survey_controller_.reset();
    web_contents_ = nullptr;
    mock_hats_service_ = nullptr;
    controller_.reset();
    side_panel_registry_.reset();
    mock_tab_.reset();
    web_contents_factory_.reset();
    testing_profile_.reset();
    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<ReadAnythingController> controller_;
  std::unique_ptr<ReadAnythingHatsSurveyController> survey_controller_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

TEST_F(ReadAnythingHatsSurveyControllerUnitTest, UsageRecordedOnSessionEnd) {
  PrefService* prefs = testing_profile_->GetPrefs();

  survey_controller_->Activate(false, std::nullopt,
                               kMinSessionDuration + base::Seconds(1));

  const base::ListValue& usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);
  EXPECT_EQ(usages.size(), 1u);
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest, UsagePrunedOlderThan14Days) {
  PrefService* prefs = testing_profile_->GetPrefs();
  base::ListValue usages;
  base::Time now = base::Time::Now();
  usages.Append(base::TimeToValue(now - (kUsageHistoryWindow + base::Days(1))));
  prefs->SetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes,
                 std::move(usages));

  survey_controller_->Activate(false, std::nullopt,
                               kMinSessionDuration + base::Seconds(1));

  const base::ListValue& final_usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);
  EXPECT_EQ(final_usages.size(), 1u);
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest, UsageListCappedAtThree) {
  PrefService* prefs = testing_profile_->GetPrefs();
  base::ListValue usages;
  base::Time now = base::Time::Now();
  usages.Append(base::TimeToValue(now - base::Days(1)));
  usages.Append(base::TimeToValue(now - base::Days(2)));
  usages.Append(base::TimeToValue(now - base::Days(3)));
  prefs->SetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes,
                 std::move(usages));

  survey_controller_->Activate(false, std::nullopt,
                               kMinSessionDuration + base::Seconds(1));

  const base::ListValue& final_usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);
  EXPECT_EQ(final_usages.size(), kMinUsages);
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest,
       SurveyNotShownIfSessionTooShort) {
  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(0);

  survey_controller_->Activate(false, std::nullopt, base::Seconds(5));
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest,
       SurveyNotShownIfSessionDurationIsNull) {
  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(0);

  // Passing std::nullopt simulates a presentation transition or early close.
  survey_controller_->Activate(false, std::nullopt, std::nullopt);

  // Verify usage wasn't recorded either.
  PrefService* prefs = testing_profile_->GetPrefs();
  const base::ListValue& usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);
  EXPECT_EQ(usages.size(), 0u);
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest,
       SurveyNotShownIfUsagesLessThanThree) {
  PrefService* prefs = testing_profile_->GetPrefs();
  base::ListValue usages;
  base::Time now = base::Time::Now();
  usages.Append(base::TimeToValue(now - base::Days(1)));
  prefs->SetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes,
                 std::move(usages));

  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(0);

  survey_controller_->Activate(false, std::nullopt,
                               kMinSessionDuration + base::Seconds(1));
}

TEST_F(ReadAnythingHatsSurveyControllerUnitTest, SurveyShownIfConditionsMet) {
  PrefService* prefs = testing_profile_->GetPrefs();
  base::ListValue usages;
  base::Time now = base::Time::Now();
  usages.Append(base::TimeToValue(now - base::Days(1)));
  usages.Append(base::TimeToValue(now - base::Days(2)));
  prefs->SetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes,
                 std::move(usages));

  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerReadingModeExit, _, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(testing::Return(true));

  survey_controller_->Activate(false, std::nullopt,
                               kMinSessionDuration + base::Seconds(1));
}
