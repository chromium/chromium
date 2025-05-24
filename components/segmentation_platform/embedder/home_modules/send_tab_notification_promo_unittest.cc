// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"

#include "base/test/scoped_feature_list.h"
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

  void SetUp() override { Test::SetUp(); }

  void TearDown() override { Test::TearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
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
      std::make_unique<SendTabNotificationPromo>(0);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

// Validates that `IsEnabled(…)` returns true when under the impression limit
// and false otherwise.
//
// Note: `kMaxSendTabNotificationCardImpressions` is 1.
TEST_F(SendTabNotificationPromoTest, TestIsEnabled) {
#if BUILDFLAG(IS_IOS)
  feature_list_.InitWithFeaturesAndParameters(
      {{send_tab_to_self::kSendTabToSelfIOSPushNotifications,
        {{send_tab_to_self::kSendTabIOSPushNotificationsWithMagicStackCardParam,
          "true"}}}},
      {});

  EXPECT_TRUE(SendTabNotificationPromo::IsEnabled(0));
  EXPECT_FALSE(SendTabNotificationPromo::IsEnabled(1));
  EXPECT_FALSE(SendTabNotificationPromo::IsEnabled(2));
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace segmentation_platform::home_modules
