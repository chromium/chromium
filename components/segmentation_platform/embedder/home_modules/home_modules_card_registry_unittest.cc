// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include <algorithm>

#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_switches.h"
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
// Tips (Magic Stack) is enabled.
TEST_F(HomeModulesCardRegistryTest, TestTipsEphemeralModuleCards) {
  feature_list_.InitWithFeatures(
      {features::kSegmentationPlatformTipsEphemeralCard}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(6u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(2u,
            registry_->get_label_index(kLensEphemeralModuleSearchVariation));
  ASSERT_EQ(12u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(3u, all_cards.size());

  // Verify that the Lens card is registered.
  ASSERT_TRUE(std::any_of(all_cards.begin(), all_cards.end(),
                          [](const std::unique_ptr<CardSelectionInfo>& card) {
                            return card->card_name() == kLensEphemeralModule;
                          }));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(8u, signal_map.find(kLensEphemeralModule)
                    ->second.find(segmentation_platform::kLensNotUsedRecently)
                    ->second);
}

// Tests that the Registry registers the Send Tab to Self ephemeral module card
// when the send-tab-to-self feature with Magic Stack param is enabled and the
// Tips (Magic Stack) is also enabled.
TEST_F(HomeModulesCardRegistryTest, TestSendTabEphemeralModuleCard) {
  feature_list_.InitWithFeaturesAndParameters(
      {{send_tab_to_self::kSendTabToSelfIOSPushNotifications,
        {{send_tab_to_self::kSendTabIOSPushNotificationsWithMagicStackCardParam,
          "true"}}}},
      {features::kSegmentationPlatformTipsEphemeralCard});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  ASSERT_EQ(3u, registry_->all_output_labels().size());
  ASSERT_EQ(0u, registry_->get_label_index(kPlaceholderEphemeralModuleLabel));
  ASSERT_EQ(2u, registry_->get_label_index(kSendTabNotificationPromo));
  ASSERT_EQ(4u, registry_->all_cards_input_size());
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  ASSERT_EQ(2u, all_cards.size());
  // Verify that the Send Tab Notification Promo card is registered.
  ASSERT_TRUE(std::any_of(all_cards.begin(), all_cards.end(),
                          [](const std::unique_ptr<CardSelectionInfo>& card) {
                            return card->card_name() ==
                                   kSendTabNotificationPromo;
                          }));
  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  ASSERT_EQ(3u, signal_map.find(kSendTabNotificationPromo)
                    ->second.find("send_tab_infobar_received_in_last_session")
                    ->second);
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Tests that the Registry registers the DefaultBrowserPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryTest, TestDefaultBrowserPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kDefaultBrowserPromo));
  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kDefaultBrowserPromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kDefaultBrowserPromo);
  EXPECT_THAT(signalKeys,
              Contains("should_show_non_role_manager_default_browser_promo"));
  EXPECT_THAT(signalKeys,
              Contains("has_default_browser_promo_shown_in_other_surface"));
  EXPECT_THAT(signalKeys, Contains("is_user_signed_in"));
}

// Tests that the Registry won't register the DefaultBrowserPromo card when it
// is disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestDefaultBrowserPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  pref_service_.SetUserPref(kDefaultBrowserPromoImpressionCounterPref,
                            std::make_unique<base::Value>(4));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(),
              Not(Contains(kDefaultBrowserPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kDefaultBrowserPromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kDefaultBrowserPromo);
  EXPECT_THAT(
      signalKeys,
      Not(Contains("should_show_non_role_manager_default_browser_promo")));
  EXPECT_THAT(
      signalKeys,
      Not(Contains("has_default_browser_promo_shown_in_other_surface")));
  EXPECT_THAT(signalKeys, Not(Contains("is_user_signed_in")));
}

// Tests that the Registry registers the TabGroupPromo card when its feature is
// enabled.
TEST_F(HomeModulesCardRegistryTest, TestTabGroupPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kTabGroupPromo));
  EXPECT_GE(registry_->all_cards_input_size(), 5u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kTabGroupPromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kTabGroupPromo);
  EXPECT_THAT(signalKeys, Contains("tab_group_exists"));
  EXPECT_THAT(signalKeys, Contains("number_of_tabs"));
  EXPECT_THAT(signalKeys, Contains("tab_group_shown_count"));
  EXPECT_THAT(signalKeys, Contains("is_user_signed_in"));
  EXPECT_THAT(signalKeys, Contains("educational_tip_shown_count"));
}

