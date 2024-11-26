// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/autofill_passwords_ephemeral_module.h"

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

class AutofillPasswordsEphemeralModuleTest : public testing::Test {
 public:
  AutofillPasswordsEphemeralModuleTest() = default;
  ~AutofillPasswordsEphemeralModuleTest() override = default;

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
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ValidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_TRUE(AutofillPasswordsEphemeralModule::IsModuleLabel(
      kAutofillPasswordsEphemeralModule));
}

// Verifies that invalid module labels are correctly identified.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       InvalidModuleLabelsAreIdentifiedCorrectly) {
  EXPECT_FALSE(
      AutofillPasswordsEphemeralModule::IsModuleLabel("some_other_label"));
}

// Verifies that the `OutputLabels(…)` method returns the expected labels.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       OutputLabelsReturnsExpectedLabels) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);
  std::vector<std::string> labels = ephemeral_module->OutputLabels();
  ASSERT_EQ(labels.size(), 1u);
  ASSERT_EQ(labels.front(), kAutofillPasswordsEphemeralModule);
}

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(AutofillPasswordsEphemeralModuleTest, GetInputsReturnsExpectedInputs) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = ephemeral_module->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kDidNotUsePasswordAutofill),
            inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::kPasswordManagerAllowedByEnterprisePolicy),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
}

// Verifies that `ComputeCardResult(…)` does not show the module when no signals
// are present.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenNoSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kDidNotUsePasswordAutofill */ 0,
          /* kIsNewUser */ 0,
          /* kPasswordManagerAllowedByEnterprisePolicy */ 0,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kAutofillPasswordsEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` does not show the module when the
// corresponding signals are partially missing.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenSomeSignalsAreMissing) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kDidNotUsePasswordAutofill */ 0,
          /* kIsNewUser */ 0,
          /* kPasswordManagerAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kAutofillPasswordsEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(…)` shows the Autofill Passwords module
// when the corresponding signals are present.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ComputeCardResultShowsModuleWhenCorrespondingSignalsArePresent) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kDidNotUsePasswordAutofill */ 1,
          /* kIsNewUser */ 0,
          /* kPasswordManagerAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kAutofillPasswordsEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
  EXPECT_EQ(kAutofillPasswordsEphemeralModule, result.result_label);
}

// Verifies that `ComputeCardResult(…)` does not show the module when the
// corresponding signals are present but the password manager is not allowed by
// enterprise policy.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenEnterprisePolicyDisallows) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kDidNotUsePasswordAutofill */ 1,
          /* kIsNewUser */ 0,
          /* kPasswordManagerAllowedByEnterprisePolicy */ 0,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kAutofillPasswordsEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Verifies that `ComputeCardResult(...)` does not show the module when the
// disqualifying signal `kIsNewUser` is present, even if other required signals
// are present.
TEST_F(AutofillPasswordsEphemeralModuleTest,
       ComputeCardResultDoesNotShowModuleWhenDisqualifyingSignalIsPresent) {
  auto ephemeral_module =
      std::make_unique<AutofillPasswordsEphemeralModule>(&pref_service_);

  AllCardSignals signals = CreateAllCardSignals(
      ephemeral_module.get(),
      {
          /* kDidNotUsePasswordAutofill */ 1,
          /* kIsNewUser */ 1,  // Disqualifying signal
          /* kPasswordManagerAllowedByEnterprisePolicy */ 1,
      });

  CardSelectionSignals selection_signals(&signals,
                                         kAutofillPasswordsEphemeralModule);

  CardSelectionInfo::ShowResult result =
      ephemeral_module->ComputeCardResult(selection_signals);

  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that `IsEnabled(…)` returns true when under the impression limit
// and false otherwise.
TEST_F(AutofillPasswordsEphemeralModuleTest, TestIsEnabled) {
  // Enable the feature flag for ephemeral modules.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformTipsEphemeralCard,
        {{features::kTipsEphemeralCardModuleMaxImpressionCount, "3"}}},
       {features::kSegmentationPlatformEphemeralCardRanker, {}}},
      {});

  EXPECT_TRUE(AutofillPasswordsEphemeralModule::IsEnabled(0));
  EXPECT_TRUE(AutofillPasswordsEphemeralModule::IsEnabled(1));
  EXPECT_TRUE(AutofillPasswordsEphemeralModule::IsEnabled(2));
  EXPECT_FALSE(AutofillPasswordsEphemeralModule::IsEnabled(3));
  EXPECT_FALSE(AutofillPasswordsEphemeralModule::IsEnabled(4));
}

}  // namespace segmentation_platform::home_modules
