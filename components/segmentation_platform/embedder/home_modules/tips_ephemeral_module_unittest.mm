// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module.h"

#import "base/test/scoped_feature_list.h"
#import "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/segmentation_platform/public/features.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

namespace {

// Helper function to create an `AllCardSignals` object with the given signal
// values. The `CardSignalMap` is populated with all the required signals for
// the `TipsEphemeralModule`.
AllCardSignals CreateAllCardSignalsFromMap(
    const std::vector<float>& signal_values) {
  // Initialize the `CardSignalMap` with all the required signals for the
  // `TipsEphemeralModule`.
  CardSignalMap signal_map;

  signal_map[kTipsEphemeralModule] = {
      {segmentation_platform::tips_manager::signals::kLensUsed, 0},
      {segmentation_platform::tips_manager::signals::
           kAddressBarPositionChoiceScreenDisplayed,
       1},
      {segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite, 2},
      {segmentation_platform::tips_manager::signals::
           kOpenedWebsiteInAnotherLanguage,
       3},
      {segmentation_platform::tips_manager::signals::kSavedPasswords, 4},
      {segmentation_platform::tips_manager::signals::kUsedGoogleTranslation, 5},
      {segmentation_platform::tips_manager::signals::kUsedPasswordAutofill, 6},
      {segmentation_platform::kHasEnhancedSafeBrowsing, 7},
  };

  return AllCardSignals(signal_map, signal_values);
}

}  // namespace

class TipsEphemeralModuleTest : public testing::Test {
 public:
  TipsEphemeralModuleTest() = default;
  ~TipsEphemeralModuleTest() override = default;

  void SetUp() override {
    // Enable the feature flag for ephemeral modules and set the
    // `kTipsEphemeralCardExperimentTrainParam` to include all variations.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSegmentationPlatformTipsEphemeralCard,
             {{features::kTipsEphemeralCardExperimentTrainParam,
               base::StrCat(
                   {kTipsLensSearchVariation, ",", kTipsSavePasswordsVariation,
                    ",", kTipsEnhancedSafeBrowsingVariation, ",",
                    kTipsAddressBarPositionVariation, ",",
                    kTipsLensShopVariation, ",", kTipsLensTranslateVariation,
                    ",", kTipsAutofillPasswordsVariation})}}},
            {features::kSegmentationPlatformEphemeralCardRanker, {}},
        },
        {});
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that valid module labels are correctly identified.
TEST_F(TipsEphemeralModuleTest, ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(TipsEphemeralModule::IsModuleLabel(kTipsLensSearchVariation));
  EXPECT_TRUE(
      TipsEphemeralModule::IsModuleLabel(kTipsAddressBarPositionVariation));
  EXPECT_TRUE(TipsEphemeralModule::IsModuleLabel(kTipsLensShopVariation));
  EXPECT_TRUE(TipsEphemeralModule::IsModuleLabel(kTipsLensTranslateVariation));
  EXPECT_TRUE(TipsEphemeralModule::IsModuleLabel(kTipsSavePasswordsVariation));
  EXPECT_TRUE(
      TipsEphemeralModule::IsModuleLabel(kTipsAutofillPasswordsVariation));
  EXPECT_TRUE(
      TipsEphemeralModule::IsModuleLabel(kTipsEnhancedSafeBrowsingVariation));
}

// Verifies that invalid module labels are correctly identified.
TEST_F(TipsEphemeralModuleTest, InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(TipsEphemeralModule::IsModuleLabel("some_other_label"));
}

