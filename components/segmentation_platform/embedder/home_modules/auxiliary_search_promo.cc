// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/auxiliary_search_promo.h"

#include <optional>

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
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

AuxiliarySearchPromo::AuxiliarySearchPromo()
    : CardSelectionInfo(kAuxiliarySearch) {}

AuxiliarySearchPromo::~AuxiliarySearchPromo() = default;

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

bool AuxiliarySearchPromo::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result = GetForceShown();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kAuxiliarySearch == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kAndroidAppIntegrationModule)) {
    return false;
  }

  if (impression_count >= features::kMaxAuxiliarySearchCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
