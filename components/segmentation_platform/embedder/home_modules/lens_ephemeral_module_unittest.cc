// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/lens_ephemeral_module.h"

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

class LensEphemeralModuleTest : public testing::Test {
 public:
  LensEphemeralModuleTest() = default;
  ~LensEphemeralModuleTest() override = default;

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
TEST_F(LensEphemeralModuleTest, ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(
      LensEphemeralModule::IsModuleLabel(kLensEphemeralModuleSearchVariation));
  EXPECT_TRUE(
      LensEphemeralModule::IsModuleLabel(kLensEphemeralModuleShopVariation));
  EXPECT_TRUE(LensEphemeralModule::IsModuleLabel(
      kLensEphemeralModuleTranslateVariation));
}

// Verifies that invalid module labels are correctly identified.
TEST_F(LensEphemeralModuleTest, InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(LensEphemeralModule::IsModuleLabel("some_other_label"));
}

// Verifies that the `OutputLabels(…)` method returns the expected labels.
TEST_F(LensEphemeralModuleTest, OutputLabelsReturnsExpectedLabels) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);
  std::vector<std::string> labels = ephemeral_module->OutputLabels();
  EXPECT_EQ(labels.size(), kLensEphemeralModuleVariationLabels.size());
  for (const std::string& label : labels) {
    EXPECT_TRUE(kLensEphemeralModuleVariationLabels.contains(label));
  }
}

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(LensEphemeralModuleTest, GetInputsReturnsExpectedInputs) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module->GetInputs();
  EXPECT_EQ(inputs.size(), 6u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kLensNotUsedRecently),
            inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::tips_manager::signals::
                            kOpenedWebsiteInAnotherLanguage),
            inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::tips_manager::signals::kUsedGoogleTranslation),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kLensAllowedByEnterprisePolicy),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
}

// Verifies that `ComputeCardResult(…)` does not show a module when no signals
// are present.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenNoSignalsArePresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 0,
                                  /* kOpenedWebsiteInAnotherLanguage */ 0,
                                  /* kUsedGoogleTranslation */ 0,
                                  /* kIsNewUser */ 0,
                                  /* kLensNotUsedRecently */ 0,
                                  /* kLensAllowedByEnterprisePolicy */ 0,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Shop module when the
// corresponding signals are present.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultShowsLensShopModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals new_signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 1,
                                  /* kOpenedWebsiteInAnotherLanguage */ 0,
                                  /* kUsedGoogleTranslation */ 0,
                                  /* kIsNewUser */ 0,
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&new_signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kLensEphemeralModuleShopVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Translate module when
// the corresponding signals are present.
TEST_F(
    LensEphemeralModuleTest,
    ComputeCardResultShowsLensTranslateModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 0,
                                  /* kOpenedWebsiteInAnotherLanguage */ 1,
                                  /* kUsedGoogleTranslation */ 1,
                                  /* kIsNewUser */ 0,
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kLensEphemeralModuleTranslateVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Search module when
// the corresponding signals are present.
TEST_F(
    LensEphemeralModuleTest,
    ComputeCardResultShowsLensSearchModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 0,
                                  /* kOpenedWebsiteInAnotherLanguage */ 0,
                                  /* kUsedGoogleTranslation */ 0,
                                  /* kIsNewUser */ 0,
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kLensEphemeralModuleSearchVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the correct module when multiple
// signals are present with varying values, respecting the priority order.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultShowsCorrectTipWhenMultipleSignalsArePresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 1,
                                  /* kOpenedWebsiteInAnotherLanguage */ 1,
                                  /* kUsedGoogleTranslation */ 1,
                                  /* kIsNewUser */ 0,
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  // Since Lens Shop has the highest priority, it should be shown.
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kLensEphemeralModuleShopVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` doesn't show a Lens module when
// `kLensAllowedByEnterprisePolicy` is false, even if other signals are
// present.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultDoesNotShowLensTipWhenEnterprisePolicyDisallows) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kOpenedShoppingWebsite */ 1,
          /* kOpenedWebsiteInAnotherLanguage */ 1,
          /* kUsedGoogleTranslation */ 1,
          /* kIsNewUser */ 0,
          /* kLensNotUsedRecently */ 1,
          /* kLensAllowedByEnterprisePolicy */ 0,  // Disallowed by policy
      });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Verifies that `ComputeCardResult(...)` does not show the Lens Search module
// when the disqualifying signal `kIsNewUser` is present, even if other
// required signals are present.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultDoesNotShowLensSearchWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 0,
                                  /* kOpenedWebsiteInAnotherLanguage */ 0,
                                  /* kUsedGoogleTranslation */ 0,
                                  /* kIsNewUser */ 1,  // Disqualifying signal
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Verifies that `ComputeCardResult(...)` does not show the Lens Shop module
// when the disqualifying signal `kIsNewUser` is present, even if other
// required signals are present.
TEST_F(LensEphemeralModuleTest,
       ComputeCardResultDoesNotShowLensShopWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 1,
                                  /* kOpenedWebsiteInAnotherLanguage */ 0,
                                  /* kUsedGoogleTranslation */ 0,
                                  /* kIsNewUser */ 1,  // Disqualifying signal
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Verifies that `ComputeCardResult(...)` does not show the Lens Translate
// module when the disqualifying signal `kIsNewUser` is present, even if other
// required signals are present.
TEST_F(
    LensEphemeralModuleTest,
    ComputeCardResultDoesNotShowLensTranslateWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module = std::make_unique<LensEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(), {
                                  /* kOpenedShoppingWebsite */ 0,
                                  /* kOpenedWebsiteInAnotherLanguage */ 1,
                                  /* kUsedGoogleTranslation */ 1,
                                  /* kIsNewUser */ 1,  // Disqualifying signal
                                  /* kLensNotUsedRecently */ 1,
                                  /* kLensAllowedByEnterprisePolicy */ 1,
                              });

  CardSelectionSignals selection_signals(&signals, kLensEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that `IsEnabled(…)` returns true when under the impression limit
// and false otherwise.
TEST_F(LensEphemeralModuleTest, TestIsEnabled) {
  // Enable the feature flag for ephemeral modules.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformTipsEphemeralCard,
        {{features::kTipsEphemeralCardModuleMaxImpressionCount, "3"}}},
       {features::kSegmentationPlatformEphemeralCardRanker, {}}},
      {});

  EXPECT_TRUE(LensEphemeralModule::IsEnabled(0));
  EXPECT_TRUE(LensEphemeralModule::IsEnabled(1));
  EXPECT_TRUE(LensEphemeralModule::IsEnabled(2));
  EXPECT_FALSE(LensEphemeralModule::IsEnabled(3));
  EXPECT_FALSE(LensEphemeralModule::IsEnabled(4));
}

}  // namespace segmentation_platform::home_modules
