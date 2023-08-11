// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace translate {

// Service that manages models required to support translation in the browser.
// Currently, the service should only be used in the browser as it relies on
// the Optimization Guide.
// TODO(crbug/1324530): TranslateModelService should own
// LanguageDetectionModel.
class TranslateModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using GetModelCallback = base::OnceCallback<void(base::File)>;
  using NotifyModelAvailableCallback = base::OnceCallback<void(bool)>;

  TranslateModelService(
      optimization_guide::OptimizationGuideModelProvider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~TranslateModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // Returns the language detection model file, should only be called when the
  // is model file is already available. See the |NotifyOnModelFileAvailable|
  // for an asynchronous notification of the model being available.
  base::File GetLanguageDetectionModelFile();

  // Returns whether the language detection model is loaded and available to be
  // requested.
  bool IsModelAvailable() { return language_detection_model_file_.has_value(); }

  // If the model file is not available, requestors can ask to be notified, via
  // |callback|. This enables a two-step approach to relabily get the model file
  // when it becomes available if the requestor needs the file right when it
  // becomes available (e.g., the translate driver). This is to ensure that if
  // the callback becomes empty, only the notification gets dropped, rather than
  // the model file which has to be closed on a background thread.
  void NotifyOnModelFileAvailable(NotifyModelAvailableCallback callback);

 private:
  // Unloads the model in background task.
  void UnloadModelFile();

  // Notifies the model update to observers, and clears the observer list.
  void NotifyModelUpdatesAndClear(bool is_model_available);

  void OnModelFileLoaded(base::File model_file);

  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;

  // The file that contains the language detection model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  absl::optional<base::File> language_detection_model_file_;

  // The set of callbacks associated with requests for the language detection
  // model. The callback notifies requesters than the model file is now
  // available and can be safely requested.
  std::vector<NotifyModelAvailableCallback> pending_model_requests_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TranslateModelService> weak_ptr_factory_{this};
};

}  //  namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_
