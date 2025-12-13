// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/quick_delete_promo.h"
#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#if BUILDFLAG(IS_IOS)
#include "components/segmentation_platform/embedder/home_modules/address_bar_position_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/autofill_passwords_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/enhanced_safe_browsing_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/lens_ephemeral_module.h"
#include "components/segmentation_platform/embedder/home_modules/save_passwords_ephemeral_module.h"
#endif

namespace segmentation_platform::home_modules {

#if BUILDFLAG(IS_ANDROID)
const char kAuxiliarySearchPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.auxiliary_search_promo_counter";
const char kAuxiliarySearchPromoInteractedPref[] =
    "ephemeral_pref_interacted.auxiliary_search_promo_interacted";
const char kDefaultBrowserPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.default_browser_promo_counter";
const char kDefaultBrowserPromoInteractedPref[] =
    "ephemeral_pref_interacted.default_browser_promo_interacted";
const char kTabGroupPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tab_group_promo_counter";
const char kTabGroupPromoInteractedPref[] =
    "ephemeral_pref_interacted.tab_group_promo_interacted";
const char kTabGroupSyncPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tab_group_sync_promo_counter";
const char kTabGroupSyncPromoInteractedPref[] =
    "ephemeral_pref_interacted.tab_group_sync_promo_interacted";
const char kQuickDeletePromoImpressionCounterPref[] =
    "ephemeral_pref_counter.quick_delete_promo_counter";
const char kQuickDeletePromoInteractedPref[] =
    "ephemeral_pref_interacted.quick_delete_promo_interacted";
const char kHistorySyncPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.history_sync_promo_counter";
const char kHistorySyncPromoInteractedPref[] =
    "ephemeral_pref_interacted.history_sync_promo_interacted";
const char kTipsNotificationsPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tips_notifications_promo_counter";
const char kTipsNotificationsPromoInteractedPref[] =
    "ephemeral_pref_interacted.tips_notifications_promo_interacted";
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
#endif

}  // namespace

HomeModulesCardRegistry::HomeModulesCardRegistry(PrefService* profile_prefs,
                                                 PrefService* local_state_prefs)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  CHECK(profile_prefs);
  CHECK(local_state_prefs);
  CreateAllCards();
}

HomeModulesCardRegistry::HomeModulesCardRegistry(
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    std::vector<std::unique_ptr<CardSelectionInfo>> cards)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  CHECK(profile_prefs);
  CHECK(local_state_prefs);
  all_cards_by_priority_.swap(cards);
  InitializeAfterAddingCards();
}

HomeModulesCardRegistry::~HomeModulesCardRegistry() = default;

// static
void HomeModulesCardRegistry::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_IOS)
  // Local state prefs are used for the `AppBundleEphemeralModule` because this
  // promo relates to app installations on the device level, meaning impressions
  // should be tracked per-device rather than per profile.
  registry->RegisterIntegerPref(
      kAppBundlePromoEphemeralModuleImpressionCounterPref, 0);
#endif
}

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
  registry->RegisterIntegerPref(
      kDefaultBrowserPromoEphemeralModuleImpressionCounterPref, 0);
#endif

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kAuxiliarySearchPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kAuxiliarySearchPromoInteractedPref, false);
  registry->RegisterIntegerPref(kDefaultBrowserPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kDefaultBrowserPromoInteractedPref, false);
  registry->RegisterIntegerPref(kTabGroupPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kTabGroupPromoInteractedPref, false);
  registry->RegisterIntegerPref(kTabGroupSyncPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kTabGroupSyncPromoInteractedPref, false);
  registry->RegisterIntegerPref(kQuickDeletePromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kQuickDeletePromoInteractedPref, false);
  registry->RegisterIntegerPref(kHistorySyncPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kHistorySyncPromoInteractedPref, false);
  registry->RegisterIntegerPref(kTipsNotificationsPromoImpressionCounterPref,
                                0);
  registry->RegisterBooleanPref(kTipsNotificationsPromoInteractedPref, false);
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
#endif

