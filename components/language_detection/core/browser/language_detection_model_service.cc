// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/browser/language_detection_model_service.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

// Load the model file at the provided file path.
base::File LoadModelFile(const base::FilePath& model_file_path) {
  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}

void PostGetModelCallback(
    language_detection::LanguageDetectionModelService::GetModelCallback
        callback,
    base::File file) {
  // Posts to the same task runner as PostTaskAndReply.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](language_detection::LanguageDetectionModelService::GetModelCallback
                 callback,
             base::File file) { std::move(callback).Run(std::move(file)); },
          std::move(callback), std::move(file)));
}

// Util class for recording the result of loading the detection model. The
// result is recorded when it goes out of scope and its destructor is called.
class ScopedModelLoadingResultRecorder {
 public:
  ScopedModelLoadingResultRecorder() = default;
  ~ScopedModelLoadingResultRecorder() {
    UMA_HISTOGRAM_BOOLEAN(
        "TranslateModelService.LanguageDetectionModel.WasLoaded", was_loaded_);
  }

  void set_was_loaded() { was_loaded_ = true; }

 private:
  bool was_loaded_ = false;
};

}  // namespace

namespace language_detection {

LanguageDetectionModelService::LanguageDetectionModelService(
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide), background_task_runner_(background_task_runner) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/std::nullopt, this);
}

LanguageDetectionModelService::~LanguageDetectionModelService() {
  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION, this);
  // Clear any pending requests, no model file is acceptable as shutdown is
  // happening.
  NotifyModelUpdatesAndClear();
}

void LanguageDetectionModelService::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
  UnloadModelFile();
  // Clear any pending requests, no model file is acceptable as shutdown is
  // happening.
  NotifyModelUpdatesAndClear();
}

void LanguageDetectionModelService::UnloadModelFile() {
  if (language_detection_model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile,
                                  std::move(*language_detection_model_file_)));
  } else {
    language_detection_model_file_ = base::File();
  }
}

void LanguageDetectionModelService::NotifyModelUpdatesAndClear() {
  // We should always have a (possibly invalid) file by now.
  CHECK(language_detection_model_file_);
  for (auto& pending_request : pending_model_requests_) {
    PostGetModelCallback(std::move(pending_request),
                         language_detection_model_file_->Duplicate());
  }
  pending_model_requests_.clear();
}

void LanguageDetectionModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION) {
    return;
  }
  if (!model_info.has_value()) {
    UnloadModelFile();
    NotifyModelUpdatesAndClear();
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info->GetModelFilePath()),
      base::BindOnce(&LanguageDetectionModelService::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LanguageDetectionModelService::OnModelFileLoaded(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedModelLoadingResultRecorder result_recorder;
  if (!model_file.IsValid()) {
    return;
  }

  UnloadModelFile();
  language_detection_model_file_ = std::move(model_file);
  result_recorder.set_was_loaded();
  NotifyModelUpdatesAndClear();
}

void LanguageDetectionModelService::GetLanguageDetectionModelFile(
    GetModelCallback callback) {
  if (language_detection_model_file_.has_value()) {
    PostGetModelCallback(std::move(callback),
                         language_detection_model_file_->Duplicate());
    return;
  } else if (pending_model_requests_.size() < kMaxPendingRequestsAllowed) {
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }

  PostGetModelCallback(std::move(callback), base::File());
}
}  // namespace language_detection
