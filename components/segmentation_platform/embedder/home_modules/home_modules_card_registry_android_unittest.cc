// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"
#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/quick_delete_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class HomeModulesCardRegistryAndroidTest : public testing::Test {
 public:
  HomeModulesCardRegistryAndroidTest() = default;
  ~HomeModulesCardRegistryAndroidTest() override = default;

  void SetUp() override {
    Test::SetUp();
    HomeModulesCardRegistryAndroid::RegisterProfilePrefs(
        profile_pref_service_.registry());
    HomeModulesCardRegistryAndroid::RegisterLocalStatePrefs(
        local_state_pref_service_.registry());
  }

 protected:
  std::unique_ptr<HomeModulesCardRegistry> registry_;
  TestingPrefServiceSimple profile_pref_service_;
  TestingPrefServiceSimple local_state_pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Registry registers the DefaultBrowserPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest, TestDefaultBrowserPromoCardEnabled) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  ExpectCardRegistered(registry_.get(), kDefaultBrowserPromo,
                       {"should_show_non_role_manager_default_browser_promo",
                        "has_default_browser_promo_shown_in_other_surface",
                        "is_user_signed_in"});
}

// Tests that the Registry won't register the DefaultBrowserPromo card when it
// is disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest,
       TestDefaultBrowserPromoCardDisabled) {
  // Simulate showing the card enough times to reach its maximum limit (3).
  auto card = std::make_unique<DefaultBrowserPromo>(&profile_pref_service_);
  for (int i = 0; i < 3; ++i) {
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(registry_.get(), kDefaultBrowserPromo,
                          {"should_show_non_role_manager_default_browser_promo",
                           "has_default_browser_promo_shown_in_other_surface",
                           "is_user_signed_in"});
}

// Tests that the Registry registers the TabGroupPromo card when its feature is
// enabled.
TEST_F(HomeModulesCardRegistryAndroidTest, TestTabGroupPromoCardEnabled) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 5u);
  ExpectCardRegistered(
      registry_.get(), kTabGroupPromo,
      {"tab_group_exists", "number_of_tabs", "tab_group_shown_count",
       "is_user_signed_in", "educational_tip_shown_count"});
}

// Tests that the Registry won't register the TabGroupPromo card when it is
// disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest, TestTabGroupPromoCardDisabled) {
  // Simulate showing the card enough times to reach its maximum limit (10).
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto card = std::make_unique<TabGroupPromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(
      registry_.get(), kTabGroupPromo,
      {"tab_group_exists", "number_of_tabs", "tab_group_shown_count",
       "is_user_signed_in", "educational_tip_shown_count"});
}

// Tests that the Registry registers the TabGroupSyncPromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest, TestTabGroupSyncPromoCardEnabled) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  ExpectCardRegistered(registry_.get(), kTabGroupSyncPromo,
                       {"synced_tab_group_exists", "tab_group_sync_shown_count",
                        "educational_tip_shown_count"});
}

// Tests that the Registry won't register the `TabGroupSyncPromo` card when it
// is disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest, TestTabGroupSyncPromoCardDisabled) {
  // Simulate showing the card across enough sessions to reach its max limit.
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto card = std::make_unique<TabGroupSyncPromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(
      registry_.get(), kTabGroupSyncPromo,
      {"synced_tab_group_exists", "tab_group_sync_shown_count",
       "educational_tip_shown_count"});
}

// Tests that the Registry registers the QuickDeletePromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest, TestQuickDeletePromoCardEnabled) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 5u);
  ExpectCardRegistered(registry_.get(), kQuickDeletePromo,
                       {"count_of_clearing_browsing_data",
                        "count_of_clearing_browsing_data_through_quick_delete",
                        "quick_delete_shown_count", "is_user_signed_in",
                        "educational_tip_shown_count"});
}

// Tests that the Registry won't register the QuickDeletePromo card when it is
// disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest, TestQuickDeletePromoCardDisabled) {
  // Simulate showing the card across enough sessions to reach its max limit.
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto card = std::make_unique<QuickDeletePromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(
      registry_.get(), kQuickDeletePromo,
      {"count_of_clearing_browsing_data",
       "count_of_clearing_browsing_data_through_quick_delete",
       "quick_delete_shown_count", "is_user_signed_in",
       "educational_tip_shown_count"});
}

// Tests that the Registry registers the AuxiliarySearchPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest,
       TestAuxiliarySearchPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kAndroidAppIntegrationModule}, {});
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 1u);
  ExpectCardRegistered(registry_.get(), kAuxiliarySearch,
                       {kAuxiliarySearchAvailable});
}

// Tests that the Registry won't register the AuxiliarySearchPromo card when it
// is disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest,
       TestAuxiliarySearchPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kAndroidAppIntegrationModule}, {});

  // Simulate showing the card across enough sessions to reach its max limit.
  int max_impressions = features::kMaxAuxiliarySearchCardImpressions.Get();
  for (int i = 0; i < max_impressions; ++i) {
    auto card = std::make_unique<AuxiliarySearchPromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(registry_.get(), kAuxiliarySearch,
                          {kAuxiliarySearchAvailable});
}

// Tests that the Registry registers the HistorySyncPromo card when its feature
// is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest, TestHistorySyncPromoCardEnabled) {
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  ExpectCardRegistered(registry_.get(), kHistorySyncPromo,
                       {kHistorySyncPromoShownCount, kIsEligibleToHistoryOptIn,
                        "educational_tip_shown_count"});
}

// Tests that the Registry won't register the HistorySyncPromo card when it is
// disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest, TestHistorySyncPromoCardDisabled) {
  // Simulate showing the card across enough sessions to reach its max limit.
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto card = std::make_unique<HistorySyncPromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(
      registry_.get(), kHistorySyncPromo,
      {kHistorySyncPromoShownCount, "educational_tip_shown_count"});
}

// Tests that the Registry registers the TipsNotificationsPromo card when its
// feature is enabled.
TEST_F(HomeModulesCardRegistryAndroidTest,
       TestTipsNotificationsPromoCardEnabled) {
  feature_list_.InitWithFeatures({features::kAndroidTipsNotifications}, {});
  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  EXPECT_GE(registry_->all_cards_input_size(), 3u);
  ExpectCardRegistered(
      registry_.get(), kTipsNotificationsPromo,
      {"tips_notifications_promo_shown_count", "is_eligible_to_tips_opt_in",
       "educational_tip_shown_count"});
}

// Tests that the Registry won't register the TipsNotificationsPromo card when
// it is disabled because of user's impression history.
TEST_F(HomeModulesCardRegistryAndroidTest,
       TestTipsNotificationsPromoCardDisabled) {
  feature_list_.InitWithFeatures({features::kAndroidTipsNotifications}, {});

  // Simulate showing the card across enough sessions to reach its max limit.
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto card =
        std::make_unique<TipsNotificationsPromo>(&profile_pref_service_);
    card->OnShow(&profile_pref_service_, &local_state_pref_service_);
  }

  registry_ = HomeModulesCardRegistry::Create(&profile_pref_service_,
                                              &local_state_pref_service_);

  ExpectCardNotRegistered(
      registry_.get(), kTipsNotificationsPromo,
      {"tips_notifications_promo_shown_count", "is_eligible_to_tips_opt_in",
       "educational_tip_shown_count"});
}

}  // namespace segmentation_platform::home_modules
