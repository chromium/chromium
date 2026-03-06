// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"

#include <optional>

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

namespace {

const char kAuxiliarySearchPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.auxiliary_search_promo_counter";
const char kAuxiliarySearchPromoInteractedPref[] =
    "ephemeral_pref_interacted.auxiliary_search_promo_interacted";

std::optional<CardSelectionInfo::ShowResult> GetForceShown() {
  if (features::kMaxAuxiliarySearchForceShow.Get()) {
    CardSelectionInfo::ShowResult result;
    result.result_label = kAuxiliarySearch;
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }
  return std::nullopt;
}

}  // namespace

AuxiliarySearchPromo::AuxiliarySearchPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kAuxiliarySearch), profile_prefs_(profile_prefs) {}

AuxiliarySearchPromo::~AuxiliarySearchPromo() = default;

// static
void AuxiliarySearchPromo::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAuxiliarySearchPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kAuxiliarySearchPromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> AuxiliarySearchPromo::GetInputs() {
  return {{segmentation_platform::kAuxiliarySearchAvailable,
           CreateFeatureQueryFromCustomInputName(
               segmentation_platform::kAuxiliarySearchAvailable)}};
}

CardSelectionInfo::ShowResult AuxiliarySearchPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result = GetForceShown();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kAuxiliarySearch == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kAuxiliarySearch;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kAuxiliarySearchPromoInteractedPref);

  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> auxiliary_search_available =
      signals.GetSignal(kAuxiliarySearchAvailable);

  if (!auxiliary_search_available.has_value() ||
      !auxiliary_search_available.value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
  } else {
    result.position = EphemeralHomeModuleRank::kTop;
  }

  return result;
}

// static
bool AuxiliarySearchPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result = GetForceShown();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kAuxiliarySearch == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kAndroidAppIntegrationModule)) {
    return false;
  }

  int impression_count =
      profile_prefs->GetInteger(kAuxiliarySearchPromoImpressionCounterPref);

  if (impression_count >= features::kMaxAuxiliarySearchCardImpressions.Get()) {
    return false;
  }

  return true;
}

void AuxiliarySearchPromo::OnShow(PrefService* profile_prefs,
                                  PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kAuxiliarySearchPromoImpressionCounterPref);

  profile_prefs->SetInteger(kAuxiliarySearchPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void AuxiliarySearchPromo::OnInteract(PrefService* profile_prefs,
                                      PrefService* local_state) {
  profile_prefs->SetBoolean(kAuxiliarySearchPromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
