// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_theme_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// Impression counter for the NTP Theme promo ephemeral module.
const char kNtpThemePromoImpressionCounterPref[] =
    "ephemeral_pref_counter.ntp_theme_promo_counter";

// Interaction counter for the NTP Theme promo ephemeral module.
const char kNtpThemePromoInteractedPref[] =
    "ephemeral_pref_interacted.ntp_theme_promo_interacted";

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// TODO(crbug.com/382803396): The enum id of the ntp theme promo card. Could
// be referenced after refactor.
const int kNtpThemePromoId = 19;

}  // namespace

namespace segmentation_platform::home_modules {

NtpThemePromo::NtpThemePromo(PrefService* profile_prefs)
    : CardSelectionInfo(kNtpThemePromo), profile_prefs_(profile_prefs) {}

// static
void NtpThemePromo::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kNtpThemePromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kNtpThemePromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> NtpThemePromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map;

  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                /* enum_id= */ nullptr, /* enum_size= */ 0,
                                /* days= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times ntp theme promo card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfNtpThemePromoShownTimes,
                                kEducationalTipModuleHistogramName,
                                &kNtpThemePromoId, /* enum_size= */ 1,
                                /* days= */ KDaysToShowEachEphemeralCardOnce);
  map.emplace(kNtpThemePromoShownCount,
              std::move(countOfNtpThemePromoShownTimes));

  map.emplace(kSupportCustomizedNtpTheme,
              FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
                  .tensor_length = 1,
                  .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
                  .name = kSupportCustomizedNtpTheme}));

  return map;
}

CardSelectionInfo::ShowResult NtpThemePromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kNtpThemePromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kNtpThemePromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kNtpThemePromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_ntp_theme_promo_shown_count =
      signals.GetSignal(kNtpThemePromoShownCount);
  std::optional<float>
      result_for_educational_tip_shown_count_for_ntp_theme_signal =
          signals.GetSignal(kEducationalTipShownCount);
  std::optional<float> result_for_support_customized_ntp_theme =
      signals.GetSignal(kSupportCustomizedNtpTheme);

  if (!result_for_ntp_theme_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count_for_ntp_theme_signal
           .has_value() ||
      !result_for_support_customized_ntp_theme.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  // Show the promo card if the promo card has not been shown more than 1 times
  // in 7 days.
  if (*result_for_support_customized_ntp_theme &&
      result_for_ntp_theme_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_ntp_theme_signal.value() < 1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool NtpThemePromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kNtpThemePromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kNewTabPageCustomizationV2)) {
    return false;
  }

  if (!segmentation_platform::features::kNewTabPageCustomizationV2ShowPromo
           .Get()) {
    return false;
  }

  if (profile_prefs->IsManagedPreference("ntp.custom_background_dict2")) {
    return false;
  }

  int impression_count =
      profile_prefs->GetInteger(kNtpThemePromoImpressionCounterPref);

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

void NtpThemePromo::OnShow(PrefService* profile_prefs,
                           PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kNtpThemePromoImpressionCounterPref);

  profile_prefs->SetInteger(kNtpThemePromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void NtpThemePromo::OnInteract(PrefService* profile_prefs,
                               PrefService* local_state) {
  profile_prefs->SetBoolean(kNtpThemePromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
