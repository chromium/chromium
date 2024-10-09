// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"
#include "components/segmentation_platform/public/features.h"
#if BUILDFLAG(IS_IOS)
#include "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module.h"
#endif
#include "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"

namespace segmentation_platform::home_modules {

namespace {

#if BUILDFLAG(IS_IOS)
// Immpression counter for each card.
const char kPriceTrackingPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.price_tracking_promo_counter";
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
#endif
}

void HomeModulesCardRegistry::NotifyCardShown(const char* card_name) {
#if BUILDFLAG(IS_IOS)
  if (strcmp(card_name, kPriceTrackingNotificationPromo) == 0) {
    int freshness_impression_count =
        profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
    profile_prefs_->SetInteger(kPriceTrackingPromoImpressionCounterPref,
                               freshness_impression_count + 1);
  } else if (strcmp(card_name, kTipsEphemeralModule) == 0) {
    // TODO(crbug.com/372415791): Implement `NotifyCardShown()` for individual
    // tips once `TipsEphemeralModule` is broken up into multiple
    // `CardSelectionInfo`.
  }
#endif
}

void HomeModulesCardRegistry::CreateAllCards() {
#if BUILDFLAG(IS_IOS)
  int price_tracking_promo_count =
      profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
  if (PriceTrackingNotificationPromo::IsEnabled(price_tracking_promo_count)) {
    all_cards_by_priority_.push_back(
        std::make_unique<PriceTrackingNotificationPromo>(
            price_tracking_promo_count));
  }
  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTipsEphemeralCard)) {
    all_cards_by_priority_.push_back(std::make_unique<TipsEphemeralModule>());
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
