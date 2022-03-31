// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace autofill_assistant {
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

// The maximum number of pending model requests allowed to be kept by the
// AnnotateDomModelService.
constexpr int kMaxPendingRequestsAllowed = 100;

}  // namespace

AnnotateDomModelService::AnnotateDomModelService(
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide), background_task_runner_(background_task_runner) {
  if (opt_guide_) {
    opt_guide_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::OPTIMIZATION_TARGET_AUTOFILL_ASSISTANT,
        /* model_metadata= */ absl::nullopt, this);
  }
}

AnnotateDomModelService::~AnnotateDomModelService() = default;

void AnnotateDomModelService::Shutdown() {
  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_AUTOFILL_ASSISTANT, this);
  opt_guide_ = nullptr;

  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
  if (annotate_dom_model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*annotate_dom_model_file_)));
  }
  for (auto& pending_request : pending_model_requests_) {
    // Clear any pending requests, no model file is acceptable as |Shutdown| is
    // happening.
    std::move(pending_request).Run(false);
  }
  pending_model_requests_.clear();
}

void AnnotateDomModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_AUTOFILL_ASSISTANT) {
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info.GetModelFilePath()),
      base::BindOnce(&AnnotateDomModelService::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AnnotateDomModelService::OnModelFileLoaded(base::File model_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!model_file.IsValid()) {
    return;
  }

  if (annotate_dom_model_file_) {
    // If the model file is already loaded, it should be closed on a background
    // thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*annotate_dom_model_file_)));
  }
  annotate_dom_model_file_ = std::move(model_file);
  for (auto& pending_request : pending_model_requests_) {
    if (!pending_request) {
      continue;
    }
    std::move(pending_request).Run(true);
  }
  pending_model_requests_.clear();
}

absl::optional<base::File> AnnotateDomModelService::GetModelFile() {
  if (!annotate_dom_model_file_) {
    return absl::nullopt;
  }
  // The model must be valid at this point.
  DCHECK(annotate_dom_model_file_->IsValid());
  return annotate_dom_model_file_->Duplicate();
}

void AnnotateDomModelService::NotifyOnModelFileAvailable(
    NotifyModelAvailableCallback callback) {
  DCHECK(!annotate_dom_model_file_);
  if (pending_model_requests_.size() < kMaxPendingRequestsAllowed) {
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}

void AnnotateDomModelService::SetModelFileForTest(base::File model_file) {
  annotate_dom_model_file_ = std::move(model_file);
  for (auto& pending_request : pending_model_requests_) {
    std::move(pending_request).Run(true);
  }
  pending_model_requests_.clear();
}

}  // namespace autofill_assistant
