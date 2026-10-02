// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_theme_ephemeral_module.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class NTPThemeEphemeralModuleTest : public testing::Test {
 public:
  NTPThemeEphemeralModuleTest() = default;
  ~NTPThemeEphemeralModuleTest() override = default;

  void SetUp() override {
    Test::SetUp();

    NTPThemeEphemeralModule::RegisterProfilePrefs(pref_service_.registry());

    // Enable the feature flags for ephemeral modules and Magic Stack Tips V2.
    scoped_feature_list_.InitWithFeatures(
        {features::kSegmentationPlatformEphemeralCardRanker,
         features::kMagicStackTipsV2Ios},
        {});
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
};

// Test that valid module labels are correctly identified.
TEST_F(NTPThemeEphemeralModuleTest, ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(NTPThemeEphemeralModule::IsModuleLabel(kNTPThemeEphemeralModule));
}

// Test that invalid module labels are correctly identified.
TEST_F(NTPThemeEphemeralModuleTest, InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(NTPThemeEphemeralModule::IsModuleLabel("some_other_label"));
}

// Test that the `OutputLabels()` method returns the expected labels.
TEST_F(NTPThemeEphemeralModuleTest, OutputLabelsReturnsExpectedLabels) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  std::vector<std::string> labels = ephemeral_module->OutputLabels();
  ASSERT_EQ(labels.size(), 1u);
  ASSERT_EQ(labels.front(), kNTPThemeEphemeralModule);
}

// Test that the `GetInputs()` method returns the expected inputs.
TEST_F(NTPThemeEphemeralModuleTest, GetInputsReturnsExpectedInputs) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kLacksNTPBackground),
            inputs.end());
  EXPECT_NE(
      inputs.find(segmentation_platform::kNTPBackgroundNotSelectedRecently),
      inputs.end());
}

// Test that `ComputeCardResult()` does not show the module when no signals are
// present.
TEST_F(NTPThemeEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenNoSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kIsNewUser */ 0,
                                  /* kLacksNTPBackground */ 0,
                                  /* kNTPBackgroundNotSelectedRecently */ 0,
                              });
  CardSelectionSignals selection_signals(&signals, kNTPThemeEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Test that `ComputeCardResult()` does not show the module when the required
// signals are partially missing.
TEST_F(NTPThemeEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenSomeSignalsAreMissing) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kIsNewUser */ 0,
                                  /* kLacksNTPBackground */ 1,
                                  /* kNTPBackgroundNotSelectedRecently */ 0,
                              });
  CardSelectionSignals selection_signals(&signals, kNTPThemeEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Test that `ComputeCardResult()` shows the NTP Theme module when the
// corresponding signals are present.
TEST_F(NTPThemeEphemeralModuleTest,
       ComputeCardResultShowsModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kIsNewUser */ 0,
                                  /* kLacksNTPBackground */ 1,
                                  /* kNTPBackgroundNotSelectedRecently */ 1,
                              });
  CardSelectionSignals selection_signals(&signals, kNTPThemeEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kNTPThemeEphemeralModule, result.result_label);
}

// Test that `ComputeCardResult()` does not show the module when the
// disqualifying signal `kIsNewUser` is present, even if required signals are
// present.
TEST_F(NTPThemeEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module =
      std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kIsNewUser */ 1,  // Disqualifying signal
                                  /* kLacksNTPBackground */ 1,
                                  /* kNTPBackgroundNotSelectedRecently */ 1,
                              });
  CardSelectionSignals selection_signals(&signals, kNTPThemeEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Test that `IsEnabled()` returns true when under the impression limit and
// false otherwise, and integrates with the `OnShow` hook.
TEST_F(NTPThemeEphemeralModuleTest,
       IsEnabledReturnsFalseWhenImpressionLimitReached) {
  auto card = std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  EXPECT_TRUE(NTPThemeEphemeralModule::IsEnabled(&pref_service_));
  // 1 impression.
  card->OnShow(&pref_service_, nullptr);
  EXPECT_TRUE(NTPThemeEphemeralModule::IsEnabled(&pref_service_));
  // 2 impressions.
  card->OnShow(&pref_service_, nullptr);
  EXPECT_TRUE(NTPThemeEphemeralModule::IsEnabled(&pref_service_));
  // 3 impressions (limit reached).
  card->OnShow(&pref_service_, nullptr);
  EXPECT_FALSE(NTPThemeEphemeralModule::IsEnabled(&pref_service_));
}

// Test that interacting with the module prevents it from being shown again.
TEST_F(NTPThemeEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleAfterInteraction) {
  auto card = std::make_unique<NTPThemeEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      card.get(), {
                      /* kIsNewUser */ 0,
                      /* kLacksNTPBackground */ 1,
                      /* kNTPBackgroundNotSelectedRecently */ 1,
                  });
  CardSelectionSignals selection_signals(&signals, kNTPThemeEphemeralModule);
  // Initially, the card should be shown.
  CardSelectionInfo::ShowResult result =
      card->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  // Simulate a user interaction.
  card->OnInteract(&pref_service_, nullptr);
  // The card should no longer be shown.
  result = card->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

}  // namespace segmentation_platform::home_modules
