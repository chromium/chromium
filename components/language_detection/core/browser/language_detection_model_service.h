// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language_detection/core/background_file.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace language_detection {

// The maximum number of pending model requests allowed to be kept
// by the LanguageDetectionModelService.
inline constexpr int kMaxPendingRequestsAllowed = 100;

// Service that manages models required to support language detection in the
// browser. Currently, the service should only be used in the browser as it
// relies on the Optimization Guide.
// TODO(crbug.com/40225076): LanguageDetectionModelService should own
// LanguageDetectionModel.
class LanguageDetectionModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using GetModelCallback = base::OnceCallback<void(base::File)>;

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

  // Provides the language detection model file. It will asynchronously call
  // `callback` with the file when availability is known. The callback is always
  // asynchronous, even if the model is already available. If the model is
  // definiteively unavailable or if too many calls to this are
  // pending, the provided file will be the invalid `base::File()`.
  void GetLanguageDetectionModelFile(GetModelCallback callback);

 private:
  // Unloads the model in a background task. This does not set
  // `has_model_ever_been_set_ = false`. After this any requests for the model
  // file will immediately receive an invalid file, until an update with a valid
  // file occurs.
  void UnloadModelFile();

  // Replaces the current model file with a new one. It is careful to open/close
  // files as necessary on a background thread.
  void UpdateModelFile(base::File model_file);

  // Called after the model file changes. It records the fact that the model has
  // been changed, notifies observers, and clears the observer list.
  void OnModelFileChangedInternal();

  // For use with `BackgroundFile::ReplaceFile`.
  void ModelFileReplacedCallback();

  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;

  // The file that contains the language detection model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  BackgroundFile language_detection_model_file_;
  // Records whether we have ever explicitly set the model file (including to an
  // invalid value). Until this becomes true, requests for the file will be
  // queued.
  bool has_model_ever_been_set_ = false;

  // The set of callbacks associated with requests for the language detection
  // model. The callback notifies requesters than the model file is now
  // available and can be safely requested.
  std::vector<GetModelCallback> pending_model_requests_;

  base::WeakPtrFactory<LanguageDetectionModelService> weak_ptr_factory_{this};
};

}  //  namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
