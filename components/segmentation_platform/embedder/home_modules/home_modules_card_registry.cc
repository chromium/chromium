// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include <string_view>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"
#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/public/features.h"
#if BUILDFLAG(IS_IOS)
#include "components/segmentation_platform/embedder/home_modules/address_bar_position_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/autofill_passwords_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/enhanced_safe_browsing_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/lens_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/save_passwords_ephemeral_module.h"
#endif

namespace segmentation_platform::home_modules {

#if BUILDFLAG(IS_ANDROID)
const char kDefaultBrowserPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.default_browser_promo_counter";
#endif

namespace {

#if BUILDFLAG(IS_IOS)
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
#endif

}  // namespace

HomeModulesCardRegistry::HomeModulesCardRegistry(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  CreateAllCards();
}

HomeModulesCardRegistry::HomeModulesCardRegistry(
    PrefService* profile_prefs,
    std::vector<std::unique_ptr<CardSelectionInfo>> cards)
    : profile_prefs_(profile_prefs) {
  all_cards_by_priority_.swap(cards);
  InitializeAfterAddingCards();
}

HomeModulesCardRegistry::~HomeModulesCardRegistry() = default;

//  static
void HomeModulesCardRegistry::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_IOS)
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
#endif

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kDefaultBrowserPromoImpressionCounterPref, 0);
#endif
}

// static
bool HomeModulesCardRegistry::IsEphemeralTipsModuleLabel(
    std::string_view label) {
#if BUILDFLAG(IS_IOS)
  return AddressBarPositionEphemeralModule::IsModuleLabel(label) ||
         AutofillPasswordsEphemeralModule::IsModuleLabel(label) ||
         EnhancedSafeBrowsingEphemeralModule::IsModuleLabel(label) ||
         SavePasswordsEphemeralModule::IsModuleLabel(label) ||
         LensEphemeralModule::IsModuleLabel(label);
#else
  return false;
#endif
}

void HomeModulesCardRegistry::NotifyCardShown(const char* card_name) {
#if BUILDFLAG(IS_IOS)
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
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (strcmp(card_name, kDefaultBrowserPromo) == 0) {
    int freshness_impression_count =
        profile_prefs_->GetInteger(kDefaultBrowserPromoImpressionCounterPref);
    profile_prefs_->SetInteger(kDefaultBrowserPromoImpressionCounterPref,
                               freshness_impression_count + 1);
  }
#endif
}

void HomeModulesCardRegistry::NotifyCardInteracted(const char* card_name) {
#if BUILDFLAG(IS_IOS)
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
#endif
}

void HomeModulesCardRegistry::CreateAllCards() {
#if BUILDFLAG(IS_IOS)
  int price_tracking_promo_count =
      profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
  int send_tab_promo_count =
      profile_prefs_->GetInteger(kSendTabPromoImpressionCounterPref);
  if (PriceTrackingNotificationPromo::IsEnabled(price_tracking_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<PriceTrackingNotificationPromo>(
            price_tracking_promo_count));
  }

  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTipsEphemeralCard)) {
    std::string enabled_variations = features::TipsExperimentTrainEnabled();

    // Iterates the variation labels without extra allocations.
    for (std::string_view variation_label :
         base::SplitString(enabled_variations, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      TipIdentifier identifier = TipIdentifierForOutputLabel(variation_label);

      AddCardForTip(identifier, all_cards_by_priority_, profile_prefs_);
    }
  }
  if (SendTabNotificationPromo::IsEnabled(send_tab_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<SendTabNotificationPromo>(send_tab_promo_count));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  int default_browser_promo_count =
      profile_prefs_->GetInteger(kDefaultBrowserPromoImpressionCounterPref);
  if (DefaultBrowserPromo::IsEnabled(default_browser_promo_count)) {
    all_cards_by_priority_.push_back(std::make_unique<DefaultBrowserPromo>());
  }
#endif
  InitializeAfterAddingCards();
}

void HomeModulesCardRegistry::InitializeAfterAddingCards() {
  size_t input_counter = 0;
  AddCardLabels({kPlaceholderEphemeralModuleLabel});
  for (std::unique_ptr<CardSelectionInfo>& card : all_cards_by_priority_) {
    std::map<SignalKey, size_t> card_signals;
    const auto& card_inputs = card->GetInputs();
    for (const auto& key_and_input : card_inputs) {
      card_signals[key_and_input.first] = input_counter;
      input_counter++;
    }
    card_signal_map_[card->card_name()] = card_signals;

    std::vector<std::string> card_labels = card->OutputLabels();
    if (!card_labels.empty()) {
      AddCardLabels(card_labels);
    } else {
      AddCardLabels({card->card_name()});
    }
  }
  all_cards_input_size_ = input_counter;
}

void HomeModulesCardRegistry::AddCardLabels(
    const std::vector<std::string>& card_labels) {
  for (const std::string& label : card_labels) {
    CHECK(!label_to_output_index_.count(label));
    label_to_output_index_[label] = all_output_labels_.size();
    all_output_labels_.push_back(label);
  }
}

base::WeakPtr<HomeModulesCardRegistry> HomeModulesCardRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace segmentation_platform::home_modules
