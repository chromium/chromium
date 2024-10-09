// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module.h"

#import <algorithm>
#import <optional>
#import <string_view>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/string_split.h"
#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/segmentation_platform/internal/database/signal_key.h"
#import "components/segmentation_platform/internal/metadata/feature_query.h"
#import "components/segmentation_platform/internal/metadata/metadata_writer.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

namespace {

// Defines the signals that must all evaluate to true for each `TipIdentifier`
// in order for the corresponding tip to be shown.
constexpr std::array<std::pair<TipIdentifier, const char*>, 8>
    kTipRequiredSignals = {
        std::make_pair(TipIdentifier::kAddressBarPosition,
                       segmentation_platform::tips_manager::signals::
                           kAddressBarPositionChoiceScreenDisplayed),
        std::make_pair(TipIdentifier::kAutofillPasswords,
                       segmentation_platform::tips_manager::signals::
                           kUsedPasswordAutofill),
        std::make_pair(TipIdentifier::kEnhancedSafeBrowsing,
                       segmentation_platform::kHasEnhancedSafeBrowsing),
        std::make_pair(TipIdentifier::kLensSearch,
                       segmentation_platform::tips_manager::signals::kLensUsed),
        std::make_pair(TipIdentifier::kLensShop,
                       segmentation_platform::tips_manager::signals::
                           kOpenedShoppingWebsite),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::tips_manager::signals::
                           kOpenedWebsiteInAnotherLanguage),
        std::make_pair(TipIdentifier::kLensTranslate,
                       segmentation_platform::tips_manager::signals::
                           kUsedGoogleTranslation),
        std::make_pair(
            TipIdentifier::kSavePasswords,
            segmentation_platform::tips_manager::signals::kSavedPasswords),
};

// Checks if a Tip variation is being forced via a Finch feature param.
//
// If a variation is forced to be shown, returns a
// `CardSelectionInfo::ShowResult` with the forced variation and a top rank.
//
// If a variation is forced to be hidden, returns a
// `CardSelectionInfo::ShowResult` with a "not shown" rank.
//
// Otherwise, returns `std::nullopt`.
std::optional<CardSelectionInfo::ShowResult> GetForcedTipVariation() {
  std::string forced_variation = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceShowCardParam, "");

  if (TipsEphemeralModule::IsModuleLabel(forced_variation)) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kTop,
                                         forced_variation);
  }

  // Check if a variation is forced to be hidden.
  forced_variation = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceHideCardParam, "");

  if (TipsEphemeralModule::IsModuleLabel(forced_variation)) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  return std::nullopt;
}

// Checks if all the required signals for a given `identifier` are present
// and have a positive value in the provided `signals`.
bool CanShowTip(TipIdentifier identifier, const CardSelectionSignals& signals) {
  bool found = false;

  for (const auto& tip_signal : kTipRequiredSignals) {
    if (tip_signal.first == identifier) {
      found = true;

      std::optional<float> result =
          signals.GetSignal(std::string(tip_signal.second));

      if (!result.has_value() || result.value() <= 0) {
        return false;
      }
    }
  }

  return found;
}

}  // namespace

// static
bool TipsEphemeralModule::IsModuleLabel(std::string_view label) {
  return kTipsOutputLabels.contains(label) || (label == kTipsEphemeralModule);
}

std::vector<std::string> TipsEphemeralModule::OutputLabels() {
  return std::vector<std::string>(kTipsOutputLabels.begin(),
                                  kTipsOutputLabels.end());
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery> TipsEphemeralModule::GetInputs() {
  // Use a lambda to reduce code duplication when creating `FeatureQuery`
  // objects.
  auto create_query = [](const char* signal_name) {
    return FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
        .tensor_length = 1,
        .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
        .name = signal_name});
  };

  return {
      {segmentation_platform::tips_manager::signals::
           kAddressBarPositionChoiceScreenDisplayed,
       create_query(segmentation_platform::tips_manager::signals::
                        kAddressBarPositionChoiceScreenDisplayed)},
      {segmentation_platform::tips_manager::signals::kLensUsed,
       create_query(segmentation_platform::tips_manager::signals::kLensUsed)},
      {segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite,
       create_query(segmentation_platform::tips_manager::signals::
                        kOpenedShoppingWebsite)},
      {segmentation_platform::tips_manager::signals::
           kOpenedWebsiteInAnotherLanguage,
       create_query(segmentation_platform::tips_manager::signals::
                        kOpenedWebsiteInAnotherLanguage)},
      {segmentation_platform::tips_manager::signals::kSavedPasswords,
       create_query(
           segmentation_platform::tips_manager::signals::kSavedPasswords)},
      {segmentation_platform::tips_manager::signals::kUsedGoogleTranslation,
       create_query(segmentation_platform::tips_manager::signals::
                        kUsedGoogleTranslation)},
      {segmentation_platform::tips_manager::signals::kUsedPasswordAutofill,
       create_query(segmentation_platform::tips_manager::signals::
                        kUsedPasswordAutofill)},
      {segmentation_platform::kHasEnhancedSafeBrowsing,
       create_query(segmentation_platform::kHasEnhancedSafeBrowsing)}};
}

CardSelectionInfo::ShowResult TipsEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check if a specific tip variation is forced to be shown via Finch flags.
  std::optional<ShowResult> forced_tip = GetForcedTipVariation();

  if (forced_tip.has_value()) {
    return forced_tip.value();
  }

  // If no tip is forced, check if any tip is eligible to be shown based on the
  // available signals and the enabled experiment variations.
  //
  // TODO(crbug.com/372415791): Refactor `TipsEphemeralModule` to use
  // individual `CardSelectionInfo` for each tip. This will involve moving the
  // `TipsExperimentTrainEnabled()` check into `HomeModulesCardRegistry`.
  std::string enabled_variations = features::TipsExperimentTrainEnabled();

  // Iterates the variation labels without extra allocations.
  for (std::string_view variation_label :
       base::SplitString(enabled_variations, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    TipIdentifier identifier = TipIdentifierForOutputLabel(variation_label);

    if (identifier == TipIdentifier::kUnknown ||
        !CanShowTip(identifier, signals)) {
      continue;
    }

    return ShowResult(EphemeralHomeModuleRank::kTop,
                      std::string(variation_label));
  }

  return ShowResult(EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
