// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_

#include <map>
#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {
class OnDeviceModelMojomImpl;
}  // namespace on_device_model

namespace optimization_guide {

namespace features {
BASE_DECLARE_FEATURE(kRequirePersistentModeForScamDetection);
}  // namespace features

class ModelBrokerAndroid;

// A implementation of OnDeviceCapability for Android.
class ModelBrokerAndroid final : public OnDeviceCapability {
 public:
  class SolutionFactory;

  explicit ModelBrokerAndroid(PrefService& local_state,
                              OptimizationGuideModelProvider& model_provider);
  ~ModelBrokerAndroid() override;

  // OptimizationGuideOnDeviceCapabilityProvider:
  void BindModelBroker(
      mojo::PendingReceiver<mojom::ModelBroker> receiver) override;

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateModelRemote(
      proto::ModelExecutionFeature feature);

 private:
  struct ModelService {
    ModelService();
    ~ModelService();
    ModelService(ModelService&&);
    ModelService& operator=(ModelService&&);

    std::unique_ptr<on_device_model::OnDeviceModelMojomImpl> impl;
    mojo::Remote<on_device_model::mojom::OnDeviceModel> remote;
  };

  // Initialize SolutionFactory, if not already initialized.
  void EnsureSolutionFactory(base::OnceClosure done_callback);

  void OnModelDisconnected(
      proto::ModelExecutionFeature feature,
      base::WeakPtr<on_device_model::mojom::OnDeviceModel> model);

  const raw_ref<PrefService> local_state_;

  raw_ref<OptimizationGuideModelProvider> model_provider_;

  // Tracks which assets have been used recently.
  UsageTracker usage_tracker_;

  // Tracks and updates the broker subscribers.
  ModelBrokerImpl impl_;

  // The factory for building solutions.
  // Creating this object may be expensive, so it is lazy-initialized.
  // Must be destroyed before usage_tracker.
  std::unique_ptr<SolutionFactory> solution_factory_;

  // The on-device model services, keyed by feature.
  absl::flat_hash_map<proto::ModelExecutionFeature, ModelService>
      model_services_;

  base::WeakPtrFactory<ModelBrokerAndroid> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
