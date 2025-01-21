// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"

#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class AuxiliarySearchPromoTest : public testing::Test {
 public:
  AuxiliarySearchPromoTest() = default;
  ~AuxiliarySearchPromoTest() override = default;
};

TEST_F(AuxiliarySearchPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<AuxiliarySearchPromo>();
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 1u);
  EXPECT_NE(inputs.find(kAuxiliarySearchAvailable), inputs.end());
}

TEST_F(AuxiliarySearchPromoTest, TestComputeCardResult_Shown) {
  auto card = std::make_unique<AuxiliarySearchPromo>();
  AllCardSignals all_signals =
      CreateAllCardSignals(card.get(), {/* kAuxiliarySearchAvailable */ 1});
  CardSelectionSignals card_signal(&all_signals, kAuxiliarySearch);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

TEST_F(AuxiliarySearchPromoTest, TestComputeCardResult_NotShown) {
  auto card = std::make_unique<AuxiliarySearchPromo>();
  AllCardSignals all_signals =
      CreateAllCardSignals(card.get(), {/* kAuxiliarySearchAvailable */ 0});
  CardSelectionSignals card_signal(&all_signals, kAuxiliarySearch);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

}  // namespace segmentation_platform::home_modules
