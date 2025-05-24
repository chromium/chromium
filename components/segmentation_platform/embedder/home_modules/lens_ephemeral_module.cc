// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/lens_ephemeral_module.h"

#include <algorithm>
#include <optional>

#include "base/containers/fixed_flat_set.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

namespace {

// Defines the signals that, for a given `TipIdentifier`, must all evaluate
// to true for the corresponding `LensEphemeralModule` to be shown.
constexpr std::array<std::pair<TipIdentifier, const char*>, 6>
    kRequiredSignals = {
        std::make_pair(TipIdentifier::kLensSearch,
                       segmentation_platform::kLensAllowedByEnterprisePolicy),
        std::make_pair(TipIdentifier::kLensSearch,
                       segmentation_platform::kLensNotUsedRecently),
        std::make_pair(TipIdentifier::kLensShop,
                       segmentation_platform::kLensAllowedByEnterprisePolicy),
        std::make_pair(TipIdentifier::kLensShop,
                       segmentation_platform::kLensNotUsedRecently),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::kLensAllowedByEnterprisePolicy),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::kLensNotUsedRecently),
};

// Defines the signals where, for a given `TipIdentifier`, if any evaluate
// to true, AND all required signals for that `TipIdentifier` are also true,
// then the corresponding `LensEphemeralModule` will be shown.
// Optional signals do not take precedence over required signals.
constexpr std::array<std::pair<TipIdentifier, const char*>, 3>
    kSupplementalSignals = {
        std::make_pair(TipIdentifier::kLensShop,
                       segmentation_platform::tips_manager::signals::
                           kOpenedShoppingWebsite),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::tips_manager::signals::
                           kOpenedWebsiteInAnotherLanguage),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::tips_manager::signals::
                           kUsedGoogleTranslation),
};

// Defines the signals where, for a given `TipIdentifier`, if any are present
// and evaluate to true, will prevent the corresponding `LensEphemeralModule`
// from being shown.
constexpr std::array<std::pair<TipIdentifier, const char*>, 3>
    kDisqualifyingSignals = {
        std::make_pair(TipIdentifier::kLensSearch,
                       segmentation_platform::kIsNewUser),
        std::make_pair(TipIdentifier::kLensShop,
                       segmentation_platform::kIsNewUser),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::kIsNewUser),
};

// Define the priority order for Lens tip variations.
constexpr std::array<TipIdentifier, 3> kLensTipPriorityOrder = {
    TipIdentifier::kLensShop,
    TipIdentifier::kLensTranslate,
    TipIdentifier::kLensSearch,
};

// Checks if the given `identifier` has any supplemental signals, and if so,
// whether any of them are satisfied.
bool SatisfiesSupplementalSignals(TipIdentifier identifier,
                                  const CardSelectionSignals& signals) {
  bool has_supplemental_signal = false;
  bool supplemental_signal_met = false;

  for (const auto& supplemental_signal : kSupplementalSignals) {
    if (supplemental_signal.first == identifier) {
      has_supplemental_signal = true;

      std::optional<float> result =
          signals.GetSignal(std::string(supplemental_signal.second));

      if (result.has_value() && result.value() > 0) {
        supplemental_signal_met = true;
        // Early exit if a supplemental signal is met.
        break;
      }
    }
  }

  // If there are supplemental signals, return true only if one is met.
  // Otherwise, if there are no supplemental signals, return true.
  return !has_supplemental_signal || supplemental_signal_met;
}

}  // namespace

// static
bool LensEphemeralModule::IsModuleLabel(std::string_view label) {
  return kLensEphemeralModuleVariationLabels.contains(label) ||
         label == kLensEphemeralModule;
}

// static
bool LensEphemeralModule::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      LensEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  int max_impression_count =
      features::GetTipsEphemeralCardModuleMaxImpressionCount();

  return impression_count < max_impression_count;
}

std::vector<std::string> LensEphemeralModule::OutputLabels() {
  return std::vector<std::string>(kLensEphemeralModuleVariationLabels.begin(),
                                  kLensEphemeralModuleVariationLabels.end());
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery> LensEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kIsNewUser,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsNewUser)},
      {segmentation_platform::kLensNotUsedRecently,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kLensNotUsedRecently)},
      {segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::tips_manager::signals::
               kOpenedShoppingWebsite)},
      {segmentation_platform::tips_manager::signals::
           kOpenedWebsiteInAnotherLanguage,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::tips_manager::signals::
               kOpenedWebsiteInAnotherLanguage)},
      {segmentation_platform::tips_manager::signals::kUsedGoogleTranslation,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::tips_manager::signals::
               kUsedGoogleTranslation)},
      {segmentation_platform::kLensAllowedByEnterprisePolicy,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kLensAllowedByEnterprisePolicy)},
  };
}

CardSelectionInfo::ShowResult LensEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      LensEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value();
  }

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kLensEphemeralModuleInteractedPref);

  if (has_been_interacted_with) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  // Find all Lens tips that satisfy their required signals.
  std::vector<TipIdentifier> satisfied_tips;

  for (TipIdentifier tip : kLensTipPriorityOrder) {
    // Check if all required signals for the current `tip` are satisfied.
    bool required_signals_met = true;

    for (const auto& required_signal : kRequiredSignals) {
      if (required_signal.first == tip) {
        std::optional<float> result =
            signals.GetSignal(std::string(required_signal.second));

        if (!result.has_value() || result.value() <= 0) {
          // Fail fast if any required signal is missing or not satisfied.
          required_signals_met = false;
          break;
        }
      }
    }

    // Check if any disqualifying signals for the current `tip` are present.
    bool disqualifying_signal_present = false;

    for (const auto& disqualifying_signal : kDisqualifyingSignals) {
      if (disqualifying_signal.first == tip) {
        std::optional<float> result =
            signals.GetSignal(std::string(disqualifying_signal.second));

        if (result.has_value() && result.value() > 0) {
          // Fail fast if any disqualifying signal is present.
          disqualifying_signal_present = true;
          break;
        }
      }
    }

    if (required_signals_met && !disqualifying_signal_present) {
      satisfied_tips.push_back(tip);
    }
  }

  // If no tip satisfies the required signals, early exit.
  if (satisfied_tips.empty()) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  // Find the first tip that also satisfies its supplemental signals.
  for (TipIdentifier tip : satisfied_tips) {
    if (SatisfiesSupplementalSignals(tip, signals)) {
      std::optional<std::string_view> output_label =
          OutputLabelForTipIdentifier(tip);

      if (output_label.has_value()) {
        return ShowResult(EphemeralHomeModuleRank::kTop,
                          std::string(output_label.value()));
      }
    }
  }

  return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
