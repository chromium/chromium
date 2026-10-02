// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_theme_ephemeral_module.h"

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

// Impression counter for the NTP theme ephemeral module.
const char kNTPThemeEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.ntp_theme_ephemeral_module_counter";

// Interaction counter for the NTP theme ephemeral module.
const char kNTPThemeEphemeralModuleInteractedPref[] =
    "ephemeral_pref_interacted.ntp_theme_ephemeral_module_interacted";

// Defines the signals that must all evaluate to true for
// `NTPThemeEphemeralModule` to be shown.
constexpr auto kRequiredSignals = base::MakeFixedFlatSet<std::string_view>({
    segmentation_platform::kLacksNTPBackground,
    segmentation_platform::kNTPBackgroundNotSelectedRecently,
});

// Defines the signals that, if any are present and evaluate to true, will
// prevent `NTPThemeEphemeralModule` from being shown.
constexpr auto kDisqualifyingSignals =
    base::MakeFixedFlatSet<std::string_view>({
        segmentation_platform::kIsNewUser,
    });

}  // namespace

// static
void NTPThemeEphemeralModule::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kNTPThemeEphemeralModuleImpressionCounterPref,
                                0);
  registry->RegisterBooleanPref(kNTPThemeEphemeralModuleInteractedPref, false);
}

// static
bool NTPThemeEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kNTPThemeEphemeralModule;
}

// static
bool NTPThemeEphemeralModule::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      NTPThemeEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  int impression_count =
      profile_prefs->GetInteger(kNTPThemeEphemeralModuleImpressionCounterPref);

  return impression_count < kTipsEphemeralCardModuleMaxImpressionCount;
}

void NTPThemeEphemeralModule::OnShow(PrefService* profile_prefs,
                                     PrefService* local_state) {
  int freshness_impression_count =
      profile_prefs->GetInteger(kNTPThemeEphemeralModuleImpressionCounterPref);

  profile_prefs->SetInteger(kNTPThemeEphemeralModuleImpressionCounterPref,
                            freshness_impression_count + 1);
}

void NTPThemeEphemeralModule::OnInteract(PrefService* profile_prefs,
                                         PrefService* local_state) {
  profile_prefs->SetBoolean(kNTPThemeEphemeralModuleInteractedPref, true);
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery> NTPThemeEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kIsNewUser,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsNewUser)},
      {segmentation_platform::kLacksNTPBackground,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kLacksNTPBackground)},
      {segmentation_platform::kNTPBackgroundNotSelectedRecently,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kNTPBackgroundNotSelectedRecently)},
  };
}

CardSelectionInfo::ShowResult NTPThemeEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      NTPThemeEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value();
  }

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kNTPThemeEphemeralModuleInteractedPref);

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

  return ShowResult(EphemeralHomeModuleRank::kTop, kNTPThemeEphemeralModule);
}

}  // namespace segmentation_platform::home_modules