// Tests that the Registry won't register the TabGroupPromo card when it is
// disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestTabGroupPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  pref_service_.SetUserPref(kTabGroupPromoImpressionCounterPref,
                            std::make_unique<base::Value>(11));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kTabGroupPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kTabGroupPromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kTabGroupPromo);
  EXPECT_THAT(signalKeys, Not(Contains("tab_group_exists")));
  EXPECT_THAT(signalKeys, Not(Contains("number_of_tabs")));
  EXPECT_THAT(signalKeys, Not(Contains("tab_group_shown_count")));
  EXPECT_THAT(signalKeys, Not(Contains("is_user_signed_in")));
  EXPECT_THAT(signalKeys, Not(Contains("educational_tip_shown_count")));
}

// Tests that the Registry won't register the TabGroupPromo card when it is
// disabled because of only show another educational tip module.
TEST_F(HomeModulesCardRegistryTest,
       TestTabGroupPromoCardDisabledForSingleModuleConstraint) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kEducationalTipModule,
        {{"names_of_ephemeral_cards_to_show", kTabGroupSyncPromo}}}},
      {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kTabGroupPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kTabGroupPromo)));
}

// Tests that for educational tip cards, except for the default browser promo
// card, could send a notification when the card is shown once per session,
// rather than every time it is displayed.
TEST_F(HomeModulesCardRegistryTest, TestShouldNotifyCardShownPerSession) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);
  const char* card_name_1 = "TabGroupPromo";
  const char* card_name_2 = "TabGroupSyncPromo";
  EXPECT_TRUE(registry_->ShouldNotifyCardShownPerSession(card_name_1));
  EXPECT_FALSE(registry_->ShouldNotifyCardShownPerSession(card_name_1));
  EXPECT_TRUE(registry_->ShouldNotifyCardShownPerSession(card_name_2));
  EXPECT_FALSE(registry_->ShouldNotifyCardShownPerSession(card_name_2));
}

// Tests that the Registry registers the TabGroupSyncPromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryTest, TestTabGroupSyncPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kTabGroupSyncPromo));
  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kTabGroupSyncPromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kTabGroupSyncPromo);
  EXPECT_THAT(signalKeys, Contains("synced_tab_group_exists"));
  EXPECT_THAT(signalKeys, Contains("tab_group_sync_shown_count"));
  EXPECT_THAT(signalKeys, Contains("educational_tip_shown_count"));
}

// Tests that the Registry won't register the TabGroupSyncPromo card when it is
// disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestTabGroupSyncPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  pref_service_.SetUserPref(kTabGroupSyncPromoImpressionCounterPref,
                            std::make_unique<base::Value>(11));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(),
              Not(Contains(kTabGroupSyncPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kTabGroupSyncPromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kTabGroupSyncPromo);
  EXPECT_THAT(signalKeys, Not(Contains("synced_tab_group_exists")));
  EXPECT_THAT(signalKeys, Not(Contains("tab_group_sync_shown_count")));
  EXPECT_THAT(signalKeys, Not(Contains("educational_tip_shown_count")));
}

// Tests that the Registry won't register the TabGroupSyncPromo card when it is
// disabled because of only show another educational tip module.
TEST_F(HomeModulesCardRegistryTest,
       TestTabGroupSyncPromoCardDisabledForSingleModuleConstraint) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kEducationalTipModule,
        {{"names_of_ephemeral_cards_to_show", kTabGroupPromo}}}},
      {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(),
              Not(Contains(kTabGroupSyncPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kTabGroupSyncPromo)));
}

// Tests that the Registry registers the QuickDeletePromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryTest, TestQuickDeletePromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kQuickDeletePromo));
  EXPECT_GE(registry_->all_cards_input_size(), 5u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kQuickDeletePromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kQuickDeletePromo);
  EXPECT_THAT(signalKeys, Contains("count_of_clearing_browsing_data"));
  EXPECT_THAT(signalKeys,
              Contains("count_of_clearing_browsing_data_through_quick_delete"));
  EXPECT_THAT(signalKeys, Contains("quick_delete_shown_count"));
  EXPECT_THAT(signalKeys, Contains("is_user_signed_in"));
  EXPECT_THAT(signalKeys, Contains("educational_tip_shown_count"));
}

