// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_common.h"

#include <cmath>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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

  NOTREACHED();
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

  NOTREACHED();
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
      NOTREACHED()
          << "CPSS only supports notifications and geolocation at the moment.";
  }

  return proto_request;
}

constexpr char kPlatform[] = "platform";
constexpr char kGesture[] = "gesture";
constexpr char kPlatformEnum[] = "platformEnum";
constexpr char kGestureEnum[] = "gestureEnum";
constexpr char kAvgDenyRate[] = "avgDenyRate";
constexpr char kAvgGrantRate[] = "avgGrantRate";
constexpr char kAvgDismissRate[] = "avgDismissRate";
constexpr char kAvgIgnoreRate[] = "avgIgnoreRate";
constexpr char kPromptsCount[] = "promptsCount";
constexpr char kClientStats[] = "clientStats";
constexpr char kClientFeatures[] = "clientFeatures";
constexpr char kNotificationPermission[] = "notificationPermission";
constexpr char kPermissionStats[] = "permissionStats";
constexpr char kPermissionFeatures[] = "permissionFeatures";
constexpr char kPrediction[] = "prediction";
constexpr char kGrantLikelihood[] = "grantLikelihood";
constexpr char kDiscretizedLikelihood[] = "discretizedLikelihood";

std::string GeneratePredictionsRequestMessageToJson(
    const GeneratePredictionsRequest& message) {
  base::Value dict_message(base::Value::Type::DICTIONARY);

  base::Value client_features(base::Value::Type::DICTIONARY);
  client_features.SetKey(kPlatform, base::Value(ClientFeatures_Platform_Name(
                                        message.client_features().platform())));
  client_features.SetKey(kGesture, base::Value(ClientFeatures_Gesture_Name(
                                       message.client_features().gesture())));
  client_features.SetKey(kPlatformEnum,
                         base::Value(ClientFeatures_PlatformEnum_Name(
                             message.client_features().platform_enum())));
  client_features.SetKey(kGestureEnum,
                         base::Value(ClientFeatures_GestureEnum_Name(
                             message.client_features().gesture_enum())));
  base::Value client_stats(base::Value::Type::DICTIONARY);
  client_stats.SetKey(
      kAvgDenyRate,
      base::Value(message.client_features().client_stats().avg_deny_rate()));
  client_stats.SetKey(
      kAvgGrantRate,
      base::Value(message.client_features().client_stats().avg_grant_rate()));
  client_stats.SetKey(
      kAvgDismissRate,
      base::Value(message.client_features().client_stats().avg_dismiss_rate()));
  client_stats.SetKey(
      kAvgIgnoreRate,
      base::Value(message.client_features().client_stats().avg_ignore_rate()));
  client_stats.SetKey(
      kPromptsCount,
      base::Value(message.client_features().client_stats().prompts_count()));
  client_features.SetKey(kClientStats, std::move(client_stats));
  dict_message.SetKey(kClientFeatures, std::move(client_features));

  CHECK_EQ(false, message.permission_features().empty());

  base::Value permission_features(base::Value::Type::LIST);
  base::Value permission_feature_entry(base::Value::Type::DICTIONARY);
  base::Value notification_features(base::Value::Type::DICTIONARY);
  permission_feature_entry.SetKey(kNotificationPermission,
                                  std::move(notification_features));
  base::Value permission_stats(base::Value::Type::DICTIONARY);
  permission_stats.SetKey(
      kAvgDenyRate,
      base::Value(
          message.permission_features()[0].permission_stats().avg_deny_rate()));
  permission_stats.SetKey(kAvgGrantRate,
                          base::Value(message.permission_features()[0]
                                          .permission_stats()
                                          .avg_grant_rate()));
  permission_stats.SetKey(kAvgDismissRate,
                          base::Value(message.permission_features()[0]
                                          .permission_stats()
                                          .avg_dismiss_rate()));
  permission_stats.SetKey(kAvgIgnoreRate,
                          base::Value(message.permission_features()[0]
                                          .permission_stats()
                                          .avg_ignore_rate()));
  permission_stats.SetKey(
      kPromptsCount,
      base::Value(
          message.permission_features()[0].permission_stats().prompts_count()));
  permission_feature_entry.SetKey(kPermissionStats,
                                  std::move(permission_stats));
  permission_features.Append(std::move(permission_feature_entry));
  dict_message.SetKey(kPermissionFeatures, std::move(permission_features));

  std::string message_str;
  if (base::JSONWriter::Write(dict_message, &message_str))
    return message_str;

  return std::string();
}

std::unique_ptr<GeneratePredictionsResponse>
GeneratePredictionsResponseJsonToMessage(std::string input) {
  auto message = std::make_unique<GeneratePredictionsResponse>();

  auto parsed_message = base::JSONReader::Read(input);
  if (!parsed_message.has_value() || !parsed_message->is_dict())
    return message;

  auto* prediction_list = parsed_message->FindListKey(kPrediction);
  if (!prediction_list || prediction_list->GetListDeprecated().empty() ||
      !prediction_list->GetListDeprecated()[0].is_dict()) {
    return message;
  }

  auto* likelihood_dict =
      prediction_list->GetListDeprecated()[0].FindDictKey(kGrantLikelihood);
  if (!likelihood_dict)
    return message;

  auto* likelihood_str = likelihood_dict->FindStringKey(kDiscretizedLikelihood);
  if (!likelihood_str)
    return message;

  PermissionPrediction_Likelihood_DiscretizedLikelihood likelihood;
  if (!PermissionPrediction_Likelihood_DiscretizedLikelihood_Parse(
          *likelihood_str, &likelihood)) {
    return message;
  }

  // Create entry and set likelihood.
  auto* prediction = message->mutable_prediction()->Add();
  prediction->mutable_grant_likelihood()->set_discretized_likelihood(
      likelihood);

  return message;
}

}  // namespace permissions
