// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"

namespace segmentation_platform::home_modules {

namespace {

#if BUILDFLAG(IS_IOS)
// The maximum number of times a card can be visible to the user.
const int kMaxPriceTrackingNotificationCardImpressions = 3;

// Immpression counter for each card.
const char kPriceTrackingPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.price_tracking_promo_counter";
#endif

}  // namespace

HomeModulesCardRegistry::HomeModulesCardRegistry(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  CreateAllCards();

  size_t input_counter = 0;
  size_t label_counter = 0;
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
      all_output_labels_.insert(all_output_labels_.end(), card_labels.begin(),
                                card_labels.end());
      for (const auto& label : card_labels) {
        label_to_output_index_[label] = label_counter;
        label_counter++;
      }
    } else {
      all_output_labels_.push_back(card->card_name());
      label_to_output_index_[card->card_name()] = label_counter;
      label_counter++;
    }
  }
  all_cards_input_size_ = input_counter;
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
  }
#endif
}

void HomeModulesCardRegistry::CreateAllCards() {
#if BUILDFLAG(IS_IOS)
  int price_tracking_promo_count =
      profile_prefs_->GetInteger(kPriceTrackingPromoImpressionCounterPref);
  if (base::FeatureList::IsEnabled(commerce::kPriceTrackingPromo) &&
      price_tracking_promo_count <
          kMaxPriceTrackingNotificationCardImpressions) {
    all_cards_by_priority_.push_back(
        std::make_unique<PriceTrackingNotificationPromo>());
  }
#else
  // Add all cards
  label_to_output_index_["label"] = 0;
  all_output_labels_ = {"label"};
#endif
}

base::WeakPtr<HomeModulesCardRegistry> HomeModulesCardRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace segmentation_platform::home_modules
