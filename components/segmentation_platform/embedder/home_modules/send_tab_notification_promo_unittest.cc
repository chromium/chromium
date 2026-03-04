// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/send_tab_to_self/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests SendTabNotificationPromo's functionality.
class SendTabNotificationPromoTest : public testing::Test {
 public:
  SendTabNotificationPromoTest() = default;
  ~SendTabNotificationPromoTest() override = default;

  void SetUp() override {
    Test::SetUp();
    SendTabNotificationPromo::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Validates that ComputeCardResult() returns kTop with the valid signals.
TEST_F(SendTabNotificationPromoTest, TestComputeCardResult) {
  CardSignalMap map;
  map[kSendTabNotificationPromo]
     [kSendTabInfobarReceivedInLastSessionSignalKey] = 0;
  std::vector<float> all_signal_vector{1};
  AllCardSignals all_signals(map, all_signal_vector);

  CardSelectionSignals card_signal(&all_signals, kSendTabNotificationPromo);
  std::unique_ptr<SendTabNotificationPromo> card =
      std::make_unique<SendTabNotificationPromo>();
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

#if BUILDFLAG(IS_IOS)
// Validates that `IsEnabled()` returns true when under the impression limit
// and false otherwise.
//
// Note: `kMaxSendTabNotificationCardImpressions` is 1.
TEST_F(SendTabNotificationPromoTest,
       IsEnabledReturnsFalseWhenImpressionLimitReached) {

  auto card = std::make_unique<SendTabNotificationPromo>();

  // 0 impressions.
  EXPECT_TRUE(SendTabNotificationPromo::IsEnabled(&pref_service_));

  // Simulate an impression.
  card->OnShow(&pref_service_, nullptr);

  // 1 impression (limit reached).
  EXPECT_FALSE(SendTabNotificationPromo::IsEnabled(&pref_service_));
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace segmentation_platform::home_modules
