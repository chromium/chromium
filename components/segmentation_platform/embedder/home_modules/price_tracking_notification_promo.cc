// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/price_tracking_notification_promo.h"

#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace home_modules {

namespace {

const char kCardName[] = "price_tracking_promo";

}  // namespace

PriceTrackingNotificationPromo::PriceTrackingNotificationPromo()
    : CardSelectionInfo(kCardName) {}

std::map<SignalKey, FeatureQuery> PriceTrackingNotificationPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {"has_subscription",
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_SHOPPING_SERVICE,
           .name = "TotalShoppingBookmarkCount"})},
      {"is_new_user",
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsNewUser})},
      {"is_sycned",
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsSynced})}};
  return map;
}

CardSelectionInfo::ShowResult PriceTrackingNotificationPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  CardSelectionInfo::ShowResult result;
  result.position = EphemeralHomeModuleRank::kNotShown;
  result.result_label = kCardName;
  // TODO(b/361576671): Implement logic.
  return result;
}

}  // namespace home_modules
}  // namespace segmentation_platform
