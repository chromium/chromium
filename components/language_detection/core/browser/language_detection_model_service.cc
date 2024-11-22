// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/browser/language_detection_model_service.h"

#include "components/language_detection/core/browser/language_detection_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace language_detection {

LanguageDetectionModelService::LanguageDetectionModelService(
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : LanguageDetectionModelProvider(background_task_runner),
      opt_guide_(opt_guide) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/std::nullopt, this);
}

LanguageDetectionModelService::~LanguageDetectionModelService() {
  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION, this);
  Shutdown();
}

void LanguageDetectionModelService::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
  UnloadModelFile();
}

void LanguageDetectionModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION) {
    return;
  }
  if (!model_info.has_value()) {
    // Start returning the invalid file as the model has been explicitly made
    // unavailable.
    UnloadModelFile();
    return;
  }
  ReplaceModelFile(model_info->GetModelFilePath());
}

}  // namespace language_detection
