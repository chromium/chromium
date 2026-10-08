// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_customization_ephemeral_module.h"

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

class NTPCustomizationEphemeralModuleTest : public testing::Test {
 public:
  NTPCustomizationEphemeralModuleTest() = default;
  ~NTPCustomizationEphemeralModuleTest() override = default;

  void SetUp() override {
    Test::SetUp();

    NTPCustomizationEphemeralModule::RegisterProfilePrefs(
        pref_service_.registry());

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
TEST_F(NTPCustomizationEphemeralModuleTest,
       ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(NTPCustomizationEphemeralModule::IsModuleLabel(
      kNTPCustomizationEphemeralModule));
}

// Test that invalid module labels are correctly identified.
TEST_F(NTPCustomizationEphemeralModuleTest,
       InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(
      NTPCustomizationEphemeralModule::IsModuleLabel("some_other_label"));
}

// Test that the `OutputLabels()` method returns the expected labels.
TEST_F(NTPCustomizationEphemeralModuleTest, OutputLabelsReturnsExpectedLabels) {
  auto ephemeral_module =
      std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  std::vector<std::string> labels = ephemeral_module->OutputLabels();
  ASSERT_EQ(labels.size(), 1u);
  ASSERT_EQ(labels.front(), kNTPCustomizationEphemeralModule);
}

// Test that the `GetInputs()` method returns the expected inputs.
TEST_F(NTPCustomizationEphemeralModuleTest, GetInputsReturnsExpectedInputs) {
  auto ephemeral_module =
      std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module->GetInputs();
  EXPECT_EQ(inputs.size(), 2u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kNeverCustomizeNTP),
            inputs.end());
}

// Test that `ComputeCardResult()` does not show the module when no signals are
// present.
TEST_F(NTPCustomizationEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenNoSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(ephemeral_module.get(),
                                                {
                                                    /* kIsNewUser */ 0,
                                                    /* kNeverCustomizeNTP */ 0,
                                                });
  CardSelectionSignals selection_signals(&signals,
                                         kNTPCustomizationEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Test that `ComputeCardResult()` shows the NTP Customization module when the
// corresponding signals are present.
TEST_F(NTPCustomizationEphemeralModuleTest,
       ComputeCardResultShowsModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(ephemeral_module.get(),
                                                {
                                                    /* kIsNewUser */ 0,
                                                    /* kNeverCustomizeNTP */ 1,
                                                });
  CardSelectionSignals selection_signals(&signals,
                                         kNTPCustomizationEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kNTPCustomizationEphemeralModule, result.result_label);
}

// Test that `ComputeCardResult()` does not show the module when the
// disqualifying signal `kIsNewUser` is present, even if required signals are
// present.
TEST_F(NTPCustomizationEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module =
      std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kIsNewUser */ 1,  // Disqualifying signal
                                  /* kNeverCustomizeNTP */ 1,
                              });
  CardSelectionSignals selection_signals(&signals,
                                         kNTPCustomizationEphemeralModule);
  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Test that `IsEnabled()` returns true when under the impression limit and
// false otherwise, and integrates with the `OnShow` hook.
TEST_F(NTPCustomizationEphemeralModuleTest,
       IsEnabledReturnsFalseWhenImpressionLimitReached) {
  auto card = std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  EXPECT_TRUE(NTPCustomizationEphemeralModule::IsEnabled(&pref_service_));
  // 1 impression.
  card->OnShow(&pref_service_, nullptr);
  EXPECT_TRUE(NTPCustomizationEphemeralModule::IsEnabled(&pref_service_));
  // 2 impressions.
  card->OnShow(&pref_service_, nullptr);
  EXPECT_TRUE(NTPCustomizationEphemeralModule::IsEnabled(&pref_service_));
  // 3 impressions (limit reached).
  card->OnShow(&pref_service_, nullptr);
  EXPECT_FALSE(NTPCustomizationEphemeralModule::IsEnabled(&pref_service_));
}

// Test that interacting with the module prevents it from being shown again.
TEST_F(NTPCustomizationEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleAfterInteraction) {
  auto card = std::make_unique<NTPCustomizationEphemeralModule>(&pref_service_);
  AllCardSignals signals =
      CreateAllCardSignals(card.get(), {
                                           /* kIsNewUser */ 0,
                                           /* kNeverCustomizeNTP */ 1,
                                       });
  CardSelectionSignals selection_signals(&signals,
                                         kNTPCustomizationEphemeralModule);
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
