// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/translate_model_service.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/post_task.h"
#include "components/optimization_guide/content/optimization_guide_decider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

// Load the model file at the provided file path.
base::File LoadModelFile(const base::FilePath& model_file_path) {
  if (!base::PathExists(model_file_path))
    return base::File();

  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

}  // namespace

namespace translate {

TranslateModelService::TranslateModelService(
    optimization_guide::OptimizationGuideDecider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide), background_task_runner_(background_task_runner) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION, this);
}

TranslateModelService::~TranslateModelService() = default;

void TranslateModelService::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
}

void TranslateModelService::OnModelFileUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION) {
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, file_path),
      base::BindOnce(&TranslateModelService::OnModelFileLoaded,
                     base::Unretained(this)));
}

void TranslateModelService::OnModelFileLoaded(base::File model_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!model_file.IsValid()) {
    // TODO(crbug.com/1157661): add histogram to log the model failed to load.
    LOCAL_HISTOGRAM_BOOLEAN(
        "TranslateModelService.LanguageDetectionModel.WasValid", false);
    return;
  }

  language_detection_model_file_ = std::move(model_file);
  LOCAL_HISTOGRAM_BOOLEAN(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true);
  for (auto& pending_request : pending_model_requests_) {
    std::move(pending_request).Run(language_detection_model_file_->Duplicate());
  }
}

void TranslateModelService::GetLanguageDetectionModelFile(
    GetModelCallback callback) {
  if (!language_detection_model_file_) {
    // TODO(crbug.com/1157661): add histogram record the number of callbacks
    // held.
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }
  // The model must be valid at this point.
  DCHECK(language_detection_model_file_->IsValid());
  std::move(callback).Run(language_detection_model_file_->Duplicate());
}

}  // namespace translate
