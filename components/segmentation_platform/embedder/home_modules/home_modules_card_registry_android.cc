// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"

#include <memory>
#include <vector>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"
#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/quick_delete_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform::home_modules {

HomeModulesCardRegistryAndroid::HomeModulesCardRegistryAndroid(
    PrefService* profile_prefs,
    PrefService* local_state_prefs)
    : HomeModulesCardRegistry(profile_prefs, local_state_prefs) {
  if (AuxiliarySearchPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<AuxiliarySearchPromo>(profile_prefs_));
  }

  // TODO(crbug.com/420897397): Move the forced card check out from each card.
  if (DefaultBrowserPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<DefaultBrowserPromo>(profile_prefs_));
  }

  if (HistorySyncPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<HistorySyncPromo>(profile_prefs_));
  }

  if (TabGroupPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TabGroupPromo>(profile_prefs_));
  }

  if (TabGroupSyncPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TabGroupSyncPromo>(profile_prefs_));
  }

  if (QuickDeletePromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<QuickDeletePromo>(profile_prefs_));
  }

  if (TipsNotificationsPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TipsNotificationsPromo>(profile_prefs_));
  }

  InitializeAfterAddingCards();
}

HomeModulesCardRegistryAndroid::~HomeModulesCardRegistryAndroid() = default;

// static
void HomeModulesCardRegistryAndroid::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Currently no local-state prefs are required for Android ephemeral modules.
}

// static
void HomeModulesCardRegistryAndroid::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  DefaultBrowserPromo::RegisterProfilePrefs(registry);
  TabGroupPromo::RegisterProfilePrefs(registry);
  TabGroupSyncPromo::RegisterProfilePrefs(registry);
  QuickDeletePromo::RegisterProfilePrefs(registry);
  AuxiliarySearchPromo::RegisterProfilePrefs(registry);
  HistorySyncPromo::RegisterProfilePrefs(registry);
  TipsNotificationsPromo::RegisterProfilePrefs(registry);
}

void HomeModulesCardRegistryAndroid::NotifyCardShown(
    std::string_view card_name) {
  for (const auto& card : get_all_cards_by_priority()) {
    if (card->card_name() == card_name) {
      card->OnShow(profile_prefs_, local_state_prefs_);
      break;
    }
  }
}

void HomeModulesCardRegistryAndroid::NotifyCardInteracted(
    std::string_view card_name) {
  for (const auto& card : get_all_cards_by_priority()) {
    if (card->card_name() == card_name) {
      card->OnInteract(profile_prefs_, local_state_prefs_);
      break;
    }
  }
}

}  // namespace segmentation_platform::home_modules