// Verifies that the `OutputLabels(…)` method returns the expected labels.
TEST_F(TipsEphemeralModuleTest, OutputLabelsReturnsExpectedLabels) {
  TipsEphemeralModule ephemeral_module;
  std::vector<std::string> labels = ephemeral_module.OutputLabels();
  EXPECT_EQ(labels.size(), kTipsOutputLabels.size());
  for (const std::string& label : labels) {
    EXPECT_TRUE(kTipsOutputLabels.contains(label));
  }
}

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(TipsEphemeralModuleTest, GetInputsReturnsExpectedInputs) {
  TipsEphemeralModule ephemeral_module;
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module.GetInputs();
  EXPECT_EQ(inputs.size(), 8u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::tips_manager::signals::
                            kAddressBarPositionChoiceScreenDisplayed),
            inputs.end());
  EXPECT_NE(
      inputs.find(segmentation_platform::tips_manager::signals::kLensUsed),
      inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::tips_manager::signals::
                            kOpenedWebsiteInAnotherLanguage),
            inputs.end());
  EXPECT_NE(inputs.find(
                segmentation_platform::tips_manager::signals::kSavedPasswords),
            inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::tips_manager::signals::kUsedGoogleTranslation),
      inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::tips_manager::signals::kUsedPasswordAutofill),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kHasEnhancedSafeBrowsing),
            inputs.end());
}

// Verifies that `ComputeCardResult(…)` returns the forced tip when specified.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultReturnsForcedTipWhenSpecified) {
  // Force the Lens Search tip to be shown.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformEphemeralCardRanker,
        {{features::kEphemeralCardRankerForceShowCardParam,
          kTipsLensSearchVariation}}}},
      {});

  // Create a `CardSelectionSignals` object with all signals set to 0.
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  // Since Lens Search was forced, it should be shown.
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsLensSearchVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` does not show a tip when no signals are
// present.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultDoesNotShowTipWhenNoSignalsArePresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Search tip when the
// corresponding signal is present.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultShowsLensSearchTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 1,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsLensSearchVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Address Bar Position tip when
// the corresponding signal is present.
TEST_F(
    TipsEphemeralModuleTest,
    ComputeCardResultShowsAddressBarPositionTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 1,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsAddressBarPositionVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Shop tip when the
// corresponding signal is present.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultShowsLensShopTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 1,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsLensShopVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Lens Translate tip when the
// corresponding signal is present.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultShowsLensTranslateTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 1,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 1,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsLensTranslateVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Save Passwords tip when the
// corresponding signal is present.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultShowsSavePasswordsTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 1,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsSavePasswordsVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Autofill Passwords tip when
// the corresponding signal is present.
TEST_F(
    TipsEphemeralModuleTest,
    ComputeCardResultShowsAutofillPasswordsTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 1,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsAutofillPasswordsVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the Enhanced Safe Browsing tip
// when the corresponding signal is present.
TEST_F(
    TipsEphemeralModuleTest,
    ComputeCardResultShowsEnhancedSafeBrowsingTipWhenCorrespondingSignalIsPresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 0,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 0,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 0,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 0,
      /* kHasEnhancedSafeBrowsing */ 1,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsEnhancedSafeBrowsingVariation, result.result_label);
}

// Verifies that `ComputeCardResult(…)` shows the correct tip when multiple
// signals are present with varying values.
TEST_F(TipsEphemeralModuleTest,
       ComputeCardResultShowsCorrectTipWhenMultipleSignalsArePresent) {
  AllCardSignals signals = CreateAllCardSignalsFromMap({
      /* kLensUsed */ 1,
      /* kAddressBarPositionChoiceScreenDisplayed */ 0,
      /* kOpenedShoppingWebsite */ 1,
      /* kOpenedWebsiteInAnotherLanguage */ 0,
      /* kSavedPasswords */ 1,
      /* kUsedGoogleTranslation */ 0,
      /* kUsedPasswordAutofill */ 1,
      /* kHasEnhancedSafeBrowsing */ 0,
  });

  CardSelectionSignals selection_signals(&signals, kTipsEphemeralModule);

  auto ephemeral_module = std::make_unique<TipsEphemeralModule>();

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  // Since Lens Search has higher priority, it should be shown.
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kTipsLensSearchVariation, result.result_label);
}

}  // namespace segmentation_platform::home_modules
