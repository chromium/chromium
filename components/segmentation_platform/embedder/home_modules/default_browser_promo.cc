// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// The maximum number of times the default browser promo card can be visible to
// the user.
const int kMaxDefaultBrowserCardImpressions = 3;

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// TODO(crbug.com/382803396): The enum id of the default browser promo card.
// Could be referenced after refactor.
const int kDefaultBrowserPromoId = 6;

}  // namespace

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
      {kIsUserSignedIn,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsUserSignedIn})},
      {kShouldShowNonRoleManagerDefaultBrowserPromo,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kShouldShowNonRoleManagerDefaultBrowserPromo})}};

  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                /* enum_id= */ nullptr, /* enum_size= */ 0,
                                /* days= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times default browser promo card has shown to
  // the user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfDefaultBrowserPromoShownTimes,
                                kEducationalTipModuleHistogramName,
                                &kDefaultBrowserPromoId, /* enum_size= */ 1,
                                /* days= */ KDaysToShowEachEphemeralCardOnce);
  map.emplace(kDefaultBrowserPromoShownCount,
              std::move(countOfDefaultBrowserPromoShownTimes));

  return map;
}

CardSelectionInfo::ShowResult DefaultBrowserPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kDefaultBrowserPromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kDefaultBrowserPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kDefaultBrowserPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_is_user_signed_in =
      signals.GetSignal(kIsUserSignedIn);
  std::optional<float>
      result_for_should_show_non_role_manager_default_browser_promo =
          signals.GetSignal(kShouldShowNonRoleManagerDefaultBrowserPromo);
  std::optional<float>
      result_for_has_default_browser_promo_shown_in_other_surface =
          signals.GetSignal(kHasDefaultBrowserPromoShownInOtherSurface);
  std::optional<float> result_for_default_browser_promo_shown_count =
      signals.GetSignal(kDefaultBrowserPromoShownCount);
  std::optional<float>
      result_for_educational_tip_shown_count_for_default_browser_signal =
          signals.GetSignal(kEducationalTipShownCount);

  if (!result_for_is_user_signed_in.has_value() ||
      !result_for_should_show_non_role_manager_default_browser_promo
           .has_value() ||
      !result_for_has_default_browser_promo_shown_in_other_surface
           .has_value() ||
      !result_for_default_browser_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count_for_default_browser_signal
           .has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (*result_for_is_user_signed_in &&
      *result_for_should_show_non_role_manager_default_browser_promo &&
      !*result_for_has_default_browser_promo_shown_in_other_surface &&
      result_for_default_browser_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_default_browser_signal
              .value() < 1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool DefaultBrowserPromo::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kDefaultBrowserPromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule)) {
    return false;
  }

  if (impression_count >= kMaxDefaultBrowserCardImpressions) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
