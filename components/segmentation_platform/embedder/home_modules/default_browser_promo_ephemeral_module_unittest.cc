// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests the functionality of `DefaultBrowserPromoEphemeralModule`.
class DefaultBrowserPromoEphemeralModuleTest : public testing::Test {
 public:
  DefaultBrowserPromoEphemeralModuleTest() = default;
  ~DefaultBrowserPromoEphemeralModuleTest() override = default;

  void SetUp() override {
    Test::SetUp();
    feature_list_.InitAndEnableFeature(features::kDefaultBrowserMagicStackIos);
  }

  void TearDown() override {
    feature_list_.Reset();
    Test::TearDown();
  }

  void TestComputeCardResultImpl(bool isChromeDefaultBrowser,
                                 EphemeralHomeModuleRank expectedRank) {
    auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {static_cast<float>(isChromeDefaultBrowser)});
    CardSelectionSignals card_signal(&all_signals,
                                     kDefaultBrowserPromoEphemeralModule);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(expectedRank, result.position);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `GetInputs()` returns the expected inputs.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestGetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 2u);

  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
  EXPECT_NE(inputs.find(kIsDefaultBrowserSignalKey), inputs.end());
}

// Tests that `ComputerCardResult()` returns `kTop` with the correct signals.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestComputeCardResultShowsCardForValidSignals) {
  // Show the card if the user is not new and Chrome is not their default
  // browser.
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  AllCardSignals signals = CreateAllCardSignals(
      card.get(), {
                      /* kIsNewUser */ 0,
                      /* isDefaultBrowserChromeIos == */ false,
                  });

  CardSelectionSignals selection_signals(&signals,
                                         kDefaultBrowserPromoEphemeralModule);
  CardSelectionInfo::ShowResult result =
      card->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kTop);
}

// Tests that `ComputerCardResult()` returns `kNotShown` when a user is new.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestComputeCardResultDoesNotShowCardIfNewUser) {
  // Do not show the card if the user is new.
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  AllCardSignals signals = CreateAllCardSignals(
      card.get(), {
                      /* kIsNewUser */ 1,
                      /* isDefaultBrowserChromeIos == */ false,
                  });

  CardSelectionSignals selection_signals(&signals,
                                         kDefaultBrowserPromoEphemeralModule);
  CardSelectionInfo::ShowResult result =
      card->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Tests that `ComputerCardResult()` returns `kNotShown` when a user has Chrome
// set as their default browser.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestComputeCardResultDoesNotShowIfAlreadyDefaultBrowser) {
  // Do not show the card if the user has Chrome as their default browser.
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  AllCardSignals signals = CreateAllCardSignals(
      card.get(), {
                      /* kIsNewUser */ 0,
                      /* isDefaultBrowserChromeIos == */ true,
                  });

  CardSelectionSignals selection_signals(&signals,
                                         kDefaultBrowserPromoEphemeralModule);
  CardSelectionInfo::ShowResult result =
      card->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Tests that `ComputerCardResult()` returns `kNotShown` when a user is new and
// has Chrome set as their default browser.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestComputeCardResultForDisqualifyingSignals) {
  // Do not show the card if the user is new or has Chrome as their default
  // browser.
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  AllCardSignals signals = CreateAllCardSignals(
      card.get(), {
                      /* kIsNewUser */ 1,
                      /* isDefaultBrowserChromeIos == */ true,
                  });

  CardSelectionSignals selection_signals(&signals,
                                         kDefaultBrowserPromoEphemeralModule);
  CardSelectionInfo::ShowResult result =
      card->ComputeCardResult(selection_signals);

  EXPECT_EQ(result.position, EphemeralHomeModuleRank::kNotShown);
}

// Tests that `IsEnabled()` returns `true` when under the impression limit and
// returns `false` otherwise.
TEST_F(DefaultBrowserPromoEphemeralModuleTest, TestIsEnabled) {
  EXPECT_TRUE(DefaultBrowserPromoEphemeralModule::IsEnabled(
      /* impressions */ features::kMaxDefaultBrowserMagicStackIosImpressions
          .Get() -
      1));
  EXPECT_FALSE(DefaultBrowserPromoEphemeralModule::IsEnabled(
      /* impressions */ features::kMaxDefaultBrowserMagicStackIosImpressions
          .Get()));
}

}  // namespace segmentation_platform::home_modules
