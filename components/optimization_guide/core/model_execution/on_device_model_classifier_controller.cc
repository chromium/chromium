// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_classifier_controller.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

namespace {

// Similar mapping as `GetBaseModelError` in
// components/optimization_guide/core/model_execution/on_device_model_service_controller.cc
// except for kReady.
OnDeviceModelEligibilityReason GetBaseModelError(
    OnDeviceModelStatus on_device_model_status) {
  switch (on_device_model_status) {
    case OnDeviceModelStatus::kNotEligible:
      return OnDeviceModelEligibilityReason::kModelNotEligible;
    case OnDeviceModelStatus::kInsufficientDiskSpace:
      return OnDeviceModelEligibilityReason::kInsufficientDiskSpace;
    case OnDeviceModelStatus::kInstallNotComplete:
    case OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason:
    case OnDeviceModelStatus::kModelInstalledTooLate:
    case OnDeviceModelStatus::kNotReadyForUnknownReason:
    case OnDeviceModelStatus::kNoOnDeviceFeatureUsed:
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
    case OnDeviceModelStatus::kReady:
      NOTREACHED();
  }
}

void CloseFilesInBackground(on_device_model::ModelAssets assets) {
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(assets)));
}

}  // namespace

class OnDeviceModelClassifierController::Solution
    : public ModelBrokerImpl::Solution {
 public:
  explicit Solution(base::WeakPtr<OnDeviceModelClassifierController> controller)
      : controller_(controller) {
    adapter_ = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
        CreateFeatureConfig());
  }
  ~Solution() override = default;

  bool IsValid() const override { return !!controller_; }

  // Hardcoded dummy config.
  mojom::ModelSolutionConfigPtr MakeConfig() const override {
    auto config = mojom::ModelSolutionConfig::New();
    config->max_tokens = kOnDeviceModelMaxTokens;
    config->feature_config = mojo_base::ProtoWrapper(CreateFeatureConfig());
    config->text_safety_config =
        mojo_base::ProtoWrapper(proto::FeatureTextSafetyConfiguration());
    config->model_versions =
        mojo_base::ProtoWrapper(proto::OnDeviceModelVersions());
    return config;
  }

  const OnDeviceModelFeatureAdapter* GetAdapter() const override {
    return adapter_.get();
  }

  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> pending,
      on_device_model::mojom::SessionParamsPtr params) override {
    TRACE_EVENT("optimization_guide",
                "OnDeviceModelClassifierController::Solution::CreateSession");
    if (controller_) {
      controller_->GetOrCreateRemote()->StartSession(std::move(pending),
                                                     std::move(params));
    }
  }

  void CreateTextSafetySession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending)
      override {
    // No text safety for classifier.
  }

  void ReportHealthyCompletion() override {
    TRACE_EVENT(
        "optimization_guide",
        "OnDeviceModelClassifierController::Solution::ReportHealthyCompletion");
  }

 private:
  // Hardcoded minimum config for the classifier model.
  static proto::OnDeviceModelExecutionFeatureConfig CreateFeatureConfig() {
    proto::OnDeviceModelExecutionFeatureConfig config;
    auto* input_config = config.mutable_input_config();
    input_config->set_request_base_name(
        "optimization_guide.proto.ClassifyApiRequest");

    // Hardcoded substitution for the classifier model.
    auto* substitution = input_config->add_execute_substitutions();
    substitution->set_string_template("%s");

    auto* sub_arg = substitution->add_substitutions();
    auto* candidate = sub_arg->add_candidates();
    candidate->mutable_proto_field()->add_proto_descriptors()->set_tag_number(
        1);

    auto* output_config = config.mutable_output_config();
    output_config->set_proto_type(
        "optimization_guide.proto.ClassifyApiResponse");
    output_config->mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(1);
    config.set_feature(
        proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_CLASSIFIER);
    return config;
  }

  base::WeakPtr<OnDeviceModelClassifierController> controller_;
  scoped_refptr<OnDeviceModelFeatureAdapter> adapter_;
};

