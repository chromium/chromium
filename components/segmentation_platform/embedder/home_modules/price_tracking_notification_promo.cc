// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// Impression counter for the Price Tracking notification promo card.
const char kPriceTrackingPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.price_tracking_promo_counter";

// The maximum number of times a card can be visible to the user.
const int kMaxPriceTrackingNotificationCardImpressions = 3;

}  // namespace

namespace segmentation_platform {

namespace home_modules {

const char kHasSubscriptionSignalKey[] = "has_subscription";
const char kIsNewUserSignalKey[] = "is_new_user";
const char kIsSyncedSignalKey[] = "is_sycned";

PriceTrackingNotificationPromo::PriceTrackingNotificationPromo()
    : CardSelectionInfo(kPriceTrackingNotificationPromo) {}

// static
void PriceTrackingNotificationPromo::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kPriceTrackingPromoImpressionCounterPref, 0);
}

std::map<SignalKey, FeatureQuery> PriceTrackingNotificationPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kHasSubscriptionSignalKey,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_SHOPPING_SERVICE,
           .name = "TotalShoppingBookmarkCount"})},
      {kIsNewUserSignalKey,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsNewUser})},
      {kIsSyncedSignalKey,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsSynced})}};
  return map;
}

CardSelectionInfo::ShowResult PriceTrackingNotificationPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kPriceTrackingNotificationPromo ==
          forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kPriceTrackingNotificationPromo;

  if (*signals.GetSignal(kHasSubscriptionSignalKey) &&
      !*signals.GetSignal(kIsNewUserSignalKey) &&
      *signals.GetSignal(kIsSyncedSignalKey)) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

void PriceTrackingNotificationPromo::OnShow(PrefService* profile_prefs,
                                            PrefService* local_state) {
  int freshness_impression_count =
      profile_prefs->GetInteger(kPriceTrackingPromoImpressionCounterPref);

  profile_prefs->SetInteger(kPriceTrackingPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

bool PriceTrackingNotificationPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kPriceTrackingNotificationPromo ==
          forced_result.value().result_label.value()) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  int impression_count =
      profile_prefs->GetInteger(kPriceTrackingPromoImpressionCounterPref);

  if (impression_count < kMaxPriceTrackingNotificationCardImpressions) {
    return true;
  }

  return false;
}

}  // namespace home_modules
}  // namespace segmentation_platform
