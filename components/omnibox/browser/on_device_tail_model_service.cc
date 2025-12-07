// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_service.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/on_device_tail_suggest_model_metadata.pb.h"

namespace {
// Constants for TFlite model validation.
constexpr std::string kTestPrefix = "google m";
constexpr std::string_view kModelValidationSwitchName =
    "omnibox-on-device-tail-model-validation";

void InitializeTailModelExecutor(
    OnDeviceTailModelExecutor* executor,
    const base::FilePath& model_file,
    const base::flat_set<base::FilePath>& additional_files,
    const optimization_guide::proto::OnDeviceTailSuggestModelMetadata&
        metadata) {
  if (executor == nullptr) {
    return;
  }
  if (model_file.empty() || additional_files.empty()) {
    return;
  }
  bool init_success = executor->Init(model_file, additional_files, metadata);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kModelValidationSwitchName)) {
    return;
  }
  // Histograms only for model validation.
  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.OnDeviceTailModel.InitExecutor",
                          init_success);
  if (init_success) {
    OnDeviceTailModelExecutor::ModelInput input(kTestPrefix, "", 5);
    std::vector<OnDeviceTailModelExecutor::Prediction> predictions =
        executor->GenerateSuggestionsForPrefix(input);
    LOCAL_HISTOGRAM_BOOLEAN("Omnibox.OnDeviceTailModel.HasResultForTestPrefix",
                            !predictions.empty());
  }
}

std::vector<OnDeviceTailModelExecutor::Prediction> RunTailModelExecutor(
    OnDeviceTailModelExecutor* executor,
    const OnDeviceTailModelExecutor::ModelInput& input) {
  std::vector<OnDeviceTailModelExecutor::Prediction> predictions;

  if (executor == nullptr) {
    return predictions;
  }

  if (!executor->IsReady() && !executor->Init()) {
    return predictions;
  }

  auto elapsed_timer = base::ElapsedTimer();
  predictions = executor->GenerateSuggestionsForPrefix(input);

  // Logs some useful histograms for model performance analysis.
  base::UmaHistogramCustomTimes("Omnibox.OnDeviceBrainModel.Latency",
                                elapsed_timer.Elapsed(), base::Milliseconds(10),
                                base::Seconds(2), 50);
  base::UmaHistogramExactLinear("Omnibox.OnDeviceBrainModel.NumResults",
                                static_cast<int>(predictions.size()), 4);
  for (const auto& p : predictions) {
    base::UmaHistogramCounts100("Omnibox.OnDeviceBrainModel.ResultLength",
                                static_cast<int>(p.suggestion.size()));
  }

  return predictions;
}

void MaybeUnloadModelExecutor(OnDeviceTailModelExecutor* executor) {
  if (executor == nullptr || !executor->IsReady()) {
    return;
  }
  executor->Reset();
}

}  // namespace

OnDeviceTailModelService::OnDeviceTailModelService(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : model_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      tail_model_executor_(new OnDeviceTailModelExecutor(),
                           base::OnTaskRunnerDeleter(model_task_runner_)),
      model_provider_(model_provider) {
  if (model_provider_ == nullptr) {
    return;
  }

  model_provider_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      /* model_metadata= */ std::nullopt, model_task_runner_, this);

  memory_pressure_listener_registration_ =
      std::make_unique<base::MemoryPressureListenerRegistration>(
          FROM_HERE, base::MemoryPressureListenerTag::kOnDeviceTailModelService,
          this);
}

OnDeviceTailModelService::OnDeviceTailModelService()
    : tail_model_executor_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

OnDeviceTailModelService::~OnDeviceTailModelService() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
        this);
    model_provider_ = nullptr;
  }
}

void OnDeviceTailModelService::Shutdown() {
  memory_pressure_listener_registration_.reset();
}

void OnDeviceTailModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (optimization_target !=
      optimization_guide::proto::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST) {
    return;
  }
  if (!model_info.has_value()) {
    model_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceTailModelExecutor::Reset,
                       base::Unretained(tail_model_executor_.get())));
    return;
  }

  const std::optional<optimization_guide::proto::Any>& metadata =
      model_info->GetModelMetadata();
  std::optional<optimization_guide::proto::OnDeviceTailSuggestModelMetadata>
      tail_model_metadata = std::nullopt;
  if (metadata.has_value()) {
    tail_model_metadata = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::OnDeviceTailSuggestModelMetadata>(
        metadata.value());
  }

  if (!tail_model_metadata.has_value()) {
    DVLOG(1) << "Failed to fetch metadata for Omnibox on device tail model";
    return;
  }
  model_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InitializeTailModelExecutor, tail_model_executor_.get(),
                     model_info->GetModelFilePath(),
                     model_info->GetAdditionalFiles(),
                     tail_model_metadata.value()));
}

void OnDeviceTailModelService::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  if (level != base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  if (model_task_runner_) {
    model_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MaybeUnloadModelExecutor, tail_model_executor_.get()));
  }
}

void OnDeviceTailModelService::GetPredictionsForInput(
    const OnDeviceTailModelExecutor::ModelInput& input,
    ResultCallback result_callback) {
  if (model_task_runner_) {
    base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
    // Do not call the model if memory pressure level is too high.
    if (!monitor ||
        monitor->GetCurrentPressureLevel(
            base::MemoryPressureMonitorTag::kOnDeviceTailModelService) !=
            base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      model_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&RunTailModelExecutor, tail_model_executor_.get(),
                         input),
          std::move(result_callback));
      return;
    }
  }

  std::move(result_callback)
      .Run(std::vector<OnDeviceTailModelExecutor::Prediction>());
}
