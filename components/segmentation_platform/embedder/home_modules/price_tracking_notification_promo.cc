// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// The maximum number of times a card can be visible to the user.
const int kMaxPriceTrackingNotificationCardImpressions = 3;

}  // namespace

namespace segmentation_platform {

namespace home_modules {

const char kHasSubscriptionSignalKey[] = "has_subscription";
const char kIsNewUserSignalKey[] = "is_new_user";
const char kIsSyncedSignalKey[] = "is_sycned";

PriceTrackingNotificationPromo::PriceTrackingNotificationPromo(
    int price_tracking_promo_count)
    : CardSelectionInfo(kPriceTrackingNotificationPromo) {}

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
  CardSelectionInfo::ShowResult result;
  result.result_label = kPriceTrackingNotificationPromo;
  if (base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceShowCardParam,
          "") == features::kPriceTrackingPromoForceOverride) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  } else if (base::GetFieldTrialParamByFeatureAsString(
                 features::kSegmentationPlatformEphemeralCardRanker,
                 features::kEphemeralCardRankerForceHideCardParam,
                 "") == features::kPriceTrackingPromoForceOverride) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }
  if (*signals.GetSignal(kHasSubscriptionSignalKey) &&
      !*signals.GetSignal(kIsNewUserSignalKey) &&
      *signals.GetSignal(kIsSyncedSignalKey)) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool PriceTrackingNotificationPromo::IsEnabled(int impression_count) {
  if (!base::FeatureList::IsEnabled(commerce::kPriceTrackingPromo)) {
    return false;
  }
  // Mark that the card shouldn't be shown if:
  // 1) the force hide feature param is set.
  // 2) the card has reached its max impression count and the force show
  // feature maram is not set.
  if (base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceHideCardParam,
          "") == features::kPriceTrackingPromoForceOverride) {
    return false;
  }
  if (impression_count > kMaxPriceTrackingNotificationCardImpressions &&
      base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceShowCardParam,
          "") != features::kPriceTrackingPromoForceOverride) {
    return false;
  }

  return true;
}

}  // namespace home_modules
}  // namespace segmentation_platform
