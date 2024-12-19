// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/browser/language_detection_model_provider.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"

namespace {

// Load the model file at the provided file path.
base::File LoadModelFile(const base::FilePath& model_file_path) {
  base::File file(model_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "While opening " << model_file_path << ": "
               << file.error_details();
  }
  return file;
}

void PostGetModelCallback(
    language_detection::LanguageDetectionModelProvider::GetModelCallback
        callback,
    base::File file) {
  // Posts to the same task runner as PostTaskAndReply.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](language_detection::LanguageDetectionModelProvider::
                 GetModelCallback callback,
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

LanguageDetectionModelProvider::LanguageDetectionModelProvider(
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : language_detection_model_file_(background_task_runner) {}

LanguageDetectionModelProvider::~LanguageDetectionModelProvider() = default;

void LanguageDetectionModelProvider::UnloadModelFile() {
  language_detection_model_file_.InvalidateFile();
  OnModelFileChangedInternal();
}

void LanguageDetectionModelProvider::OnModelFileChangedInternal() {
  has_model_ever_been_set_ = true;

  for (auto& pending_request : pending_model_requests_) {
    PostGetModelCallback(std::move(pending_request),
                         language_detection_model_file_.GetFile().Duplicate());
  }
  pending_model_requests_.clear();
}

void LanguageDetectionModelProvider::ReplaceModelFile(base::FilePath path) {
  language_detection_model_file_.ReplaceFile(
      base::BindOnce(&LoadModelFile, path),
      base::BindOnce(&LanguageDetectionModelProvider::ModelFileReplacedCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LanguageDetectionModelProvider::ModelFileReplacedCallback() {
  ScopedModelLoadingResultRecorder result_recorder;
  if (!language_detection_model_file_.GetFile().IsValid()) {
    return;
  }

  result_recorder.set_was_loaded();
  OnModelFileChangedInternal();
}

void LanguageDetectionModelProvider::GetLanguageDetectionModelFile(
    GetModelCallback callback) {
  if (has_model_ever_been_set_) {
    PostGetModelCallback(std::move(callback),
                         language_detection_model_file_.GetFile().Duplicate());
    return;
  } else if (pending_model_requests_.size() < kMaxPendingRequestsAllowed) {
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }

  PostGetModelCallback(std::move(callback), base::File());
}

bool LanguageDetectionModelProvider::HasValidModelFile() {
  if (!has_model_ever_been_set_) {
    return false;
  }
  return language_detection_model_file_.GetFile().IsValid();
}

}  // namespace language_detection
