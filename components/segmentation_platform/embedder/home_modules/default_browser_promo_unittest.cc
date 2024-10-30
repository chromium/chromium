// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"

#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests DefaultBrowserPromo's functionality.
class DefaultBrowserPromoTest : public testing::Test {
 public:
  DefaultBrowserPromoTest() = default;
  ~DefaultBrowserPromoTest() override = default;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(DefaultBrowserPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<DefaultBrowserPromo>();
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 2u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsDefaultBrowserChrome),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::
                            kHasDefaultBrowserPromoReachedLimitInRoleManager),
            inputs.end());
}

// Validates that ComputeCardResult() returns kTop when default browser promo
// card is enabled.
TEST_F(DefaultBrowserPromoTest, TestComputeCardResultWithCardEnabled) {
  auto card = std::make_unique<DefaultBrowserPromo>();
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kHasDefaultBrowserPromoReachedLimitInRoleManager */ 1,
                   /* kIsDefaultBrowserChrome */ 0});
  CardSelectionSignals card_signal(&all_signals, kDefaultBrowserPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

// Validates that when the default browser promo card is disabled because the
// promo has not reached its limit in the role manager, the ComputeCardResult()
// function returns kNotShown.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForNotReachLimit) {
  auto card = std::make_unique<DefaultBrowserPromo>();
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kHasDefaultBrowserPromoReachedLimitInRoleManager */ 0,
                   /* kIsDefaultBrowserChrome */ 0});
  CardSelectionSignals card_signal(&all_signals, kDefaultBrowserPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// default browser promo card is disabled because the user already has Chrome
// set as the default browser.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForDefaultBrowserIsChrome) {
  auto card = std::make_unique<DefaultBrowserPromo>();
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kHasDefaultBrowserPromoReachedLimitInRoleManager */ 1,
                   /* kIsDefaultBrowserChrome */ 1});
  CardSelectionSignals card_signal(&all_signals, kDefaultBrowserPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// default browser promo card is disabled because the promo hasn't reached its
// limit in the role manager and the user already has Chrome set as the default
// browser.
TEST_F(DefaultBrowserPromoTest, TestComputeCardResultWithCardDisabled) {
  auto card = std::make_unique<DefaultBrowserPromo>();
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kHasDefaultBrowserPromoReachedLimitInRoleManager */ 0,
                   /* kIsDefaultBrowserChrome */ 1});
  CardSelectionSignals card_signal(&all_signals, kDefaultBrowserPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

}  // namespace segmentation_platform::home_modules
