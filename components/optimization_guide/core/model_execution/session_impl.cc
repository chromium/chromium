// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {
namespace {

using google::protobuf::RepeatedPtrField;
using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

void LogSessionCreation(OptimizationGuideLogger* logger,
                        mojom::OnDeviceFeature feature) {
  if (logger && logger->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION, logger)
        << "Starting on-device session for " << base::ToString(feature);
  }
}

}  // namespace

SessionImpl::SessionImpl(mojom::OnDeviceFeature feature,
                         OnDeviceOptions on_device_opts)
    : feature_(feature),
      // TODO(crbug.com/403383823): Get these from on_device_context_.
      sampling_params_(*on_device_opts.session_params.sampling_params),
      capabilities_(on_device_opts.session_params.capabilities) {
  if (on_device_opts.ShouldUse()) {
    TRACE_EVENT("optimization_guide", "SessionImpl::Warmup", "target",
                base::ToString(feature_));
    LogSessionCreation(on_device_opts.logger.get(), feature_);
    on_device_context_ =
        std::make_unique<OnDeviceContext>(std::move(on_device_opts), feature_);
    // Prewarm the initial session to make sure the service is started.
    on_device_context_->GetOrCreateSession();
  }
}

SessionImpl::SessionImpl(mojom::OnDeviceFeature feature,
                         const SamplingParams& sampling_params)
    : feature_(feature), sampling_params_(sampling_params) {}

SessionImpl::~SessionImpl() {}

const TokenLimits& SessionImpl::GetTokenLimits() const {
  if (!on_device_context_) {
    static const TokenLimits null_limits{};
    return null_limits;
  }
  return on_device_context_->opts().token_limits;
}

void SessionImpl::SetInput(MultimodalMessage request,
                           SetInputCallback callback) {
  const auto result = AddContextImpl(std::move(request), std::move(callback));
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceAddContextResult.",
           GetVariantName(feature_)}),
      result);
}

void SessionImpl::AddContext(
    const google::protobuf::MessageLite& request_metadata) {
  SetInput(MultimodalMessage(request_metadata), {});
}

SessionImpl::AddContextResult SessionImpl::AddContextImpl(
    MultimodalMessage request,
    SetInputCallback callback) {
  if (callback) {
    callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        std::move(callback),
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                OptimizationGuideModelExecutionError::ModelExecutionError::
                    kCancelled)));
  }
  context_ = std::move(request);
  context_start_time_ = base::TimeTicks::Now();

  // Cancel any pending response.
  if (on_device_execution_) {
    on_device_execution_->Cancel();
  }

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    return AddContextResult::kUsingServer;
  }

  if (!on_device_context_->SetInput(context_.read(), std::move(callback))) {
    // Use server if can't construct input.
    DestroyOnDeviceState();
    return AddContextResult::kFailedConstructingInput;
  }

  return AddContextResult::kUsingOnDevice;
}

void SessionImpl::Score(const std::string& text,
                        OptimizationGuideModelScoreCallback callback) {
  // Fail if not using on device, or no session was started yet.
  if (!on_device_context_ || !on_device_context_->CanUse()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  on_device_context_->GetOrCreateSession()->Score(
      text,
      base::BindOnce([](float score) { return std::optional<float>(score); })
          .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                            std::nullopt)));
}

void SessionImpl::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  ExecuteModelWithResponseConstraint(request_metadata,
                                     /*constraint=*/nullptr,
                                     std::move(callback));
}

