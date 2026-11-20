// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// Impression counter for the Tips Notifications promo ephemeral module.
const char kTipsNotificationsPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tips_notifications_opt_in_promo_counter";

// Interaction counter for the Tips Notifications promo ephemeral module.
const char kTipsNotificationsPromoInteractedPref[] =
    "ephemeral_pref_interacted.tips_notifications_opt_in_promo_interacted";

constexpr char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// Enum id of ModuleType.TIPS_NOTIFICATIONS_PROMO.
constexpr int kTipsNotificationsPromoId = 20;

}  // namespace

namespace segmentation_platform::home_modules {

TipsNotificationsPromo::TipsNotificationsPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kTipsNotificationsPromo),
      profile_prefs_(profile_prefs) {}

// static
void TipsNotificationsPromo::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTipsNotificationsPromoImpressionCounterPref,
                                0);
  registry->RegisterBooleanPref(kTipsNotificationsPromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> TipsNotificationsPromo::GetInputs() {
  STACK_ALLOCATED_IGNORE("https://crbug.com/350256130")
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

// When not forced to be shown by flags and has less than 10 impressions, the
// tips notifications promo will be shown to users if:
// 1. The user has not already interacted with the card.
// 2. The user is eligible for the tips opt-in (OS app notifications enabled,
//    Tips notifications not enabled, and sheet opt-in not accepted; users who
//    declined the sheet stay eligible so the card remains a passive way back).
CardSelectionInfo::ShowResult TipsNotificationsPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kTipsNotificationsPromo == forced_result.value().result_label.value()) {
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

  if (result_for_is_eligible_to_tips_opt_in.value() > 0 &&
      result_for_tips_notifications_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_tips_notifications_signal
              .value() < 1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

// static
bool TipsNotificationsPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kTipsNotificationsPromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kAndroidTipsNotifications)) {
    return false;
  }

  int impression_count =
      profile_prefs->GetInteger(kTipsNotificationsPromoImpressionCounterPref);

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

void TipsNotificationsPromo::OnShow(PrefService* profile_prefs,
                                    PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kTipsNotificationsPromoImpressionCounterPref);

  profile_prefs->SetInteger(kTipsNotificationsPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void TipsNotificationsPromo::OnInteract(PrefService* profile_prefs,
                                        PrefService* local_state) {
  profile_prefs->SetBoolean(kTipsNotificationsPromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
