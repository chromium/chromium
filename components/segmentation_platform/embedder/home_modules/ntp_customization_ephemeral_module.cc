// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_customization_ephemeral_module.h"

#include <optional>

#include "base/containers/fixed_flat_set.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform::home_modules {

namespace {

// Impression counter for the NTP customization ephemeral module.
const char kNTPCustomizationEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.ntp_customization_ephemeral_module_counter";

// Interaction counter for the NTP customization ephemeral module.
const char kNTPCustomizationEphemeralModuleInteractedPref[] =
    "ephemeral_pref_interacted.ntp_customization_ephemeral_module_interacted";

// Defines the signals that must all evaluate to true for
// `NTPCustomizationEphemeralModule` to be shown.
constexpr auto kRequiredSignals = base::MakeFixedFlatSet<std::string_view>({
    segmentation_platform::kNeverCustomizeNTP,
});

// Defines the signals that, if any are present and evaluate to true, will
// prevent `NTPCustomizationEphemeralModule` from being shown.
constexpr auto kDisqualifyingSignals =
    base::MakeFixedFlatSet<std::string_view>({
        segmentation_platform::kIsNewUser,
    });

}  // namespace

// static
void NTPCustomizationEphemeralModule::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kNTPCustomizationEphemeralModuleImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kNTPCustomizationEphemeralModuleInteractedPref,
                                false);
}

// static
bool NTPCustomizationEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kNTPCustomizationEphemeralModule;
}

// static
bool NTPCustomizationEphemeralModule::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      NTPCustomizationEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  int impression_count = profile_prefs->GetInteger(
      kNTPCustomizationEphemeralModuleImpressionCounterPref);

  return impression_count < kTipsEphemeralCardModuleMaxImpressionCount;
}

void NTPCustomizationEphemeralModule::OnShow(PrefService* profile_prefs,
                                             PrefService* local_state) {
  int freshness_impression_count = profile_prefs->GetInteger(
      kNTPCustomizationEphemeralModuleImpressionCounterPref);

  profile_prefs->SetInteger(
      kNTPCustomizationEphemeralModuleImpressionCounterPref,
      freshness_impression_count + 1);
}

void NTPCustomizationEphemeralModule::OnInteract(PrefService* profile_prefs,
                                                 PrefService* local_state) {
  profile_prefs->SetBoolean(kNTPCustomizationEphemeralModuleInteractedPref,
                            true);
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery> NTPCustomizationEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kIsNewUser,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsNewUser)},
      {segmentation_platform::kNeverCustomizeNTP,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kNeverCustomizeNTP)},
  };
}

CardSelectionInfo::ShowResult
NTPCustomizationEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      NTPCustomizationEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value();
  }

  bool has_been_interacted_with = profile_prefs_->GetBoolean(
      kNTPCustomizationEphemeralModuleInteractedPref);

  if (has_been_interacted_with) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  // Checks if all the required signals are present and have a positive value in
  // the provided `signals`.
  for (const auto& signal : kRequiredSignals) {
    std::optional<float> result = signals.GetSignal(std::string(signal));

    if (!result.has_value() || result.value() <= 0) {
      return ShowResult(EphemeralHomeModuleRank::kNotShown);
    }
  }

  // Checks if any of the disqualifying signals are present and have a positive
  // value in the provided `signals`.
  for (const auto& signal : kDisqualifyingSignals) {
    std::optional<float> result = signals.GetSignal(std::string(signal));

    if (result.has_value() && result.value() > 0) {
      return ShowResult(EphemeralHomeModuleRank::kNotShown);
    }
  }

  return ShowResult(EphemeralHomeModuleRank::kTop,
                    kNTPCustomizationEphemeralModule);
}

}  // namespace segmentation_platform::home_modules