OnDeviceModelClassifierController::OnDeviceModelClassifierController(
    PrefService& local_state,
    base::SafeRef<PerformanceClassifier> performance_classifier,
    UsageTracker& usage_tracker,
    base::SafeRef<on_device_model::ServiceClient> service_client,
    ModelBrokerImpl& model_broker_impl,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> delegate)
    : service_client_(service_client),
      model_broker_impl_(model_broker_impl),
      usage_tracker_(usage_tracker),
      component_state_manager_(&local_state,
                               performance_classifier,
                               usage_tracker,
                               std::move(delegate)) {
  component_state_manager_.AddObserver(this);
}

OnDeviceModelClassifierController::~OnDeviceModelClassifierController() {
  component_state_manager_.RemoveObserver(this);
}

void OnDeviceModelClassifierController::StateChanged(
    MaybeOnDeviceModelComponentState state) {
  remote_.reset();
  if (state.has_value()) {
    model_metadata_ = std::make_unique<OnDeviceModelMetadata>(
        state->get().GetInstallDirectory(),
        state->get().GetComponentVersion().GetString(),
        state->get().GetBaseModelSpec(), proto::OnDeviceModelExecutionConfig());
    model_status_ = OnDeviceModelStatus::kReady;
  } else {
    model_metadata_.reset();
    model_status_ = state.error();
  }
  UpdateSolution();
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelClassifierController::GetOrCreateRemote() {
  if (remote_) {
    return remote_;
  }
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelClassifierController::"
              "CreateRemote");
  service_client_->AddPendingUsage();  // Warm up the service.
  on_device_model::ModelAssetPaths paths;
  paths.weights = model_metadata_->model_path().Append(kWeightsFile);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, paths),
      base::BindOnce(
          [](base::WeakPtr<OnDeviceModelClassifierController> self,
             mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel>
                 receiver,
             on_device_model::ModelAssets assets) {
            if (!self || !self->service_client_->is_bound()) {
              if (self) {
                self->service_client_->RemovePendingUsage();
              }
              CloseFilesInBackground(std::move(assets));
              return;
            }
            TRACE_EVENT("optimization_guide",
                        "OnDeviceModelClassifierController::"
                        "OnModelAssetsLoaded");
            auto params = on_device_model::mojom::LoadModelParams::New();
            params->assets = std::move(assets);
            params->max_tokens = kOnDeviceModelMaxTokens;
            // Hardcoded because the model can only run on CPU.
            params->backend_type = ml::ModelBackendType::kCpuBackend;

            self->service_client_->Get()->LoadModel(
                std::move(params), std::move(receiver), base::DoNothing());
            self->service_client_->RemovePendingUsage();
          },
          weak_ptr_factory_.GetWeakPtr(),
          remote_.BindNewPipeAndPassReceiver()));

  remote_.set_disconnect_with_reason_handler(
      base::BindOnce(&OnDeviceModelClassifierController::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  remote_.reset_on_idle_timeout(features::GetOnDeviceModelIdleTimeout());

  return remote_;
}

void OnDeviceModelClassifierController::OnDisconnect(
    uint32_t reason,
    const std::string& description) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelClassifierController::OnDisconnect");
  remote_.reset();
  const bool is_idle =
      reason == static_cast<uint32_t>(
                    on_device_model::ModelDisconnectReason::kIdleShutdown);
  if (is_idle) {
    return;
  }
  LOG(ERROR) << "TinyGemma model disconnected unexpectedly.";
}

void OnDeviceModelClassifierController::UpdateSolution() {
  model_broker_impl_->GetSolutionProvider(mojom::OnDeviceFeature::kClassifier)
      .Update(GetSolution());
}

ModelBrokerImpl::MaybeSolution
OnDeviceModelClassifierController::GetSolution() {
  // No adaptation or text safety for classifier, so component ready implies
  // solution ready.
  if (model_status_ == OnDeviceModelStatus::kReady) {
    return std::make_unique<Solution>(weak_ptr_factory_.GetWeakPtr());
  }
  auto error = GetBaseModelError(model_status_);
  if (error != OnDeviceModelEligibilityReason::kModelToBeInstalled) {
    // Device eligibility not determined yet or device ineligible takes
    // precedence over feature usage.
    return base::unexpected(error);
  }

  if (!usage_tracker_->WasOnDeviceEligibleFeatureRecentlyUsed(
          mojom::OnDeviceFeature::kClassifier)) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed);
  }
  return base::unexpected(OnDeviceModelEligibilityReason::kModelToBeInstalled);
}

}  // namespace optimization_guide
