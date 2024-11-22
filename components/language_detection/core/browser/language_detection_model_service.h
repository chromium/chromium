// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language_detection/core/browser/language_detection_model_provider.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace language_detection {

// A `LanguageDetectionModelProvider` that receives its file from the
// Optimization Guide service.
// TODO(crbug.com/40225076): LanguageDetectionModelService should own
// LanguageDetectionModel.
class LanguageDetectionModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver,
      public LanguageDetectionModelProvider {
 public:
  LanguageDetectionModelService(
      optimization_guide::OptimizationGuideModelProvider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~LanguageDetectionModelService() override;

  // KeyedService implementation:
  // Clear any pending requests and unload the model file as shutdown is
  // happening.
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation.
  // Called when a new model file is available.
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

 private:
  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;
};

}  //  namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
