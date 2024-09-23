// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class CardSelectionSignalsTest : public testing::Test {
 public:
  CardSelectionSignalsTest() = default;
  ~CardSelectionSignalsTest() override = default;

  void SetUp() override { Test::SetUp(); }

  void TearDown() override { Test::TearDown(); }

 protected:
};

TEST_F(CardSelectionSignalsTest, ResultConversion) {
  EXPECT_NEAR(EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kLast),
              0.01, 0.00001);
  EXPECT_NEAR(EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kTop), 1,
              0.00001);
}

TEST_F(CardSelectionSignalsTest, GetSignals) {
  CardSignalMap map;
  map["card1"]["signal1"] = 0;
  map["card1"]["signal2"] = 1;
  map["card2"]["signal1"] = 2;

  std::vector<float> all_signal_vector{100, 200, 300};
  AllCardSignals all_signals(map, all_signal_vector);

  CardSelectionSignals card_signal(&all_signals, "card1");
  EXPECT_EQ(card_signal.GetSignal("missing_signal"), std::nullopt);
  std::optional<float> res1 = card_signal.GetSignal("signal2");
  ASSERT_TRUE(res1);
  EXPECT_NEAR(*res1, 200, 0.01);

  CardSelectionSignals missing_card(&all_signals, "missing_card");
  EXPECT_EQ(missing_card.GetSignal("signal1"), std::nullopt);
}

}  // namespace segmentation_platform::home_modules
