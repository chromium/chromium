// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/send_tab_notification_promo.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/send_tab_to_self/features.h"

namespace {

// The maximum number of times a card can be visible to the user.
const int kMaxSendTabNotificationCardImpressions = 1;

}  // namespace

namespace segmentation_platform {

namespace home_modules {

const char kSendTabInfobarReceivedInLastSessionSignalKey[] =
    "send_tab_infobar_received_in_last_session";

SendTabNotificationPromo::SendTabNotificationPromo(int send_tab_promo_count)
    : CardSelectionInfo(kSendTabNotificationPromo) {}

std::map<SignalKey, FeatureQuery> SendTabNotificationPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kSendTabInfobarReceivedInLastSessionSignalKey,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kSendTabInfobarReceivedInLastSession})}};
  return map;
}

CardSelectionInfo::ShowResult SendTabNotificationPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  CardSelectionInfo::ShowResult result;
  result.result_label = kSendTabNotificationPromo;
  if (base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceShowCardParam,
          "") == features::kSendTabPromoForceOverride) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  } else if (base::GetFieldTrialParamByFeatureAsString(
                 features::kSegmentationPlatformEphemeralCardRanker,
                 features::kEphemeralCardRankerForceHideCardParam,
                 "") == features::kSendTabPromoForceOverride) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }
  if (*signals.GetSignal(kSendTabInfobarReceivedInLastSessionSignalKey)) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool SendTabNotificationPromo::IsEnabled(int impression_count) {
#if BUILDFLAG(IS_IOS)
  if (!send_tab_to_self::
          IsSendTabIOSPushNotificationsEnabledWithMagicStackCard()) {
    return false;
  }
#endif  // BUILDFLAG(IS_IOS)

  // Mark that the card shouldn't be shown if:
  // 1) the force hide feature param is set.
  // 2) the card has reached its max impression count and the force show
  // feature param is not set.
  if (base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceHideCardParam,
          "") == features::kSendTabPromoForceOverride) {
    return false;
  }
  if (impression_count > kMaxSendTabNotificationCardImpressions &&
      base::GetFieldTrialParamByFeatureAsString(
          features::kSegmentationPlatformEphemeralCardRanker,
          features::kEphemeralCardRankerForceShowCardParam,
          "") != features::kSendTabPromoForceOverride) {
    return true;
  }

  return true;
}

}  // namespace home_modules
}  // namespace segmentation_platform
