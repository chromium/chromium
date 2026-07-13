// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_UPDATE_LISTENER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_UPDATE_LISTENER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Tracks component updater updates for prediction models, loads them in the
// background, and notifies observers via OptimizationGuideModelProvider
// interface.
//
// This class is not thread-safe and all its methods must be called on the
// same sequence (typically the UI thread).
class PredictionModelComponentUpdateListener
    : public OptimizationGuideModelProvider {
 public:
  using RegisterComponentCallback = base::RepeatingCallback<void(
      proto::OptimizationTarget,
      base::WeakPtr<PredictionModelComponentUpdateListener>)>;

  PredictionModelComponentUpdateListener(
      OptimizationGuideModelProvider& fallback_provider,
      RegisterComponentCallback register_component_callback);
  ~PredictionModelComponentUpdateListener() override;

  PredictionModelComponentUpdateListener(
      const PredictionModelComponentUpdateListener&) = delete;
  PredictionModelComponentUpdateListener& operator=(
      const PredictionModelComponentUpdateListener&) = delete;

  // OptimizationGuideModelProvider implementation:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override;
  void SetModelDownloadSchedulingParams(
      proto::OptimizationTarget optimization_target,
      const download::SchedulingParams& params) override;

  // Called by the component installer policy when a component is ready.
  void MaybeUpdateModel(proto::OptimizationTarget target,
                        const base::Version& version,
                        const base::FilePath& install_dir);

  // Called by the component installer policy when a component is uninstalled.
  void OnModelUninstalled(proto::OptimizationTarget target);

  // Returns the current model for `target` if available.
  // For testing only.
  const ModelInfo* GetModelForTesting(proto::OptimizationTarget target) const;

  base::WeakPtr<PredictionModelComponentUpdateListener> GetWeakPtr();

 private:
  // Callback when model loading completes on the background thread.
  void OnModelLoaded(proto::OptimizationTarget target,
                     const base::Version& version,
                     std::optional<ModelInfo> model_info);

  // Returns the task runner to use for loading models for `target`.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner(
      proto::OptimizationTarget target);

  struct ComponentInfo {
    base::Version version;
    base::FilePath install_dir;
  };

  SEQUENCE_CHECKER(sequence_checker_);

  // Registry to manage observers and current models.
  ModelProviderRegistry registry_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Default background thread where file processing (loading) is performed
  // if no observer has registered a specific task runner yet.
  const scoped_refptr<base::SequencedTaskRunner> default_model_task_runner_;

  // Tracks the task runner provided by the first registered observer for each
  // target.
  base::flat_map<proto::OptimizationTarget,
                 scoped_refptr<base::SequencedTaskRunner>>
      optimization_target_model_task_runner_
          GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the expected version/path for each target.
  base::flat_map<proto::OptimizationTarget, ComponentInfo> component_info_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Returns true if the target is migrated to component delivery.
  bool IsMigrated(proto::OptimizationTarget optimization_target) const;

  const raw_ref<OptimizationGuideModelProvider> fallback_provider_;

  RegisterComponentCallback register_component_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<proto::OptimizationTarget> registered_targets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PredictionModelComponentUpdateListener>
      weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_UPDATE_LISTENER_H_
