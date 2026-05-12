// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_

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
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {
class OnDeviceModelMojomImpl;
}  // namespace on_device_model

namespace optimization_guide {

namespace features {
BASE_DECLARE_FEATURE(kAICorePrompt);
BASE_DECLARE_FEATURE(kAICoreScamDetection);
BASE_DECLARE_FEATURE(kAICoreTest);
BASE_DECLARE_FEATURE(kRequirePersistentModeForScamDetection);
}  // namespace features

class ModelBrokerAndroid;

// A implementation of OnDeviceCapability for Android.
class ModelBrokerAndroid final : public OnDeviceCapability,
                                 mojom::ModelBrokerDebug {
 public:
  class SolutionFactory;

  explicit ModelBrokerAndroid(PrefService& local_state,
                              OptimizationGuideModelProvider& model_provider);
  ~ModelBrokerAndroid() override;

  // OnDeviceCapability:
  void BindModelBroker(
      mojo::PendingReceiver<mojom::ModelBroker> receiver) override;
  void BindModelBrokerDebug(
      base::PassKey<on_device_internals::PageHandler> key,
      mojo::PendingReceiver<mojom::ModelBrokerDebug> receiver) override;

  // mojom::ModelBrokerDebug
  void GetStateInfo(
      mojom::ModelBrokerDebug::GetStateInfoCallback callback) override;
  void SetUseCaseRequested(const std::string& use_case,
                           bool requested) override;
  void UninstallModels() override;

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
  void EnsureSolutionFactory(ModelBrokerImpl::InitCallback done_callback);

  // Called when CheckModelStatus completes. Fires all pending init callbacks.
  void OnStatusCheckComplete();

  void OnModelDisconnected(
      proto::ModelExecutionFeature feature,
      base::WeakPtr<on_device_model::mojom::OnDeviceModel> model);

  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer);

  void OnDownloadProgressUpdated(int64_t downloaded_bytes, int64_t total_bytes);

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

  mojo::ReceiverSet<ModelBrokerDebug> receivers_;

  // Callbacks queued while initialization is in flight.
  std::vector<ModelBrokerImpl::InitCallback> pending_init_callbacks_;

  // Whether the status checks have completed.
  bool status_check_complete_ = false;

  // Observers for download progress updates.
  mojo::RemoteSet<on_device_model::mojom::DownloadObserver> download_observers_;

  // Whether a fresh download is actively reporting progress. Set to true when
  // the first OnDownloadProgressUpdated call is received, indicating that a
  // real download is in progress with progress callbacks. Used to decide
  // whether to send an initial zero-progress event to late-joining observers.
  bool has_active_download_progress_ = false;

  // Whether the model was already downloaded. Used to send 0% and 100% progress
  // to observers added after the model became available.
  bool model_already_downloaded_ = false;

  base::WeakPtrFactory<ModelBrokerAndroid> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