void SessionImpl::ExecuteModelWithResponseConstraint(
    const google::protobuf::MessageLite& request_metadata,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  auto logger = std::make_unique<OnDeviceExecution::ResultLogger>(feature_);

  // Compute the amount of time context would have for processing assuming
  // on-device execution.
  base::TimeDelta context_start_to_execution = base::TimeDelta::Min();
  if (context_start_time_ != base::TimeTicks()) {
    context_start_to_execution = base::TimeTicks::Now() - context_start_time_;
    base::UmaHistogramLongTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.ContextStartToExecutionTime.",
             GetVariantName(feature_)}),
        context_start_to_execution);
    // Only interested in logging the first request after adding context.
    context_start_time_ = base::TimeTicks();
  }

  auto merged_request = context_.Merge(request_metadata);

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    std::move(callback).Run(OptimizationGuideModelStreamingExecutionResult(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                OptimizationGuideModelExecutionError::ModelExecutionError::
                    kGenericFailure)),
        /*provided_by_on_device=*/true));
    return;
  }

  if (on_device_execution_) {
    on_device_execution_->Cancel();
  }
  on_device_execution_.reset();

  // Set new pending response.
  on_device_execution_.emplace(
      feature_, on_device_context_->opts(), std::move(merged_request),
      std::move(constraint), std::move(logger), std::move(callback),
      base::BindOnce(&SessionImpl::OnDeviceExecutionTerminated,
                     weak_ptr_factory_.GetWeakPtr()));

  on_device_execution_->BeginExecution(*on_device_context_);
}

void SessionImpl::OnDeviceExecutionTerminated(bool healthy) {
  on_device_execution_.reset();
  if (!healthy) {
    DestroyOnDeviceState();
  }
}

bool SessionImpl::ShouldUseOnDeviceModel() const {
  return on_device_context_ && on_device_context_->CanUse();
}

void SessionImpl::DestroyOnDeviceState() {
  on_device_context_.reset();
}

void SessionImpl::GetSizeInTokens(
    const std::string& text,
    OptimizationGuideModelSizeInTokenCallback callback) {
  if (!ShouldUseOnDeviceModel()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  auto input = on_device_model::mojom::Input::New();
  input->pieces.push_back(text);
  on_device_context_->GetOrCreateSession()->GetSizeInTokens(
      std::move(input), base::BindOnce([](uint32_t size) {
                          return std::optional<uint32_t>(size);
                        })
                            .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                std::move(callback), std::nullopt)));
}

void SessionImpl::GetExecutionInputSizeInTokens(
    MultimodalMessageReadView request_metadata,
    OptimizationGuideModelSizeInTokenCallback callback) {
  GetSizeInTokensInternal(request_metadata, std::move(callback),
                          /*want_input_context=*/false);
}

void SessionImpl::GetContextSizeInTokens(
    MultimodalMessageReadView request_metadata,
    OptimizationGuideModelSizeInTokenCallback callback) {
  GetSizeInTokensInternal(request_metadata, std::move(callback),
                          /*want_input_context=*/true);
}

const proto::Any& SessionImpl::GetOnDeviceFeatureMetadata() const {
  return on_device_context_->opts().adapter->GetFeatureMetadata();
}

const SamplingParams SessionImpl::GetSamplingParams() const {
  return sampling_params_;
}

on_device_model::Capabilities SessionImpl::GetCapabilities() const {
  return capabilities_;
}

std::unique_ptr<OnDeviceSession> SessionImpl::Clone() {
  auto session = std::make_unique<SessionImpl>(feature_, sampling_params_);
  session->context_ = context_.Clone();
  session->context_start_time_ = context_start_time_;
  if (on_device_context_ && on_device_context_->CanUse()) {
    session->on_device_context_ = on_device_context_->Clone();
  }
  return session;
}

void SessionImpl::SetPriority(on_device_model::mojom::Priority priority) {
  if (on_device_context_) {
    on_device_context_->SetPriority(priority);
  }
}

void SessionImpl::GetSizeInTokensInternal(
    MultimodalMessageReadView request,
    OptimizationGuideModelSizeInTokenCallback callback,
    bool want_input_context) {
  if (!ShouldUseOnDeviceModel()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  auto input = on_device_context_->opts().adapter->ConstructInputString(
      request, want_input_context);
  if (!input) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  on_device_context_->GetOrCreateSession()->GetSizeInTokens(
      std::move(input->input),
      base::BindOnce(
          [](uint32_t size) { return std::optional<uint32_t>(size); })
          .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                            std::nullopt)));
}

}  // namespace optimization_guide
