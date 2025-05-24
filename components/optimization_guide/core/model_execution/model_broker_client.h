// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
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

struct CreateSessionArgs final {
  CreateSessionArgs(base::WeakPtr<OptimizationGuideLogger> logger,
                    ExecuteRemoteFn remote_fn);
  ~CreateSessionArgs();

  CreateSessionArgs(const CreateSessionArgs&);

  base::WeakPtr<OptimizationGuideLogger> logger_;
  ExecuteRemoteFn remote_fn_;
};

class ModelClient final : public TextSafetyClient {
 public:
  ModelClient(mojo::PendingRemote<mojom::ModelSolution> remote,
              mojom::ModelSolutionConfigPtr config);
  ~ModelClient() override;

  // Construct a session for this capability.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> CreateSession(
      const CreateSessionArgs& args,
      const std::optional<SessionConfigParams>& config_params);

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
  ModelBasedCapabilityKey key_;
  base::WeakPtrFactory<ModelClient> weak_ptr_factory_{this};
};

class ModelSubscriber final : public mojom::ModelSubscriber {
 public:
  explicit ModelSubscriber(
      mojo::PendingReceiver<mojom::ModelSubscriber> pending);
  ~ModelSubscriber() override;

  using CreateSessionResult =
      std::unique_ptr<OptimizationGuideModelExecutor::Session>;
  using CreateSessionCallback = base::OnceCallback<void(CreateSessionResult)>;
  using ClientCallback = base::OnceCallback<void(base::WeakPtr<ModelClient>)>;

  // Get info about whether the model is / will be available.
  std::optional<mojom::ModelUnavailableReason> unavailable_reason() const {
    return unavailable_reason_;
  }

  // Creates and returns a session via callback as soon as a model is available.
  // Calls the callback with nullptr if the state become NotSupported.
  void CreateSession(const CreateSessionArgs& args,
                     const std::optional<SessionConfigParams>& config_params,
                     CreateSessionCallback callback);

  // Wait for the client to be available and call the callback with a reference.
  // Calls the callback with nullptr if the state become NotSupported.
  void WaitForClient(ClientCallback callback);

 private:
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
  mojo::Receiver<mojom::ModelSubscriber> receiver_;
};

class ModelBrokerClient final {
 public:
  explicit ModelBrokerClient(mojo::PendingRemote<mojom::ModelBroker> remote,
                             CreateSessionArgs args);
  ~ModelBrokerClient();

  using CreateSessionResult = ModelSubscriber::CreateSessionResult;
  using CreateSessionCallback = ModelSubscriber::CreateSessionCallback;

  // Get or create the subscriber for the given key.
  ModelSubscriber& GetSubscriber(mojom::ModelBasedCapabilityKey key);

  // Async session creation.
  void CreateSession(mojom::ModelBasedCapabilityKey key,
                     const std::optional<SessionConfigParams>& config_params,
                     CreateSessionCallback callback);

 private:
  mojo::Remote<mojom::ModelBroker> remote_;
  CreateSessionArgs args_;

  absl::flat_hash_map<mojom::ModelBasedCapabilityKey,
                      std::unique_ptr<ModelSubscriber>>
      subscribers_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_CLIENT_H_
