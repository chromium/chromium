// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_ios.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/address_bar_position_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/autofill_passwords_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/enhanced_safe_browsing_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/lens_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/save_passwords_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"

namespace segmentation_platform::home_modules {

namespace {

// Impression counter for the Send Tab ephemeral module.
const char kSendTabPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.send_tab_promo_counter";
// Impression counter for the App Bundle promo ephemeral module.
const char kAppBundlePromoEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.app_bundle_promo_ephemeral_module_counter";
// Impression counter for the Default Browser promo ephemeral module.
const char kDefaultBrowserPromoEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.default_browser_promo_ephemeral_module_counter";

// Creates a card corresponding to the given ephemeral `tip` module and adds
// it to the `cards` list if the module is enabled.
void AddCardForTip(TipIdentifier tip,
                   std::vector<std::unique_ptr<CardSelectionInfo>>& cards,
                   PrefService* prefs) {
  switch (tip) {
    case TipIdentifier::kUnknown:
      return;  // Do nothing for unknown tips
    case TipIdentifier::kLensSearch:
    case TipIdentifier::kLensShop:
    case TipIdentifier::kLensTranslate: {
      if (LensEphemeralModule::IsEnabled(prefs)) {
        cards.push_back(std::make_unique<LensEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kAddressBarPosition: {
      if (AddressBarPositionEphemeralModule::IsEnabled(prefs)) {
        cards.push_back(
            std::make_unique<AddressBarPositionEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kSavePasswords: {
      if (SavePasswordsEphemeralModule::IsEnabled(prefs)) {
        cards.push_back(std::make_unique<SavePasswordsEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kAutofillPasswords: {
      if (AutofillPasswordsEphemeralModule::IsEnabled(prefs)) {
        cards.push_back(
            std::make_unique<AutofillPasswordsEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kEnhancedSafeBrowsing: {
      if (EnhancedSafeBrowsingEphemeralModule::IsEnabled(prefs)) {
        cards.push_back(
            std::make_unique<EnhancedSafeBrowsingEphemeralModule>(prefs));
      }
      break;
    }
  }
}

// Returns the default sequence of tips variations for iOS.
std::string GetDefaultTipsExperimentTrain() {
  return base::StrCat({kLensEphemeralModuleSearchVariation, ",",
                       kEnhancedSafeBrowsingEphemeralModule});
}

}  // namespace

HomeModulesCardRegistryIOS::HomeModulesCardRegistryIOS(
    PrefService* profile_prefs,
    PrefService* local_state_prefs)
    : HomeModulesCardRegistry(profile_prefs, local_state_prefs) {
  if (PriceTrackingNotificationPromo::IsEnabled(profile_prefs_)) {
    all_cards_by_priority_.push_back(
        std::make_unique<PriceTrackingNotificationPromo>());
  }

  int send_tab_promo_count =
      profile_prefs_->GetInteger(kSendTabPromoImpressionCounterPref);
  int app_bundle_promo_count = local_state_prefs_->GetInteger(
      kAppBundlePromoEphemeralModuleImpressionCounterPref);
  int default_browser_promo_count = profile_prefs_->GetInteger(
      kDefaultBrowserPromoEphemeralModuleImpressionCounterPref);

  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // Determine the forced card identifier and label, if any.
  TipIdentifier forced_identifier = TipIdentifier::kUnknown;
  std::string_view forced_label;

  if (forced_result.has_value() &&
      forced_result.value().position == EphemeralHomeModuleRank::kTop) {
    forced_label = forced_result.value().result_label.value();
    forced_identifier = TipIdentifierForOutputLabel(forced_label);

    if (forced_identifier != TipIdentifier::kUnknown) {
      AddCardForTip(forced_identifier, all_cards_by_priority_, profile_prefs_);
    }
  }

  // Iterate through variation labels and add unique cards.
  for (std::string_view variation_label :
       base::SplitString(GetDefaultTipsExperimentTrain(), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    TipIdentifier identifier = TipIdentifierForOutputLabel(variation_label);

    // Skip adding if:
    // 1. The identifier is unknown.
    // 2. It matches the forced identifier.
    // 3. Both belong to the same "family" of Lens cards.
    if (identifier == TipIdentifier::kUnknown ||
        identifier == forced_identifier ||
        (LensEphemeralModule::IsModuleLabel(variation_label) &&
         LensEphemeralModule::IsModuleLabel(forced_label))) {
      continue;
    }

    AddCardForTip(identifier, all_cards_by_priority_, profile_prefs_);
  }

  if (SendTabNotificationPromo::IsEnabled(send_tab_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<SendTabNotificationPromo>(send_tab_promo_count));
  }

  if (AppBundlePromoEphemeralModule::IsEnabled(app_bundle_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<AppBundlePromoEphemeralModule>());
  }

  if (DefaultBrowserPromoEphemeralModule::IsEnabled(
          default_browser_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<DefaultBrowserPromoEphemeralModule>());
  }

  InitializeAfterAddingCards();
}

HomeModulesCardRegistryIOS::~HomeModulesCardRegistryIOS() = default;

// static
void HomeModulesCardRegistryIOS::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Local state prefs are used for the `AppBundleEphemeralModule` because this
  // promo relates to app installations on the device level, meaning impressions
  // should be tracked per-device rather than per profile.
  registry->RegisterIntegerPref(
      kAppBundlePromoEphemeralModuleImpressionCounterPref, 0);
}

// static
void HomeModulesCardRegistryIOS::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  PriceTrackingNotificationPromo::RegisterProfilePrefs(registry);
  AddressBarPositionEphemeralModule::RegisterProfilePrefs(registry);
  AutofillPasswordsEphemeralModule::RegisterProfilePrefs(registry);
  EnhancedSafeBrowsingEphemeralModule::RegisterProfilePrefs(registry);
  SavePasswordsEphemeralModule::RegisterProfilePrefs(registry);
  LensEphemeralModule::RegisterProfilePrefs(registry);
  registry->RegisterIntegerPref(kSendTabPromoImpressionCounterPref, 0);
  registry->RegisterIntegerPref(
      kDefaultBrowserPromoEphemeralModuleImpressionCounterPref, 0);
}

// static
bool HomeModulesCardRegistryIOS::IsEphemeralTipsModuleLabel(
    std::string_view label) {
  return AddressBarPositionEphemeralModule::IsModuleLabel(label) ||
         AutofillPasswordsEphemeralModule::IsModuleLabel(label) ||
         EnhancedSafeBrowsingEphemeralModule::IsModuleLabel(label) ||
         SavePasswordsEphemeralModule::IsModuleLabel(label) ||
         LensEphemeralModule::IsModuleLabel(label);
}

void HomeModulesCardRegistryIOS::NotifyCardShown(const char* card_name) {
  // For unmigrated cards, `OnShow()` is empty, so this is a no-op.
  // Execution continues to the legacy blocks below.
  for (const auto& card : get_all_cards_by_priority()) {
    const auto& labels = card->OutputLabels();
    if (strcmp(card->card_name(), card_name) == 0 ||
        (std::find(labels.begin(), labels.end(), card_name) != labels.end())) {
      card->OnShow(profile_prefs_, local_state_prefs_);
      break;
    }
  }

  // TODO(crbug.com/489042527): Remove the legacy if/else block below when
  // all cards have been migrated to the new `OnShow()` lifecycle hook.
  if (strcmp(card_name, kSendTabNotificationPromo) == 0) {
    int impression_count =
        profile_prefs_->GetInteger(kSendTabPromoImpressionCounterPref);
    profile_prefs_->SetInteger(kSendTabPromoImpressionCounterPref,
                               impression_count + 1);
  } else if (strcmp(card_name, kAppBundlePromoEphemeralModule) == 0) {
    int local_impression_count = local_state_prefs_->GetInteger(
        kAppBundlePromoEphemeralModuleImpressionCounterPref);
    local_state_prefs_->SetInteger(
        kAppBundlePromoEphemeralModuleImpressionCounterPref,
        local_impression_count + 1);
  } else if (strcmp(card_name, kDefaultBrowserPromoEphemeralModule) == 0) {
    int impression_count = profile_prefs_->GetInteger(
        kDefaultBrowserPromoEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(
        kDefaultBrowserPromoEphemeralModuleImpressionCounterPref,
        impression_count + 1);
  }
}

void HomeModulesCardRegistryIOS::NotifyCardInteracted(const char* card_name) {
  for (const auto& card : get_all_cards_by_priority()) {
    const auto& labels = card->OutputLabels();
    if (strcmp(card->card_name(), card_name) == 0 ||
        (std::find(labels.begin(), labels.end(), card_name) != labels.end())) {
      card->OnInteract(profile_prefs_, local_state_prefs_);
      break;
    }
  }
}

}  // namespace segmentation_platform::home_modules
