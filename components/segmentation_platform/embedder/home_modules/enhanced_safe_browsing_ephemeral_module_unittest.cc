// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/enhanced_safe_browsing_ephemeral_module.h"

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

class EnhancedSafeBrowsingEphemeralModuleTest : public testing::Test {
 public:
  EnhancedSafeBrowsingEphemeralModuleTest() = default;
  ~EnhancedSafeBrowsingEphemeralModuleTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
    // Enable the feature flag for ephemeral modules.
    scoped_feature_list_.InitAndEnableFeature(
        features::kSegmentationPlatformEphemeralCardRanker);
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
};

// Verifies that valid module labels are correctly identified.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(EnhancedSafeBrowsingEphemeralModule::IsModuleLabel(
      kEnhancedSafeBrowsingEphemeralModule));
}

// Verifies that invalid module labels are correctly identified.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(
      EnhancedSafeBrowsingEphemeralModule::IsModuleLabel("some_other_label"));
}

// Verifies that the `OutputLabels(…)` method returns the expected labels.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       OutputLabelsReturnsExpectedLabels) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);
  std::vector<std::string> labels = ephemeral_module->OutputLabels();
  ASSERT_EQ(labels.size(), 1u);
  ASSERT_EQ(labels.front(), kEnhancedSafeBrowsingEphemeralModule);
}

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       GetInputsReturnsExpectedInputs) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kLacksEnhancedSafeBrowsing),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::
                            kEnhancedSafeBrowsingAllowedByEnterprisePolicy),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
}

// Verifies that `ComputeCardResult(…)` does not show the module when no signals
// are present.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenNoSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kLacksEnhancedSafeBrowsing */ 0,
          /* kIsNewUser */ 0,
          /* kEnhancedSafeBrowsingAllowedByEnterprisePolicy */ 0,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kEnhancedSafeBrowsingEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` does not show the module when the
// corresponding signals are partially missing.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenSomeSignalsAreMissing) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kLacksEnhancedSafeBrowsing */ 0,
          /* kIsNewUser */ 0,
          /* kEnhancedSafeBrowsingAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kEnhancedSafeBrowsingEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` shows the Enhanced Safe Browsing module
// when the corresponding signals are present.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ComputeCardResultShowsModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kLacksEnhancedSafeBrowsing */ 1,
          /* kIsNewUser */ 0,
          /* kEnhancedSafeBrowsingAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kEnhancedSafeBrowsingEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kEnhancedSafeBrowsingEphemeralModule, result.result_label);
}

// Verifies that `ComputeCardResult(…)` does not show the module when the
// corresponding signals are present, but Enhanced Safe Browsing is not allowed
// by enterprise policy.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenEnterprisePolicyDisallows) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kLacksEnhancedSafeBrowsing */ 1,
          /* kIsNewUser */ 0,
          /* kEnhancedSafeBrowsingAllowedByEnterprisePolicy */ 0,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kEnhancedSafeBrowsingEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(...)` does not show the module when the
// disqualifying signal `kIsNewUser` is present, even if other required signals
// are present.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module =
      std::make_unique<EnhancedSafeBrowsingEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kLacksEnhancedSafeBrowsing */ 1,
          /* kIsNewUser */ 1,  // Disqualifying signal
          /* kEnhancedSafeBrowsingAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kEnhancedSafeBrowsingEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that `IsEnabled(…)` returns true when under the impression limit
// and false otherwise.
TEST_F(EnhancedSafeBrowsingEphemeralModuleTest, TestIsEnabled) {
  // Enable the feature flag for ephemeral modules.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformTipsEphemeralCard,
        {{features::kTipsEphemeralCardModuleMaxImpressionCount, "3"}}},
       {features::kSegmentationPlatformEphemeralCardRanker, {}}},
      {});

  EXPECT_TRUE(EnhancedSafeBrowsingEphemeralModule::IsEnabled(0));
  EXPECT_TRUE(EnhancedSafeBrowsingEphemeralModule::IsEnabled(1));
  EXPECT_TRUE(EnhancedSafeBrowsingEphemeralModule::IsEnabled(2));
  EXPECT_FALSE(EnhancedSafeBrowsingEphemeralModule::IsEnabled(3));
  EXPECT_FALSE(EnhancedSafeBrowsingEphemeralModule::IsEnabled(4));
}

}  // namespace segmentation_platform::home_modules
