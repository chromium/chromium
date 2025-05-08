// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

struct OnDeviceOptions final {
  OnDeviceOptions();
  OnDeviceOptions(const OnDeviceOptions&);
  OnDeviceOptions(OnDeviceOptions&&);
  ~OnDeviceOptions();

  class Client {
   public:
    virtual ~Client() = 0;
    // Create another client for the same model.
    virtual std::unique_ptr<Client> Clone() const = 0;
    // Called to check whether this client is still usable.
    virtual bool ShouldUse() = 0;
    // Called to create a new empty session.
    virtual void StartSession(
        mojo::PendingReceiver<on_device_model::mojom::Session> pending,
        on_device_model::mojom::SessionParamsPtr params) = 0;
    // Called to report a successful execution of the model.
    virtual void OnResponseCompleted() = 0;
  };

  std::unique_ptr<Client> model_client;
  proto::OnDeviceModelVersions model_versions;
  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter;
  std::unique_ptr<SafetyChecker> safety_checker;
  TokenLimits token_limits;
  on_device_model::Capabilities capabilities;
  SamplingParams sampling_params;

  base::WeakPtr<OptimizationGuideLogger> logger;

  // Returns true if the on-device model may be used.
  bool ShouldUse() const;
};

// Constructs an on-device session and populates it with input context.
// Context is processed incrementally. After the min context size has been
// processed, any pending context processing will be cancelled if an
// CloneSession() call is made.
class OnDeviceContext : public on_device_model::mojom::ContextClient {
 public:
  OnDeviceContext(OnDeviceOptions opts, ModelBasedCapabilityKey feature);
  ~OnDeviceContext() override;

  // Constructs the input context and begins processing it.
  bool SetInput(
      MultimodalMessageReadView request,
      OptimizationGuideModelExecutor::Session::SetInputCallback callback);

  // Get the session that we've sent the input to, creating it if does not
  // exist (e.g. due to a disconnect.)
  mojo::Remote<on_device_model::mojom::Session>& GetOrCreateSession();

  // Clones from the session to begin processing a request, terminating any
  // optional processing, and logging data about the processing.
  void CloneSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> clone,
      proto::OnDeviceModelServiceRequest* logged_request,
      bool ignore_context);

  const OnDeviceOptions& opts() { return opts_; }

  // Whether using this session is still allowed.
  // This should be checked before called any other public methods.
  bool CanUse() { return opts_.ShouldUse(); }

  // Creates a new OnDeviceContext that builds off the current context. All
  // settings of the cloned object will match this one.
  std::unique_ptr<OnDeviceContext> Clone();

  // Sets the priority of the underlying session.
  void SetPriority(on_device_model::mojom::Priority priority);

 private:
  void Append(on_device_model::mojom::InputPtr input);

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override;

  OnDeviceOptions opts_;
  ModelBasedCapabilityKey feature_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  on_device_model::mojom::InputPtr input_ =
      on_device_model::mojom::Input::New();
  uint32_t tokens_processed_ = 0;
  on_device_model::mojom::Priority priority_ =
      on_device_model::mojom::Priority::kForeground;
  OptimizationGuideModelExecutor::Session::SetInputCallback callback_;
  mojo::ReceiverSet<on_device_model::mojom::ContextClient> clients_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_
