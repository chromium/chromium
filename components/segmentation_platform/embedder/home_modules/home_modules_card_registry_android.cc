// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"

#include <memory>
#include <string>
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

const char kTipsNotificationsPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tips_notifications_promo_counter";
const char kTipsNotificationsPromoInteractedPref[] =
    "ephemeral_pref_interacted.tips_notifications_promo_interacted";

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

  int tips_notifications_promo_show_count =
      profile_prefs_->GetInteger(kTipsNotificationsPromoImpressionCounterPref);
  if (TipsNotificationsPromo::IsEnabled(tips_notifications_promo_show_count)) {
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
  registry->RegisterIntegerPref(kTipsNotificationsPromoImpressionCounterPref,
                                0);
  registry->RegisterBooleanPref(kTipsNotificationsPromoInteractedPref, false);
}

void HomeModulesCardRegistryAndroid::NotifyCardShown(const char* card_name) {
  // For unmigrated cards, `OnShow()` is empty, so this is a no-op.
  // Execution continues to the legacy blocks below.
  for (const auto& card : get_all_cards_by_priority()) {
    if (strcmp(card->card_name(), card_name) == 0) {
      card->OnShow(profile_prefs_, local_state_prefs_);
      break;
    }
  }

  // TODO(crbug.com/489042527): Remove the legacy if/else block below when
  // all cards have been migrated to the new `OnShow()` lifecycle hook.
  if (ShouldNotifyCardShownPerSession(card_name)) {
    // Educational tip cards, except for the default browser promo card, will
    // send a notification when the card is shown once per session, rather than
    // every time it is displayed.
    if (strcmp(card_name, kTipsNotificationsPromo) == 0) {
      int freshness_impression_count = profile_prefs_->GetInteger(
          kTipsNotificationsPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kTipsNotificationsPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    }
  }
}

void HomeModulesCardRegistryAndroid::NotifyCardInteracted(
    const char* card_name) {
  // For unmigrated cards, `OnInteract()` is empty, so this is a no-op.
  // Execution continues to the legacy blocks below.
  for (const auto& card : get_all_cards_by_priority()) {
    if (strcmp(card->card_name(), card_name) == 0) {
      card->OnInteract(profile_prefs_, local_state_prefs_);
      break;
    }
  }

  // TODO(crbug.com/489042527): Remove the legacy if/else block below when
  // all cards have been migrated to the new `OnInteract()` lifecycle hook.
  if (strcmp(card_name, kTipsNotificationsPromo) == 0) {
    profile_prefs_->SetBoolean(kTipsNotificationsPromoInteractedPref, true);
  }
}

bool HomeModulesCardRegistryAndroid::ShouldNotifyCardShownPerSession(
    const std::string& card_name) {
  if (shown_in_current_session_.find(card_name) !=
      shown_in_current_session_.end()) {
    return false;
  }

  shown_in_current_session_.insert(card_name);
  return true;
}

}  // namespace segmentation_platform::home_modules
