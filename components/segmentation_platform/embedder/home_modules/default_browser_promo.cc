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

DefaultBrowserPromo::DefaultBrowserPromo()
    : CardSelectionInfo(kDefaultBrowserPromo) {}

std::map<SignalKey, FeatureQuery> DefaultBrowserPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kHasDefaultBrowserPromoReachedLimitInRoleManager,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kHasDefaultBrowserPromoReachedLimitInRoleManager})},
      {kIsDefaultBrowserChrome,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsDefaultBrowserChrome})}};
  return map;
}

CardSelectionInfo::ShowResult DefaultBrowserPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  CardSelectionInfo::ShowResult result;
  result.result_label = kDefaultBrowserPromo;

  std::optional<float> resultForIsDefaultBrowserChrome =
      signals.GetSignal(kIsDefaultBrowserChrome);
  std::optional<float>
      resultForHasDefaultBrowserPromoReachedLimitInRoleManager =
          signals.GetSignal(kHasDefaultBrowserPromoReachedLimitInRoleManager);

  if (!resultForIsDefaultBrowserChrome.has_value() ||
      !resultForHasDefaultBrowserPromoReachedLimitInRoleManager.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (!*resultForIsDefaultBrowserChrome &&
      *resultForHasDefaultBrowserPromoReachedLimitInRoleManager) {
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
