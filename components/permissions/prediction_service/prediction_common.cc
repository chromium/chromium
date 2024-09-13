// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_common.h"

#include <cmath>
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace permissions {

float GetRoundedRatio(int numerator, int denominator) {
  if (denominator == 0)
    return 0;
  return roundf(numerator / kRoundToMultiplesOf / denominator) *
         kRoundToMultiplesOf;
}

int GetRoundedRatioForUkm(int numerator, int denominator) {
  return GetRoundedRatio(numerator, denominator) * 100;
}

int BucketizeValue(int count) {
  for (const int bucket : kCountBuckets) {
    if (count >= bucket)
      return bucket;
  }
  return 0;
}

ClientFeatures_Platform GetCurrentPlatformProto() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  return permissions::ClientFeatures_Platform_PLATFORM_DESKTOP;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  return permissions::ClientFeatures_Platform_PLATFORM_MOBILE;
#else
  return permissions::ClientFeatures_Platform_PLATFORM_UNSPECIFIED;
#endif
}

ClientFeatures_PlatformEnum GetCurrentPlatformEnumProto() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  return permissions::ClientFeatures_PlatformEnum_PLATFORM_DESKTOP_V2;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  return permissions::ClientFeatures_PlatformEnum_PLATFORM_MOBILE_V2;
#else
  return permissions::ClientFeatures_PlatformEnum_PLATFORM_UNSPECIFIED_V2;
#endif
}

ClientFeatures_Gesture ConvertToProtoGesture(
    const permissions::PermissionRequestGestureType type) {
  switch (type) {
    case permissions::PermissionRequestGestureType::GESTURE:
      return permissions::ClientFeatures_Gesture_GESTURE;
    case permissions::PermissionRequestGestureType::NO_GESTURE:
      return permissions::ClientFeatures_Gesture_NO_GESTURE;
    case permissions::PermissionRequestGestureType::UNKNOWN:
      return permissions::ClientFeatures_Gesture_GESTURE_UNSPECIFIED;
    case permissions::PermissionRequestGestureType::NUM:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return permissions::ClientFeatures_Gesture_GESTURE_UNSPECIFIED;
}

ClientFeatures_GestureEnum ConvertToProtoGestureEnum(
    const permissions::PermissionRequestGestureType type) {
  switch (type) {
    case permissions::PermissionRequestGestureType::GESTURE:
      return permissions::ClientFeatures_GestureEnum_GESTURE_V2;
    case permissions::PermissionRequestGestureType::NO_GESTURE:
    case permissions::PermissionRequestGestureType::UNKNOWN:
      return permissions::ClientFeatures_GestureEnum_GESTURE_UNSPECIFIED_V2;
    case permissions::PermissionRequestGestureType::NUM:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return permissions::ClientFeatures_GestureEnum_GESTURE_UNSPECIFIED_V2;
}

void FillInStatsFeatures(const PredictionRequestFeatures::ActionCounts& counts,
                         StatsFeatures* features) {
  int total_counts = counts.total();

  // Round to only 2 decimal places to help prevent fingerprinting.
  features->set_avg_deny_rate(GetRoundedRatio(counts.denies, total_counts));
  features->set_avg_dismiss_rate(
      GetRoundedRatio(counts.dismissals, total_counts));
  features->set_avg_grant_rate(GetRoundedRatio(counts.grants, total_counts));
  features->set_avg_ignore_rate(GetRoundedRatio(counts.ignores, total_counts));
  features->set_prompts_count(BucketizeValue(total_counts));
}

std::unique_ptr<GeneratePredictionsRequest> GetPredictionRequestProto(
    const PredictionRequestFeatures& entity) {
  auto proto_request = std::make_unique<GeneratePredictionsRequest>();

  ClientFeatures* client_features = proto_request->mutable_client_features();
  client_features->set_platform(GetCurrentPlatformProto());
  client_features->set_gesture(ConvertToProtoGesture(entity.gesture));
  client_features->set_platform_enum(GetCurrentPlatformEnumProto());
  client_features->set_gesture_enum(ConvertToProtoGestureEnum(entity.gesture));
  FillInStatsFeatures(entity.all_permission_counts,
                      client_features->mutable_client_stats());

  PermissionFeatures* permission_features =
      proto_request->mutable_permission_features()->Add();
  FillInStatsFeatures(entity.requested_permission_counts,
                      permission_features->mutable_permission_stats());

  switch (entity.type) {
    case RequestType::kNotifications:
      permission_features->mutable_notification_permission()->Clear();
      break;
    case RequestType::kGeolocation:
      permission_features->mutable_geolocation_permission()->Clear();
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "CPSS only supports notifications and geolocation at the moment.";
  }
  if (!entity.url.is_empty()) {
    SiteFeatures* site_features = proto_request->mutable_site_features();
    site_features->set_origin(entity.url.spec());
  }

  ClientFeatures_ExperimentConfig* experiment_config =
      client_features->mutable_experiment_config();
  experiment_config->set_experiment_id(entity.experiment_id);

  return proto_request;
}

}  // namespace permissions
