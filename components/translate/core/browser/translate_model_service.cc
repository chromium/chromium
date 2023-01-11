// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_model_service.h"

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
  if (!base::PathExists(model_file_path))
    return base::File();

  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid())
    return;
  model_file.Close();
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

// The maximum number of pending model requests allowed to be kept
// by the TranslateModelService.
constexpr int kMaxPendingRequestsAllowed = 100;

}  // namespace

namespace translate {

TranslateModelService::TranslateModelService(
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide), background_task_runner_(background_task_runner) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/absl::nullopt, this);
}

TranslateModelService::~TranslateModelService() {
  for (auto& pending_request : pending_model_requests_) {
    // Clear any pending requests, no model file is acceptable as shutdown is
    // happening.
    std::move(pending_request).Run(false);
  }
  pending_model_requests_.clear();
}

void TranslateModelService::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
  if (language_detection_model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile,
                                  std::move(*language_detection_model_file_)));
  }
  for (auto& pending_request : pending_model_requests_) {
    // Clear any pending requests, no model file is acceptable as shutdown is
    // happening.
    std::move(pending_request).Run(false);
  }
  pending_model_requests_.clear();
}

void TranslateModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION) {
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info.GetModelFilePath()),
      base::BindOnce(&TranslateModelService::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TranslateModelService::OnModelFileLoaded(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedModelLoadingResultRecorder result_recorder;
  if (!model_file.IsValid())
    return;

  if (language_detection_model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile,
                                  std::move(*language_detection_model_file_)));
  }
  language_detection_model_file_ = std::move(model_file);
  result_recorder.set_was_loaded();
  UMA_HISTOGRAM_COUNTS_100(
      "TranslateModelService.LanguageDetectionModel.PendingRequestCallbacks",
      pending_model_requests_.size());
  for (auto& pending_request : pending_model_requests_) {
    if (!pending_request) {
      continue;
    }
    std::move(pending_request).Run(true);
  }
  pending_model_requests_.clear();
}

base::File TranslateModelService::GetLanguageDetectionModelFile() {
  DCHECK(IsModelAvailable());
  if (!language_detection_model_file_) {
    return base::File();
  }
  // The model must be valid at this point.
  DCHECK(language_detection_model_file_->IsValid());
  return language_detection_model_file_->Duplicate();
}

void TranslateModelService::NotifyOnModelFileAvailable(
    NotifyModelAvailableCallback callback) {
  DCHECK(!IsModelAvailable());
  if (pending_model_requests_.size() < kMaxPendingRequestsAllowed) {
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}
}  // namespace translate