// Tests that the Registry won't register the QuickDeletePromo card when it is
// disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestQuickDeletePromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kEducationalTipModule}, {});
  pref_service_.SetUserPref(kQuickDeletePromoImpressionCounterPref,
                            std::make_unique<base::Value>(11));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kQuickDeletePromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kQuickDeletePromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kQuickDeletePromo);
  EXPECT_THAT(signalKeys, Not(Contains("count_of_clearing_browsing_data")));
  EXPECT_THAT(
      signalKeys,
      Not(Contains("count_of_clearing_browsing_data_through_quick_delete")));
  EXPECT_THAT(signalKeys, Not(Contains("quick_delete_shown_count")));
  EXPECT_THAT(signalKeys, Not(Contains("is_user_signed_in")));
  EXPECT_THAT(signalKeys, Not(Contains("educational_tip_shown_count")));
}

// Tests that the Registry won't register the QuickDeletePromo card when it is
// disabled because of only show another educational tip module.
TEST_F(HomeModulesCardRegistryTest,
       TestQuickDeletePromoCardDisabledForSingleModuleConstraint) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kEducationalTipModule,
        {{"names_of_ephemeral_cards_to_show", kTabGroupPromo}}}},
      {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kQuickDeletePromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kQuickDeletePromo)));
}

// Tests that the Registry registers the AuxiliarySearchPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryTest, TestAuxiliarySearchPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kAndroidAppIntegrationModule}, {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kAuxiliarySearch));
  EXPECT_GE(registry_->all_cards_input_size(), 1u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kAuxiliarySearch));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kAuxiliarySearch);
  EXPECT_THAT(signalKeys, Contains(kAuxiliarySearchAvailable));
}

// Tests that the Registry won't register the AuxiliarySearchPromo card when it
// is disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestAuxiliarySearchPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kAndroidAppIntegrationModule}, {});
  pref_service_.SetUserPref(kAuxiliarySearchPromoImpressionCounterPref,
                            std::make_unique<base::Value>(4));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kAuxiliarySearch)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kAuxiliarySearch)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kAuxiliarySearch);
  EXPECT_THAT(signalKeys, Not(Contains(kAuxiliarySearchAvailable)));
}

// Tests that the Registry registers the HistorySyncPromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryTest, TestHistorySyncPromoCardEnabled) {
  feature_list_.InitWithFeatures(
      {features::kEducationalTipModule, switches::kHistoryOptInEducationalTip},
      {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Contains(kHistorySyncPromo));
  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Contains(kHistorySyncPromo));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kHistorySyncPromo);
  EXPECT_THAT(signalKeys, Contains(kHistorySyncPromoShownCount));
  EXPECT_THAT(signalKeys, Contains(kIsEligibleToHistoryOptIn));
  EXPECT_THAT(signalKeys, Contains("educational_tip_shown_count"));
}

// Tests that the Registry won't register the HistorySyncPromo card when it is
// disabled because of user's interaction history.
TEST_F(HomeModulesCardRegistryTest, TestHistorySyncPromoCardDisabled) {
  feature_list_.InitWithFeatures(
      {features::kEducationalTipModule, switches::kHistoryOptInEducationalTip},
      {});
  pref_service_.SetUserPref(kHistorySyncPromoImpressionCounterPref,
                            std::make_unique<base::Value>(11));
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kHistorySyncPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kHistorySyncPromo)));

  const CardSignalMap& signal_map = registry_->get_card_signal_map();
  std::vector<std::string> signalKeys =
      GetSignalKeys(signal_map, kHistorySyncPromo);
  EXPECT_THAT(signalKeys,
              Not(Contains("history_sync_educational_promo_shown_count")));
  EXPECT_THAT(signalKeys, Not(Contains("educational_tip_shown_count")));
}

// Tests that the Registry won't register the HistorySyncPromo card when it is
// disabled because of only show another educational tip module.
TEST_F(HomeModulesCardRegistryTest,
       TestHistorySyncPromoCardDisabledForSingleModuleConstraint) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kEducationalTipModule,
        {{"names_of_ephemeral_cards_to_show", kTabGroupSyncPromo}}}},
      {});
  registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);

  EXPECT_THAT(registry_->all_output_labels(), Not(Contains(kHistorySyncPromo)));
  EXPECT_GE(registry_->all_cards_input_size(), 0u);
  const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards =
      registry_->get_all_cards_by_priority();
  std::vector<std::string> card_names = ExtractCardNames(all_cards);
  EXPECT_THAT(card_names, Not(Contains(kHistorySyncPromo)));
}

#endif

}  // namespace segmentation_platform::home_modules
