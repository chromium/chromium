// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

constexpr char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

constexpr int kTipsNotificationsPromoId = 11;

}  // namespace

namespace segmentation_platform::home_modules {

TipsNotificationsPromo::TipsNotificationsPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kTipsNotificationsPromo),
      profile_prefs_(profile_prefs) {}
std::map<SignalKey, FeatureQuery> TipsNotificationsPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kIsEligibleToTipsOptIn,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsEligibleToTipsOptIn})}};

  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                /* enum_id= */ nullptr, /* enum_size= */ 0,
                                /* days= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times tips notifications promo card has shown
  // to the user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfTipsNotificationsPromoShownTimes,
                                kEducationalTipModuleHistogramName,
                                &kTipsNotificationsPromoId, /* enum_size= */ 1,
                                /* days= */ KDaysToShowEachEphemeralCardOnce);
  map.emplace(kTipsNotificationsPromoShownCount,
              std::move(countOfTipsNotificationsPromoShownTimes));
  return map;
}

//  When not forced to be shown by flags and has less than 10 impressions, the
//  tips notifications promo will be shown to users if
//  1. The user has not already interacted with the card.
//  2. The use must be eligible for the tips opt in (notifications toggle off).
CardSelectionInfo::ShowResult TipsNotificationsPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();
  if (forced_result.has_value() &&
      kTipsNotificationsPromo ==
          forced_result.value().result_label.value_or("")) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kTipsNotificationsPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kTipsNotificationsPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_tips_notifications_promo_shown_count =
      signals.GetSignal(kTipsNotificationsPromoShownCount);
  std::optional<float> result_for_is_eligible_to_tips_opt_in =
      signals.GetSignal(kIsEligibleToTipsOptIn);
  std::optional<float>
      result_for_educational_tip_shown_count_for_tips_notifications_signal =
          signals.GetSignal(kEducationalTipShownCount);

  if (!result_for_is_eligible_to_tips_opt_in.has_value() ||
      !result_for_tips_notifications_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count_for_tips_notifications_signal
           .has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (result_for_is_eligible_to_tips_opt_in.value() < 1 &&
      result_for_tips_notifications_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_tips_notifications_signal
              .value() < 1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool TipsNotificationsPromo::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      kTipsNotificationsPromo ==
          forced_result.value().result_label.value_or("")) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule) ||
      !base::FeatureList::IsEnabled(features::kAndroidTipsNotifications)) {
    return false;
  }

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}
}  // namespace segmentation_platform::home_modules
