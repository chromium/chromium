// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"

#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests PriceTrackingNotificationPromo's functionality.
class PriceTrackingNotificationPromoTest : public testing::Test {
 public:
  PriceTrackingNotificationPromoTest() = default;
  ~PriceTrackingNotificationPromoTest() override = default;

  void SetUp() override { Test::SetUp(); }

  void TearDown() override { Test::TearDown(); }

 protected:
};

// Validates that ComputeCardResult() returns kTop with the valid signals.
TEST_F(PriceTrackingNotificationPromoTest, TestComputeCardResult) {
  CardSignalMap map;
  map[kPriceTrackingNotificationPromo][kHasSubscriptionSignalKey] = 0;
  map[kPriceTrackingNotificationPromo][kIsNewUserSignalKey] = 1;
  map[kPriceTrackingNotificationPromo][kIsSyncedSignalKey] = 2;
  std::vector<float> all_signal_vector{1, 0, 1};
  AllCardSignals all_signals(map, all_signal_vector);

  CardSelectionSignals card_signal(&all_signals,
                                   kPriceTrackingNotificationPromo);
  std::unique_ptr<PriceTrackingNotificationPromo> card =
      std::make_unique<PriceTrackingNotificationPromo>(0);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

}  // namespace segmentation_platform::home_modules
