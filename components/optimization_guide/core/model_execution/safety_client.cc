// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/safety_client.h"

#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

SafetyClient::SafetyClient(
    base::WeakPtr<on_device_model::ServiceClient> service_client)
    : service_client_(std::move(service_client)) {}
SafetyClient::~SafetyClient() = default;

void SafetyClient::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  if (!model_info.has_value()) {
    language_detection_model_path_.reset();
    return;
  }
  remote_.reset();  // The remote's assets are outdated.
  language_detection_model_path_ = model_info->GetModelFilePath();
}

void SafetyClient::MaybeUpdateSafetyModel(
    base::optional_ref<const ModelInfo> model_info) {
  auto new_info = SafetyModelInfo::Load(model_info);
  if (!new_info) {
    safety_model_info_.reset();
    return;
  }
  remote_.reset();  // The remote's assets are outdated.
  safety_model_info_ = std::move(new_info);
}

base::expected<std::unique_ptr<SafetyChecker>, OnDeviceModelEligibilityReason>
SafetyClient::MakeSafetyChecker(ModelBasedCapabilityKey feature,
                                bool can_skip) {
  if (!features::ShouldUseTextSafetyClassifierModel() || can_skip) {
    // Construct a dummy checker that always passes all checks.
    return std::make_unique<SafetyChecker>(
        nullptr, on_device_model::TextSafetyLoaderParams(), SafetyConfig());
  }
  if (!safety_model_info_) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable);
  }
  auto config =
      safety_model_info_->GetConfig(ToModelExecutionFeatureProto(feature));
  if (!config) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature);
  }
  if (!config->allowed_languages().empty() && !language_detection_model_path_) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kLanguageDetectionModelNotAvailable);
  }

  // TODO: crbug.com/375492234 - It's weird that we pass params here. Ideally
  // this can change so that the SafetyChecker always runs checks with the
  // latest config.
  return std::make_unique<SafetyChecker>(weak_ptr_factory_.GetWeakPtr(),
                                         LoaderParams(), SafetyConfig(*config));
}

on_device_model::TextSafetyLoaderParams SafetyClient::LoaderParams() const {
  on_device_model::TextSafetyLoaderParams params;
  // Populate the model paths even if they are not needed for the current
  // feature, since the base model remote could be used for subsequent features.
  if (safety_model_info_) {
    params.ts_paths.emplace();
    params.ts_paths->data = safety_model_info_->GetDataPath();
    params.ts_paths->sp_model = safety_model_info_->GetSpModelPath();
  }
  if (language_detection_model_path_) {
    params.language_paths.emplace();
    params.language_paths->model = *language_detection_model_path_;
  }
  return params;
}

SafetyClient::Remote& SafetyClient::GetTextSafetyModelRemote(
    const on_device_model::TextSafetyLoaderParams& params) {
  if (remote_) {
    return remote_;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadTextSafetyParams, params),
      base::BindOnce(
          [](base::WeakPtr<SafetyClient> self,
             mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel>
                 model,
             on_device_model::mojom::TextSafetyModelParamsPtr params) {
            if (!self || !self->service_client_) {
              // Close the files on a background thread.
              base::ThreadPool::PostTask(
                  FROM_HERE, {base::MayBlock()},
                  base::DoNothingWithBoundArgs(std::move(params)));
            }
            self->service_client_->Get()->LoadTextSafetyModel(std::move(params),
                                                              std::move(model));
          },
          weak_ptr_factory_.GetWeakPtr(),
          remote_.BindNewPipeAndPassReceiver()));
  remote_.reset_on_disconnect();  // Maybe track disconnects?
  remote_.reset_on_idle_timeout(features::GetOnDeviceModelIdleTimeout());
  return remote_;
}

}  // namespace optimization_guide
