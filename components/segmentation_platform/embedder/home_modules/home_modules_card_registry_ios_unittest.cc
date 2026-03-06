// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_ios.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class HomeModulesCardRegistryIOSTest : public testing::Test {
 public:
  HomeModulesCardRegistryIOSTest() = default;
  ~HomeModulesCardRegistryIOSTest() override = default;

  void SetUp() override {
    Test::SetUp();
    HomeModulesCardRegistryIOS::RegisterProfilePrefs(
        profile_pref_service_.registry());
    HomeModulesCardRegistryIOS::RegisterLocalStatePrefs(
        local_state_pref_service_.registry());
  }

 protected:
  std::unique_ptr<HomeModulesCardRegistry> registry_;
  TestingPrefServiceSimple profile_pref_service_;
  TestingPrefServiceSimple local_state_pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Registry registers the PriceTrackingNotificationPromo card
// when its feature is enabled.
TEST_F(HomeModulesCardRegistryIOSTest, TestPriceTrackingNotificationPromoCard) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ASSERT_EQ(9u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(1u, registry_->get_label_index(kPriceTrackingNotificationPromo));
  ASSERT_EQ(16u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(6u, all_cards.size());
  ASSERT_EQ(std::string(kPriceTrackingNotificationPromo),
            std::string(all_cards.front()->card_name()));

  ExpectCardRegistered(registry_.get(), kPriceTrackingNotificationPromo,
                       {"has_subscription"});

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(0u, signal_map.find(kPriceTrackingNotificationPromo)
                    ->second.find("has_subscription")
                    ->second);
}

// Tests that the Registry registers the TipsEphemeralModule cards when the
// Tips (Magic Stack) is enabled.
TEST_F(HomeModulesCardRegistryIOSTest, TestTipsEphemeralModuleCards) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ASSERT_EQ(9u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(2u,
            registry_->get_label_index(kLensEphemeralModuleSearchVariation));
  ASSERT_EQ(16u, registry_->all_cards_input_size());

  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(6u, all_cards.size());

  EXPECT_THAT(ExtractCardNames(all_cards),
              testing::Contains(kLensEphemeralModule));

  std::vector<std::string> signal_keys =
      GetSignalKeys(registry_->get_card_signal_map(), kLensEphemeralModule);
  EXPECT_THAT(signal_keys,
              testing::Contains(segmentation_platform::kLensNotUsedRecently));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(8u, signal_map.find(kLensEphemeralModule)
                    ->second.find(segmentation_platform::kLensNotUsedRecently)
                    ->second);
}

// Tests that the Registry registers the Send Tab to Self ephemeral module card
// when the send-tab-to-self feature with Magic Stack param is enabled and the
// Tips (Magic Stack) is also enabled.
TEST_F(HomeModulesCardRegistryIOSTest, TestSendTabEphemeralModuleCard) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ASSERT_EQ(9u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(6u, registry_->get_label_index(kSendTabNotificationPromo));
  ASSERT_EQ(16u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(6u, all_cards.size());

  ExpectCardRegistered(registry_.get(), kSendTabNotificationPromo,
                       {"send_tab_infobar_received_in_last_session"});

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(12u, signal_map.find(kSendTabNotificationPromo)
                     ->second.find("send_tab_infobar_received_in_last_session")
                     ->second);
}

// Tests that the Registry registers the `AppBundlePromoEphemeralModule` card
// when its feature is enabled.
TEST_F(HomeModulesCardRegistryIOSTest, TestAppBundlePromoCard) {
  feature_list_.InitAndEnableFeature(features::kAppBundlePromoEphemeralCard);
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardRegistered(registry_.get(), kAppBundlePromoEphemeralModule,
                       {kAppBundleAppsInstalledCountSignalKey});
}

// Tests that the Registry registers the `DefaultBrowserPromoEphemeralModule`
// card when its feature is enabled.
TEST_F(HomeModulesCardRegistryIOSTest, TestDefaultBrowserPromoCard) {
  feature_list_.InitAndEnableFeature(features::kDefaultBrowserMagicStackIos);
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardRegistered(registry_.get(), kDefaultBrowserPromoEphemeralModule,
                       {kIsDefaultBrowserSignalKey});
}

}  // namespace segmentation_platform::home_modules
