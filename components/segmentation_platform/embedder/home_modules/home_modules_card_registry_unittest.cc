// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

using ::testing::Contains;
using ::testing::Not;

class HomeModulesCardRegistryTest : public testing::Test {
 public:
  HomeModulesCardRegistryTest() = default;
  ~HomeModulesCardRegistryTest() override = default;

  void SetUp() override {
    Test::SetUp();
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  std::unique_ptr<HomeModulesCardRegistry> registry_;
  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Registry registers the PriceTrackingNotificationPromo card
// when its feature is enabled.
#if BUILDFLAG(IS_IOS)
TEST_F(HomeModulesCardRegistryTest, TestPriceTrackingNotificationPromoCard) {
  feature_list_.InitWithFeatures(
      {commerce::kPriceTrackingPromo},
      {features::kSegmentationPlatformTipsEphemeralCard});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(2u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(1u, registry_->get_label_index(kPriceTrackingNotificationPromo));
  ASSERT_EQ(3u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(1u, all_cards.size());
  ASSERT_EQ(std::string(kPriceTrackingNotificationPromo),
            std::string(all_cards.front()->card_name()));
  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(0u, signal_map.find(kPriceTrackingNotificationPromo)
                    ->second.find("has_subscription")
                    ->second);
}

// Tests that the Registry registers the TipsEphemeralModule cards when the
// tips magic stack is enabled.
TEST_F(HomeModulesCardRegistryTest, TestTipsEphemeralModuleCards) {
  feature_list_.InitWithFeatures(
      {features::kSegmentationPlatformTipsEphemeralCard}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(7u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(1u,
            registry_->get_label_index(kLensEphemeralModuleSearchVariation));
  ASSERT_EQ(15u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(4u, all_cards.size());

  // Verify that the Lens card is registered and has the correct signal index.
  ASSERT_EQ(std::string(kLensEphemeralModule),
            std::string(all_cards.front()->card_name()));
  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(5u, signal_map.find(kLensEphemeralModule)
                    ->second.find(segmentation_platform::kLensNotUsedRecently)
                    ->second);
}

TEST_F(HomeModulesCardRegistryTest, TestSendTabEphemeralModuleCard) {
  feature_list_.InitWithFeaturesAndParameters(
      {{send_tab_to_self::kSendTabToSelfIOSPushNotifications,
        {{send_tab_to_self::kSendTabIOSPushNotificationsWithMagicStackCardParam,
          "true"}}}},
      {features::kSegmentationPlatformTipsEphemeralCard});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(2u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(1u, registry_->get_label_index(kSendTabNotificationPromo));
  ASSERT_EQ(1u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(1u, all_cards.size());
  ASSERT_EQ(std::string(kSendTabNotificationPromo),
            std::string(all_cards.front()->card_name()));
  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(0u, signal_map.find(kSendTabNotificationPromo)
                    ->second.find("send_tab_infobar_received_in_last_session")
                    ->second);
}
#endif

// Tests that the Registry registers the DefaultBrowserPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryTest, TestDefaultBrowserPromoCardEnabled) {
#if BUILDFLAG(IS_ANDROID)
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kDefaultBrowserPromo));
  EXPECT_GE(registry_->all_cards_input_size(), 2u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kDefaultBrowserPromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kDefaultBrowserPromo);
  EXPECT_THAT(
      signalKeys,
      Contains("has_default_browser_promo_reached_limit_in_role_manager"));
  EXPECT_THAT(signalKeys, Contains("is_default_browser_chrome"));
#endif
}

// Tests that the Registry won't register the DefaultBrowserPromo card when it
// is disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestDefaultBrowserPromoCardDisabled) {
#if BUILDFLAG(IS_ANDROID)
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  pref_service_.SetUserPref(kDefaultBrowserPromoImpressionCounterPref,
                            std::make_unique<base::Value>(4));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(),
              Not(Contains(kDefaultBrowserPromo)));
  ASSERT_EQ(0u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kDefaultBrowserPromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kDefaultBrowserPromo);
  EXPECT_THAT(
      signalKeys,
      Not(Contains("has_default_browser_promo_reached_limit_in_role_manager")));
  EXPECT_THAT(signalKeys, Not(Contains("is_default_browser_chrome")));
#endif
}

}  // namespace segmentation_platform::home_modules
