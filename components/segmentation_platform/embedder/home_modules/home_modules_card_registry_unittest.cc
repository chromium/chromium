// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

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
TEST_F(HomeModulesCardRegistryTest, TestPriceTrackingNotificationPromoCard) {
#if BUILDFLAG(IS_IOS)
  feature_list_.InitWithFeatures({commerce::kPriceTrackingPromo}, {});
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
#endif
}

// Tests that the Registry registers the TipsEphemeralModule card when the
// tips magic stack is enabled.
TEST_F(HomeModulesCardRegistryTest, TestTipsEphemeralModuleCard) {
#if BUILDFLAG(IS_IOS)
  feature_list_.InitWithFeatures(
      {features::kSegmentationPlatformTipsEphemeralCard}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(8u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(4u, registry_->get_label_index(kTipsLensSearchVariation));
  ASSERT_EQ(8u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(1u, all_cards.size());
  ASSERT_EQ(std::string(kTipsEphemeralModule),
            std::string(all_cards.front()->card_name()));
  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(1u,
            signal_map.find(kTipsEphemeralModule)
                ->second
                .find(segmentation_platform::tips_manager::signals::kLensUsed)
                ->second);
#endif
}

}  // namespace segmentation_platform::home_modules
