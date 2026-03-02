// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_ios.h"

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

// Impression counter for the Price Tracking notification promo card.
const char kPriceTrackingPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.price_tracking_promo_counter";
// Impression counter for the Address Bar Position ephemeral module.
const char kAddressBarPositionEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.address_bar_position_ephemeral_module_counter";
// Impression counter for the Autofill Passwords ephemeral module.
const char kAutofillPasswordsEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.autofill_passwords_ephemeral_module_counter";
// Impression counter for the Enhanced Safe Browsing ephemeral module.
const char kEnhancedSafeBrowsingEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.enhanced_safe_browsing_ephemeral_module_counter";
// Impression counter for the Save Passwords ephemeral module.
const char kSavePasswordsEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.save_passwords_ephemeral_module_counter";
// Impression counter for the Lens ephemeral module.
const char kLensEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.lens_ephemeral_module_counter";
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
      int impression_count =
          prefs->GetInteger(kLensEphemeralModuleImpressionCounterPref);
      if (LensEphemeralModule::IsEnabled(impression_count)) {
        cards.push_back(std::make_unique<LensEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kAddressBarPosition: {
      int impression_count = prefs->GetInteger(
          kAddressBarPositionEphemeralModuleImpressionCounterPref);
      if (AddressBarPositionEphemeralModule::IsEnabled(impression_count)) {
        cards.push_back(
            std::make_unique<AddressBarPositionEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kSavePasswords: {
      int impression_count =
          prefs->GetInteger(kSavePasswordsEphemeralModuleImpressionCounterPref);
      if (SavePasswordsEphemeralModule::IsEnabled(impression_count)) {
        cards.push_back(std::make_unique<SavePasswordsEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kAutofillPasswords: {
      int impression_count = prefs->GetInteger(
          kAutofillPasswordsEphemeralModuleImpressionCounterPref);
      if (AutofillPasswordsEphemeralModule::IsEnabled(impression_count)) {
        cards.push_back(
            std::make_unique<AutofillPasswordsEphemeralModule>(prefs));
      }
      break;
    }
    case TipIdentifier::kEnhancedSafeBrowsing: {
      int impression_count = prefs->GetInteger(
          kEnhancedSafeBrowsingEphemeralModuleImpressionCounterPref);
      if (EnhancedSafeBrowsingEphemeralModule::IsEnabled(impression_count)) {
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
  int price_tracking_promo_count =
      profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
  int send_tab_promo_count =
      profile_prefs_->GetInteger(kSendTabPromoImpressionCounterPref);
  int app_bundle_promo_count = local_state_prefs_->GetInteger(
      kAppBundlePromoEphemeralModuleImpressionCounterPref);

  if (PriceTrackingNotificationPromo::IsEnabled(price_tracking_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<PriceTrackingNotificationPromo>(
            price_tracking_promo_count));
  }

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
  registry->RegisterIntegerPref(kPriceTrackingPromoImpressionCounterPref, 0);
  registry->RegisterIntegerPref(kSendTabPromoImpressionCounterPref, 0);
  registry->RegisterIntegerPref(
      kAddressBarPositionEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterIntegerPref(
      kAutofillPasswordsEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterIntegerPref(
      kEnhancedSafeBrowsingEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterIntegerPref(
      kSavePasswordsEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterIntegerPref(kLensEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterBooleanPref(
      kAddressBarPositionEphemeralModuleInteractedPref, false);
  registry->RegisterBooleanPref(kAutofillPasswordsEphemeralModuleInteractedPref,
                                false);
  registry->RegisterBooleanPref(
      kEnhancedSafeBrowsingEphemeralModuleInteractedPref, false);
  registry->RegisterBooleanPref(kSavePasswordsEphemeralModuleInteractedPref,
                                false);
  registry->RegisterBooleanPref(kLensEphemeralModuleInteractedPref, false);
  registry->RegisterBooleanPref(
      kLensEphemeralModuleSearchVariationInteractedPref, false);
  registry->RegisterBooleanPref(kLensEphemeralModuleShopVariationInteractedPref,
                                false);
  registry->RegisterBooleanPref(
      kLensEphemeralModuleTranslateVariationInteractedPref, false);
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
  if (strcmp(card_name, kPriceTrackingNotificationPromo) == 0) {
    int freshness_impression_count =
        profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
    profile_prefs_->SetInteger(kPriceTrackingPromoImpressionCounterPref,
                               freshness_impression_count + 1);
  } else if (strcmp(card_name, kAddressBarPositionEphemeralModule) == 0) {
    int freshness_impression_count = profile_prefs_->GetInteger(
        kAddressBarPositionEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(
        kAddressBarPositionEphemeralModuleImpressionCounterPref,
        freshness_impression_count + 1);
  } else if (strcmp(card_name, kAutofillPasswordsEphemeralModule) == 0) {
    int freshness_impression_count = profile_prefs_->GetInteger(
        kAutofillPasswordsEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(
        kAutofillPasswordsEphemeralModuleImpressionCounterPref,
        freshness_impression_count + 1);
  } else if (strcmp(card_name, kEnhancedSafeBrowsingEphemeralModule) == 0) {
    int freshness_impression_count = profile_prefs_->GetInteger(
        kEnhancedSafeBrowsingEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(
        kEnhancedSafeBrowsingEphemeralModuleImpressionCounterPref,
        freshness_impression_count + 1);
  } else if (strcmp(card_name, kSavePasswordsEphemeralModule) == 0) {
    int freshness_impression_count = profile_prefs_->GetInteger(
        kSavePasswordsEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(
        kSavePasswordsEphemeralModuleImpressionCounterPref,
        freshness_impression_count + 1);
  } else if (strcmp(card_name, kLensEphemeralModule) == 0 ||
             strcmp(card_name, kLensEphemeralModuleSearchVariation) == 0 ||
             strcmp(card_name, kLensEphemeralModuleShopVariation) == 0 ||
             strcmp(card_name, kLensEphemeralModuleTranslateVariation) == 0) {
    int freshness_impression_count =
        profile_prefs_->GetInteger(kLensEphemeralModuleImpressionCounterPref);
    profile_prefs_->SetInteger(kLensEphemeralModuleImpressionCounterPref,
                               freshness_impression_count + 1);
  } else if (strcmp(card_name, kSendTabNotificationPromo) == 0) {
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
  if (strcmp(card_name, kAddressBarPositionEphemeralModule) == 0) {
    profile_prefs_->SetBoolean(kAddressBarPositionEphemeralModuleInteractedPref,
                               true);
  } else if (strcmp(card_name, kAutofillPasswordsEphemeralModule) == 0) {
    profile_prefs_->SetBoolean(kAutofillPasswordsEphemeralModuleInteractedPref,
                               true);
  } else if (strcmp(card_name, kEnhancedSafeBrowsingEphemeralModule) == 0) {
    profile_prefs_->SetBoolean(
        kEnhancedSafeBrowsingEphemeralModuleInteractedPref, true);
  } else if (strcmp(card_name, kSavePasswordsEphemeralModule) == 0) {
    profile_prefs_->SetBoolean(kSavePasswordsEphemeralModuleInteractedPref,
                               true);
  } else if (strcmp(card_name, kLensEphemeralModule) == 0) {
    profile_prefs_->SetBoolean(kLensEphemeralModuleInteractedPref, true);
  } else if (strcmp(card_name, kLensEphemeralModuleSearchVariation) == 0) {
    profile_prefs_->SetBoolean(
        kLensEphemeralModuleSearchVariationInteractedPref, true);
  } else if (strcmp(card_name, kLensEphemeralModuleShopVariation) == 0) {
    profile_prefs_->SetBoolean(kLensEphemeralModuleShopVariationInteractedPref,
                               true);
  } else if (strcmp(card_name, kLensEphemeralModuleTranslateVariation) == 0) {
    profile_prefs_->SetBoolean(
        kLensEphemeralModuleTranslateVariationInteractedPref, true);
  }
}

}  // namespace segmentation_platform::home_modules
