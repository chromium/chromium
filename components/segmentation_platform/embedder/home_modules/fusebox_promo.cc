// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/fusebox_promo.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/stack_allocated.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// Impression counter for the Fusebox promo ephemeral module.
constexpr char kFuseboxPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.fusebox_promo_counter";

// Interaction flag for the Fusebox promo ephemeral module.
constexpr char kFuseboxPromoInteractedPref[] =
    "ephemeral_pref_interacted.fusebox_promo_interacted";

constexpr char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// Corresponds to ModuleDelegate.java ModuleType.FUSEBOX_PROMO = 21.
constexpr int32_t kFuseboxPromoId = 21;

}  // namespace

namespace segmentation_platform::home_modules {

FuseboxPromo::FuseboxPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kFuseboxPromo), profile_prefs_(profile_prefs) {}

// static
void FuseboxPromo::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kFuseboxPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kFuseboxPromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> FuseboxPromo::GetInputs() {
  STACK_ALLOCATED_IGNORE("https://crbug.com/350256130")
  std::map<SignalKey, FeatureQuery> map;

  // Define signal for number of times any Magic Stack module has shown to the
  // user in limited days (shared across educational tip cards via
  // kEducationalTipShownCount).
  DEFINE_UMA_FEATURE_ENUM_COUNT(
      countOfEducationalTipCardShownTimes, kEducationalTipModuleHistogramName,
      /* enum_ids= */ nullptr,
      /* enum_ids_size= */ 0,
      /* bucket_count= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times the Fusebox promo card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(
      countOfFuseboxPromoShownTimes, kEducationalTipModuleHistogramName,
      &kFuseboxPromoId, /* enum_ids_size= */ 1,
      /* bucket_count= */ KDaysToShowEachEphemeralCardOnce);
  map.emplace(kFuseboxPromoShownCount,
              std::move(countOfFuseboxPromoShownTimes));

  // Define signal for whether Fusebox is currently eligible for the user.
  map.emplace(kIsFuseboxEligible,
              FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
                  .tensor_length = 1,
                  .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
                  .name = kIsFuseboxEligible}));

  return map;
}

CardSelectionInfo::ShowResult FuseboxPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kFuseboxPromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kFuseboxPromo;

  if (profile_prefs_->GetBoolean(kFuseboxPromoInteractedPref) ||
      profile_prefs_->GetBoolean(omnibox::kFuseboxAimQuerySubmitted)) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_fusebox_promo_shown_count =
      signals.GetSignal(kFuseboxPromoShownCount);
  std::optional<float> result_for_educational_tip_shown_count =
      signals.GetSignal(kEducationalTipShownCount);
  std::optional<float> result_for_is_fusebox_eligible =
      signals.GetSignal(kIsFuseboxEligible);

  if (!result_for_fusebox_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count.has_value() ||
      !result_for_is_fusebox_eligible.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  // Show the promo card if Fusebox is eligible, the user has never submitted an
  // AI Mode query, and the card has not been shown within the frequency limits.
  if (result_for_is_fusebox_eligible.value() > 0 &&
      result_for_fusebox_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count.value() < 1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool FuseboxPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kFuseboxPromo == forced_result.value().result_label.value()) {
    return forced_result.value().position != EphemeralHomeModuleRank::kNotShown;
  }

  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxFuseboxUserEdGuidedTour)) {
    return false;
  }

  if (profile_prefs->GetBoolean(kFuseboxPromoInteractedPref) ||
      profile_prefs->GetBoolean(omnibox::kFuseboxAimQuerySubmitted)) {
    return false;
  }

  int impression_count =
      profile_prefs->GetInteger(kFuseboxPromoImpressionCounterPref);

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

void FuseboxPromo::OnShow(PrefService* profile_prefs,
                          PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kFuseboxPromoImpressionCounterPref);

  profile_prefs->SetInteger(kFuseboxPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void FuseboxPromo::OnInteract(PrefService* profile_prefs,
                              PrefService* local_state) {
  profile_prefs->SetBoolean(kFuseboxPromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
