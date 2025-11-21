// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

class ModelClient final : public TextSafetyClient {
 public:
  ModelClient(mojo::PendingRemote<mojom::ModelSolution> remote,
              mojom::ModelSolutionConfigPtr config);
  ~ModelClient() override;

  // Construct a session for this capability.
  std::unique_ptr<OnDeviceSession> CreateSession(
      const SessionConfigParams& config_params,
      base::WeakPtr<OptimizationGuideLogger> logger);

  // TextSafetyClient:
  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> session)
      override;

  base::WeakPtr<ModelClient> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  mojom::ModelSolution& solution() { return *remote_; }

  const OnDeviceModelFeatureAdapter& feature_adapter() const {
    return *feature_adapter_;
  }

  uint32_t max_tokens() const { return max_tokens_; }

  const proto::FeatureTextSafetyConfiguration& safety_config() const {
    return safety_config_;
  }

 private:
  // Called when the remote disconnects.
  void OnDisconnect();

  class OnDeviceOptionsClient;

  mojo::Remote<mojom::ModelSolution> remote_;
  scoped_refptr<const OnDeviceModelFeatureAdapter> feature_adapter_;
  proto::FeatureTextSafetyConfiguration safety_config_;
  proto::OnDeviceModelVersions model_versions_;
  // The full combined limit for input and output tokens.
  uint32_t max_tokens_ = 0;
  mojom::OnDeviceFeature feature_;
  base::WeakPtrFactory<ModelClient> weak_ptr_factory_{this};
};

class ModelSubscriberImpl : public mojom::ModelSubscriber {
 public:
  ModelSubscriberImpl();
  ~ModelSubscriberImpl() override;

  using CreateSessionResult = std::unique_ptr<OnDeviceSession>;
  using CreateSessionCallback = base::OnceCallback<void(CreateSessionResult)>;
  using ClientCallback = base::OnceCallback<void(base::WeakPtr<ModelClient>)>;

  // Get info about whether the model is / will be available.
  std::optional<mojom::ModelUnavailableReason> unavailable_reason() const {
    return unavailable_reason_;
  }

  std::optional<ModelClient>& client() { return client_; }

  // Creates and returns a session via callback as soon as a model is available.
  // Calls the callback with nullptr if the state become NotSupported.
  void CreateSession(const SessionConfigParams& config_params,
                     CreateSessionCallback callback,
                     base::WeakPtr<OptimizationGuideLogger> logger);

  // Wait for the client to be available and call the callback with a reference.
  // Calls the callback with nullptr if the state become NotSupported.
  void WaitForClient(ClientCallback callback);

 protected:
  // mojom::ModelSubscriber
  void Unavailable(mojom::ModelUnavailableReason) override;
  void Available(mojom::ModelSolutionConfigPtr config,
                 mojo::PendingRemote<mojom::ModelSolution> remote) override;

  // Fire all pending callbacks
  void FlushCallbacks();

  std::vector<ClientCallback> callbacks_;
  std::optional<mojom::ModelUnavailableReason> unavailable_reason_ =
      mojom::ModelUnavailableReason::kUnknown;
  std::optional<ModelClient> client_;
};

class ModelSubscriber final : public ModelSubscriberImpl {
 public:
  explicit ModelSubscriber(
      mojo::PendingReceiver<mojom::ModelSubscriber> pending);
  ~ModelSubscriber() override;

 private:
  void OnDisconnect();
  mojo::Receiver<mojom::ModelSubscriber> receiver_;
};

class ModelBrokerClient final {
 public:
  explicit ModelBrokerClient(mojo::PendingRemote<mojom::ModelBroker> remote,
                             base::WeakPtr<OptimizationGuideLogger> logger);
  ~ModelBrokerClient();

  using CreateSessionResult = ModelSubscriber::CreateSessionResult;
  using CreateSessionCallback = ModelSubscriber::CreateSessionCallback;

  // Get or create the subscriber for the given key.
  ModelSubscriber& GetSubscriber(mojom::OnDeviceFeature feature);

  // Whether the subscriber for this key already exists.
  bool HasSubscriber(mojom::OnDeviceFeature feature);

  // Async session creation.
  void CreateSession(mojom::OnDeviceFeature feature,
                     const SessionConfigParams& config_params,
                     CreateSessionCallback callback);

 private:
  mojo::Remote<mojom::ModelBroker> remote_;
  base::WeakPtr<OptimizationGuideLogger> logger_;

  absl::flat_hash_map<mojom::OnDeviceFeature, std::unique_ptr<ModelSubscriber>>
      subscribers_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_
