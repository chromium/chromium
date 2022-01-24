// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service_common.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace permissions {
ClientFeatures_Platform GetCurrentPlatformProto() {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
  return permissions::ClientFeatures_Platform_PLATFORM_DESKTOP;
#elif defined(OS_ANDROID) || defined(OS_FUCHSIA)
  return permissions::ClientFeatures_Platform_PLATFORM_MOBILE;
#else
  return permissions::ClientFeatures_Platform_PLATFORM_UNSPECIFIED;
#endif
}

constexpr char kPlatform[] = "platform";
constexpr char kGesture[] = "gesture";
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
  if (!prediction_list || prediction_list->GetList().empty() ||
      !prediction_list->GetList()[0].is_dict()) {
    return message;
  }

  auto* likelihood_dict =
      prediction_list->GetList()[0].FindDictKey(kGrantLikelihood);
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
