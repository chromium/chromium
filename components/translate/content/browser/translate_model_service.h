// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

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
  using GetModelCallback = base::OnceCallback<void(base::File)>;

  TranslateModelService(
      optimization_guide::OptimizationGuideDecider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~TranslateModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelFileUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& file_path) override;

  // Invokes |callback| with a language detection model file when it is
  // available.
  void GetLanguageDetectionModelFile(GetModelCallback callback);

 private:
  void OnModelFileLoaded(base::File model_file);

  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  optimization_guide::OptimizationGuideDecider* opt_guide_;

  // The file that contains the language detection model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  base::Optional<base::File> language_detection_model_file_;

  // The set of callbacks associated with requests for the language detection
  // model.
  std::vector<GetModelCallback> pending_model_requests_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  //  namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_MODEL_SERVICE_H_
