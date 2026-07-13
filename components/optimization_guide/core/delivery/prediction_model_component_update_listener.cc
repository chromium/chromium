// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_model_component_update_listener.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"

namespace optimization_guide {

PredictionModelComponentUpdateListener::PredictionModelComponentUpdateListener(
    OptimizationGuideModelProvider& fallback_provider,
    RegisterComponentCallback register_component_callback)
    : registry_(OptimizationGuideLogger::GetInstance()),
      default_model_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      fallback_provider_(fallback_provider),
      register_component_callback_(std::move(register_component_callback)) {
  DCHECK(register_component_callback_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PredictionModelComponentUpdateListener::
    ~PredictionModelComponentUpdateListener() = default;

void PredictionModelComponentUpdateListener::
    AddObserverForOptimizationTargetModel(
        proto::OptimizationTarget optimization_target,
        const std::optional<proto::Any>& model_metadata,
        scoped_refptr<base::SequencedTaskRunner> model_task_runner,
        OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsMigrated(optimization_target)) {
    fallback_provider_->AddObserverForOptimizationTargetModel(
        optimization_target, model_metadata, std::move(model_task_runner),
        observer);
    return;
  }

  if (auto [it, ok] = registered_targets_.emplace(optimization_target); ok) {
    register_component_callback_.Run(optimization_target, GetWeakPtr());
  }

  optimization_target_model_task_runner_.emplace(optimization_target,
                                                 model_task_runner);
  registry_.AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, std::move(model_task_runner),
      observer);
}

void PredictionModelComponentUpdateListener::
    RemoveObserverForOptimizationTargetModel(
        proto::OptimizationTarget optimization_target,
        OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsMigrated(optimization_target)) {
    fallback_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_target, observer);
    return;
  }

  registry_.RemoveObserverForOptimizationTargetModel(optimization_target,
                                                     observer);
  if (!registry_.IsRegistered(optimization_target)) {
    optimization_target_model_task_runner_.erase(optimization_target);
  }
}

void PredictionModelComponentUpdateListener::SetModelDownloadSchedulingParams(
    proto::OptimizationTarget optimization_target,
    const download::SchedulingParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsMigrated(optimization_target)) {
    fallback_provider_->SetModelDownloadSchedulingParams(optimization_target,
                                                         params);
    return;
  }
}

bool PredictionModelComponentUpdateListener::IsMigrated(
    proto::OptimizationTarget optimization_target) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetPredictionModelComponentConfig(optimization_target).has_value();
}

void PredictionModelComponentUpdateListener::MaybeUpdateModel(
    proto::OptimizationTarget target,
    const base::Version& version,
    const base::FilePath& install_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(version.IsValid());

  if (install_dir.empty()) {
    return;
  }

  auto [it, inserted] =
      component_info_map_.insert({target, ComponentInfo{version, install_dir}});
  if (!inserted) {
    it->second.version = version;
    it->second.install_dir = install_dir;
  }

  // Load the model in the background.
  GetTaskRunner(target)->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadAndVerifyModelInfoOffThread, target, install_dir),
      base::BindOnce(&PredictionModelComponentUpdateListener::OnModelLoaded,
                     GetWeakPtr(), target, version));
}

void PredictionModelComponentUpdateListener::OnModelLoaded(
    proto::OptimizationTarget target,
    const base::Version& version,
    std::optional<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if this target is still expected and the version matches.
  auto it = component_info_map_.find(target);
  if (it == component_info_map_.end() || it->second.version != version) {
    // Discard because it was uninstalled or a newer version is loading.
    return;
  }

  if (!model_info) {
    // The model is invalid. We keep it in component_info_map_, since we've
    // still "loaded" it and we don't want to re-do that, but we'll report it to
    // the model observers as removed.
    registry_.RemoveModel(target);
    return;
  }

  registry_.UpdateModel(target, std::move(*model_info));
}

void PredictionModelComponentUpdateListener::OnModelUninstalled(
    proto::OptimizationTarget target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  component_info_map_.erase(target);
  registry_.RemoveModel(target);
}

scoped_refptr<base::SequencedTaskRunner>
PredictionModelComponentUpdateListener::GetTaskRunner(
    proto::OptimizationTarget target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = optimization_target_model_task_runner_.find(target);
  if (it != optimization_target_model_task_runner_.end() && it->second) {
    return it->second;
  }
  return default_model_task_runner_;
}

base::WeakPtr<PredictionModelComponentUpdateListener>
PredictionModelComponentUpdateListener::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

const ModelInfo*
PredictionModelComponentUpdateListener::GetModelForTesting(  // IN-TEST
    proto::OptimizationTarget target) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registry_.GetModel(target);
}

}  // namespace optimization_guide
