// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_

#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/optimization_target_model_observer.h"

namespace base {
class File;
class FilePath;
}  // namespace base

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace translate {

// Service that manages models required to support translation in the browser.
// Currently, the service should only be used in the browser as it relies on
// the Optimization Guide.
class TranslateModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  explicit TranslateModelService(
      optimization_guide::OptimizationGuideDecider* opt_guide);
  ~TranslateModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelFileUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& file_path) override;

  // Returns a loaded file containing the TFLite model capable of detecting the
  // language of a web page's text.
  base::Optional<base::File> GetLanguageDetectionModelFile();

 private:
  // Optimization Guide Service that provides model files for this
  // service. Optimization Guide Service is a
  // BrowserContextKeyedServiceFactory and should not
  // be used after ShutDown.
  optimization_guide::OptimizationGuideDecider* opt_guide_;
};

}  //  namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_
