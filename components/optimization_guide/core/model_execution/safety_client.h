// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CLIENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CLIENT_H_

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"

namespace optimization_guide {

// Provides a shared remote to the current safety model.
// Loads the model on demand, and disconnects when it is no longer in use.
class SafetyClient final : public TextSafetyClient {
 public:
  using Remote = mojo::Remote<on_device_model::mojom::TextSafetyModel>;

  explicit SafetyClient(
      base::WeakPtr<on_device_model::ServiceClient> service_client);
  ~SafetyClient() override;

  // Updates the language detection model, possibly interrupting ongoing
  // executions.
  void SetLanguageDetectionModel(
      base::optional_ref<const ModelInfo> model_info);
  // Updates the safety model, possibly interrupting ongoing executions.
  void MaybeUpdateSafetyModel(base::optional_ref<const ModelInfo> model_info);

  SafetyModelInfo* safety_model_info() const {
    return safety_model_info_.get();
  }
  std::optional<base::FilePath> language_detection_model_path() const {
    return language_detection_model_path_;
  }

  // Construct a feature-specific safety checker.
  base::expected<std::unique_ptr<SafetyChecker>, OnDeviceModelEligibilityReason>
  MakeSafetyChecker(ModelBasedCapabilityKey feature, bool can_skip);

  // Get the remote, creating it if it was disconnected.
  // The remote will disconnect if it remains idle.
  Remote& GetTextSafetyModelRemote(
      const on_device_model::TextSafetyLoaderParams& params) override;

 private:
  on_device_model::TextSafetyLoaderParams LoaderParams() const;

  // How to get a service remote.
  base::WeakPtr<on_device_model::ServiceClient> service_client_;
  // Can be null for language-detection only mode.
  std::unique_ptr<SafetyModelInfo> safety_model_info_;
  // Full path of the language detection model file if it's available.
  std::optional<base::FilePath> language_detection_model_path_;
  // Remote to the currently loaded safety model.
  Remote remote_;
  base::WeakPtrFactory<SafetyClient> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CLIENT_H_
