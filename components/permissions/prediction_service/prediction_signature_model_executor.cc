// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_signature_model_executor.h"

#include "components/permissions/prediction_service/prediction_common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

PredictionSignatureModelExecutor::PredictionSignatureModelExecutor() = default;
PredictionSignatureModelExecutor::~PredictionSignatureModelExecutor() = default;

bool PredictionSignatureModelExecutor::Preprocess(
    const std::map<std::string, TfLiteTensor*>& input_tensors,
    const PredictionModelExecutorInput& input) {
  model_metadata_ = input.metadata;
  switch (input.request.permission_features()[0].permission_type_case()) {
    case PermissionFeatures::kNotificationPermission:
      request_type_ = RequestType::kNotifications;
      break;
    case PermissionFeatures::kGeolocationPermission:
      request_type_ = RequestType::kGeolocation;
      break;
    default:
      NOTREACHED();
  }
  auto itr = input_tensors.find("AvgClientDenyRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find AvgClientDenyRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.client_features().client_stats().avg_deny_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate AvgClientDenyRate input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientDismissRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find AvgClientDismissRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.client_features().client_stats().avg_dismiss_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING)
        << "[CPSS] Failed to populate AvgClientDismissRate input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientGrantRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find AvgClientGrantRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.client_features().client_stats().avg_grant_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate AvgClientGrantRate input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientIgnoreRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find AvgClientIgnoreRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.client_features().client_stats().avg_ignore_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING)
        << "[CPSS] Failed to populate AvgClientIgnoreRate input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientPermissionDenyRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING)
        << "[CPSS] Failed to find AvgClientPermissionDenyRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.permission_features()[0]
               .permission_stats()
               .avg_deny_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING)
        << "[CPSS] Failed to populate AvgClientPermissionDenyRate input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientPermissionDismissRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING)
        << "[CPSS] Failed to find AvgClientPermissionDismissRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.permission_features()[0]
               .permission_stats()
               .avg_dismiss_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate AvgClientPermissionDismissRate "
                    "input tensor";
    return false;
  }

  itr = input_tensors.find("AvgClientPermissionGrantRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING)
        << "[CPSS] Failed to find AvgClientPermissionGrantRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.permission_features()[0]
               .permission_stats()
               .avg_grant_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate AvgClientPermissionGrantRate "
                    "input tensor";

    return false;
  }

  itr = input_tensors.find("AvgClientPermissionIgnoreRate");
  if (itr == input_tensors.end()) {
    LOG(WARNING)
        << "[CPSS] Failed to find AvgClientPermissionIgnoreRate input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<float>(
           input.request.permission_features()[0]
               .permission_stats()
               .avg_ignore_rate(),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate AvgClientPermissionIgnoreRate "
                    "input tensor";
    return false;
  }

  itr = input_tensors.find("ClientTotalPermissionPrompts");
  if (itr == input_tensors.end()) {
    LOG(WARNING)
        << "[CPSS] Failed to find ClientTotalPermissionPrompts input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<int64_t>(
           static_cast<int64_t>(input.request.permission_features()[0]
                                    .permission_stats()
                                    .prompts_count()),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate ClientTotalPermissionPrompts "
                    "input tensor";
    return false;
  }

  itr = input_tensors.find("ClientTotalPrompts");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find ClientTotalPrompts input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<int64_t>(
           static_cast<int64_t>(
               input.request.client_features().client_stats().prompts_count()),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate ClientTotalPrompts input tensor";
    return false;
  }

  itr = input_tensors.find("GestureEnum");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find GestureEnum input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<int64_t>(
           static_cast<int64_t>(input.request.client_features().gesture_enum()),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate GestureEnum input tensor";
    return false;
  }

  itr = input_tensors.find("PlatformEnum");
  if (itr == input_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find PlatformEnum input tensor";
    return false;
  }
  if (!tflite::task::core::PopulateTensor<int64_t>(
           static_cast<int64_t>(
               input.request.client_features().platform_enum()),
           itr->second)
           .ok()) {
    LOG(WARNING) << "[CPSS] Failed to populate PlatformEnum input tensor";
    return false;
  }
  return true;
}

std::optional<GeneratePredictionsResponse>
PredictionSignatureModelExecutor::Postprocess(
    const std::map<std::string, const TfLiteTensor*>& output_tensors) {
  DCHECK(request_type_ == RequestType::kNotifications ||
         request_type_ == RequestType::kGeolocation);
  auto itr = output_tensors.find("outputs");
  if (itr == output_tensors.end()) {
    LOG(WARNING) << "[CPSS] Failed to find outputs tensor";
    return std::nullopt;
  }
  std::vector<float> data;
  if (!tflite::task::core::PopulateVector<float>(itr->second, &data).ok()) {
    LOG(WARNING) << "[CPSS] Failed to read from outputs tensor";
    return std::nullopt;
  }

  float threshold = request_type_ == RequestType::kNotifications
                        ? kNotificationPredictionsThreshold
                        : kGeolocationPredictionsThreshold;

  // If the model has a metadata which contains a threshold value,
  // use that threshold value.
  if (model_metadata_ && model_metadata_->has_not_grant_thresholds()) {
    // max_likely represents very likely to not grant
    threshold = model_metadata_->not_grant_thresholds().max_likely();
    base::UmaHistogramEnumeration(
        "Permissions.PredictionService.PredictionThresholdSource",
        PermissionPredictionThresholdSource::MODEL_METADATA);
  } else {
    base::UmaHistogramEnumeration(
        "Permissions.PredictionService.PredictionThresholdSource",
        PermissionPredictionThresholdSource::HARDCODED_FALLBACK);
  }
  GeneratePredictionsResponse response;
  response.mutable_prediction()
      ->Add()
      ->mutable_grant_likelihood()
      ->set_discretized_likelihood(
          data[0] > threshold
              ? PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY
              : PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED);

  return response;
}

const char* PredictionSignatureModelExecutor::GetSignature() {
  return "serving_default";
}
}  // namespace permissions
