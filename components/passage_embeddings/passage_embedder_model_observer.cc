// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embedder_model_observer.h"

#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/public/background_service/download_params.h"
#endif

namespace passage_embeddings {

PassageEmbedderModelObserver::PassageEmbedderModelObserver(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    PassageEmbeddingsServiceController* service_controller)
    : model_provider_(model_provider),
      service_controller_(service_controller),
      target_(optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER) {
  if (model_provider_) {
#if BUILDFLAG(IS_ANDROID)
    download::SchedulingParams scheduling_params;
    scheduling_params.priority = download::SchedulingParams::Priority::HIGH;
    scheduling_params.network_requirements =
        download::SchedulingParams::NetworkRequirements::UNMETERED;
    scheduling_params.battery_requirements =
        download::SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
    model_provider_->SetModelDownloadSchedulingParams(target_,
                                                      scheduling_params);
#endif
    model_provider_->AddObserverForOptimizationTargetModel(
        target_,
        /*model_metadata=*/std::nullopt,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
        this);
  }
}

PassageEmbedderModelObserver::~PassageEmbedderModelObserver() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(target_, this);
  }
}

void PassageEmbedderModelObserver::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  VLOG(3) << "Model updated for target: " << optimization_target;
  if (optimization_target != target_) {
    return;
  }

  service_controller_->MaybeUpdateModelInfo(model_info);
}

}  // namespace passage_embeddings
