// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_CLASSIFIER_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_CLASSIFIER_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

class PrefService;

namespace optimization_guide {

class PerformanceClassifier;
class UsageTracker;

// Controls the lifecycle of the classifier model, loading and unloading
// of the models, and executing them via the service.
// This class parallels the `OnDeviceModelServiceController` but downloads the
// feature-specific model for Classifier instead of the common base model.
class OnDeviceModelClassifierController
    : public OnDeviceModelComponentStateManager::Observer {
 public:
  OnDeviceModelClassifierController(
      PrefService& local_state,
      base::SafeRef<PerformanceClassifier> performance_classifier,
      UsageTracker& usage_tracker,
      base::SafeRef<on_device_model::ServiceClient> service_client,
      ModelBrokerImpl& model_broker_impl,
      std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> delegate);
  ~OnDeviceModelClassifierController() override;

  // Updates the solution for Classifier feature.
  void UpdateSolution();

 private:
  class Solution;

  // Observes `OnDeviceModelComponentStateManager`.
  void StateChanged(MaybeOnDeviceModelComponentState state) override;
  // Returns the solution for the Classifier feature.
  ModelBrokerImpl::MaybeSolution GetSolution();

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateRemote();
  void OnDisconnect(uint32_t reason, const std::string& description);

  base::SafeRef<on_device_model::ServiceClient> service_client_;
  base::raw_ref<ModelBrokerImpl> model_broker_impl_;
  base::raw_ref<UsageTracker> usage_tracker_;
  OnDeviceModelComponentStateManager component_state_manager_;

  std::unique_ptr<OnDeviceModelMetadata> model_metadata_;
  OnDeviceModelStatus model_status_ =
      OnDeviceModelStatus::kNotReadyForUnknownReason;
  mojo::Remote<on_device_model::mojom::OnDeviceModel> remote_;

  base::WeakPtrFactory<OnDeviceModelClassifierController> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_CLASSIFIER_CONTROLLER_H_