#if BUILDFLAG(IS_ANDROID)
  if (strcmp(card_name, kDefaultBrowserPromo) == 0) {
    int freshness_impression_count =
        profile_prefs_->GetInteger(kDefaultBrowserPromoImpressionCounterPref);
    profile_prefs_->SetInteger(kDefaultBrowserPromoImpressionCounterPref,
                               freshness_impression_count + 1);
  } else if (ShouldNotifyCardShownPerSession(card_name)) {
    // Educational tip cards, except for the default browser promo card, will
    // send a notification when the card is shown once per session, rather than
    // every time it is displayed.
    if (strcmp(card_name, kTabGroupPromo) == 0) {
      int freshness_impression_count =
          profile_prefs_->GetInteger(kTabGroupPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kTabGroupPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    } else if (strcmp(card_name, kTabGroupSyncPromo) == 0) {
      int freshness_impression_count =
          profile_prefs_->GetInteger(kTabGroupSyncPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kTabGroupSyncPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    } else if (strcmp(card_name, kQuickDeletePromo) == 0) {
      int freshness_impression_count =
          profile_prefs_->GetInteger(kQuickDeletePromoImpressionCounterPref);
      profile_prefs_->SetInteger(kQuickDeletePromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    } else if (strcmp(card_name, kAuxiliarySearch) == 0) {
      int freshness_impression_count = profile_prefs_->GetInteger(
          kAuxiliarySearchPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kAuxiliarySearchPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    } else if (strcmp(card_name, kHistorySyncPromo) == 0) {
      int freshness_impression_count =
          profile_prefs_->GetInteger(kHistorySyncPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kHistorySyncPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    } else if (strcmp(card_name, kTipsNotificationsPromo) == 0) {
      int freshness_impression_count = profile_prefs_->GetInteger(
          kTipsNotificationsPromoImpressionCounterPref);
      profile_prefs_->SetInteger(kTipsNotificationsPromoImpressionCounterPref,
                                 freshness_impression_count + 1);
    }
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool HomeModulesCardRegistry::ShouldNotifyCardShownPerSession(
    const std::string& card_name) {
  if (shown_in_current_session_.find(card_name) !=
      shown_in_current_session_.end()) {
    return false;
  }

  shown_in_current_session_.insert(card_name);
  return true;
}
#endif

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

#if BUILDFLAG(IS_ANDROID)
  if (strcmp(card_name, kDefaultBrowserPromo) == 0) {
    profile_prefs_->SetBoolean(kDefaultBrowserPromoInteractedPref, true);
  } else if (strcmp(card_name, kTabGroupPromo) == 0) {
    profile_prefs_->SetBoolean(kTabGroupPromoInteractedPref, true);
  } else if (strcmp(card_name, kTabGroupSyncPromo) == 0) {
    profile_prefs_->SetBoolean(kTabGroupSyncPromoInteractedPref, true);
  } else if (strcmp(card_name, kQuickDeletePromo) == 0) {
    profile_prefs_->SetBoolean(kQuickDeletePromoInteractedPref, true);
  } else if (strcmp(card_name, kAuxiliarySearch) == 0) {
    profile_prefs_->SetBoolean(kAuxiliarySearchPromoInteractedPref, true);
  } else if (strcmp(card_name, kHistorySyncPromo) == 0) {
    profile_prefs_->SetBoolean(kHistorySyncPromoInteractedPref, true);
  } else if (strcmp(card_name, kTipsNotificationsPromo) == 0) {
    profile_prefs_->SetBoolean(kTipsNotificationsPromoInteractedPref, true);
  }
#endif
}

void HomeModulesCardRegistry::CreateAllCards() {
#if BUILDFLAG(IS_IOS)
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

  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTipsEphemeralCard)) {
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
        AddCardForTip(forced_identifier, all_cards_by_priority_,
                      profile_prefs_);
      }
    }

    // Iterate through variation labels and add unique cards.
    for (std::string_view variation_label :
         base::SplitString(features::TipsExperimentTrainEnabled(), ",",
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
#endif

#if BUILDFLAG(IS_ANDROID)
  int auxiliary_search_promo_count =
      profile_prefs_->GetInteger(kAuxiliarySearchPromoImpressionCounterPref);
  if (AuxiliarySearchPromo::IsEnabled(auxiliary_search_promo_count)) {
    all_cards_by_priority_.push_back(std::make_unique<AuxiliarySearchPromo>());
  }

  // TODO(crbug.com/420897397): Move the forced card check out from each card.
  int default_browser_promo_count =
      profile_prefs_->GetInteger(kDefaultBrowserPromoImpressionCounterPref);
  if (DefaultBrowserPromo::IsEnabled(
          default_browser_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<DefaultBrowserPromo>(profile_prefs_));
  }
  int history_sync_educational_promo_show_count =
      profile_prefs_->GetInteger(kHistorySyncPromoImpressionCounterPref);
  if (HistorySyncPromo::IsEnabled(history_sync_educational_promo_show_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<HistorySyncPromo>(profile_prefs_));
  }
  int tab_group_promo_count =
      profile_prefs_->GetInteger(kTabGroupPromoImpressionCounterPref);
  if (TabGroupPromo::IsEnabled(tab_group_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TabGroupPromo>(profile_prefs_));
  }

  int tab_group_sync_promo_count =
      profile_prefs_->GetInteger(kTabGroupSyncPromoImpressionCounterPref);
  if (TabGroupSyncPromo::IsEnabled(tab_group_sync_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TabGroupSyncPromo>(profile_prefs_));
  }

  int quick_delete_promo_count =
      profile_prefs_->GetInteger(kQuickDeletePromoImpressionCounterPref);
  if (QuickDeletePromo::IsEnabled(quick_delete_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<QuickDeletePromo>(profile_prefs_));
  }

  int tips_notifications_promo_show_count =
      profile_prefs_->GetInteger(kTipsNotificationsPromoImpressionCounterPref);
  if (TipsNotificationsPromo::IsEnabled(tips_notifications_promo_show_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<TipsNotificationsPromo>(profile_prefs_));
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
