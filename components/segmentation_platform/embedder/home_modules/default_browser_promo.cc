// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

DefaultBrowserPromo::DefaultBrowserPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kDefaultBrowserPromo), profile_prefs_(profile_prefs) {}

std::map<SignalKey, FeatureQuery> DefaultBrowserPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kHasDefaultBrowserPromoShownInOtherSurface,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kHasDefaultBrowserPromoShownInOtherSurface})},
      {kShouldShowNonRoleManagerDefaultBrowserPromo,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kShouldShowNonRoleManagerDefaultBrowserPromo})}};
  return map;
}

CardSelectionInfo::ShowResult DefaultBrowserPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  CardSelectionInfo::ShowResult result;
  result.result_label = kDefaultBrowserPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kDefaultBrowserPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> resultForShouldShowNonRoleManagerDefaultBrowserPromo =
      signals.GetSignal(kShouldShowNonRoleManagerDefaultBrowserPromo);
  std::optional<float> resultForHasDefaultBrowserPromoShownInOtherSurface =
      signals.GetSignal(kHasDefaultBrowserPromoShownInOtherSurface);

  if (!resultForShouldShowNonRoleManagerDefaultBrowserPromo.has_value() ||
      !resultForHasDefaultBrowserPromoShownInOtherSurface.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (*resultForShouldShowNonRoleManagerDefaultBrowserPromo &&
      !*resultForHasDefaultBrowserPromoShownInOtherSurface) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool DefaultBrowserPromo::IsEnabled(int impression_count) {
  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule)) {
    return false;
  }

  if (impression_count >= features::kMaxDefaultBrowserCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
