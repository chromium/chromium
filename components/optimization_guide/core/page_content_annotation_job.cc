// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_content_annotation_job.h"

#include "base/check_op.h"

namespace optimization_guide {

PageContentAnnotationJob::PageContentAnnotationJob(
    BatchAnnotationCallback on_complete_callback,
    const std::vector<std::string>& inputs,
    AnnotationType type)
    : on_complete_callback_(std::move(on_complete_callback)),
      type_(type),
      inputs_(inputs.begin(), inputs.end()) {
  DCHECK(!inputs_.empty());
}

PageContentAnnotationJob::~PageContentAnnotationJob() = default;

void PageContentAnnotationJob::FillWithNullOutputs() {
  while (auto input = GetNextInput()) {
    switch (type()) {
      case AnnotationType::kPageTopics:
        PostNewResult(BatchAnnotationResult::CreatePageTopicsResult(
            *input, absl::nullopt));
        break;
      case AnnotationType::kPageEntities:
        PostNewResult(BatchAnnotationResult::CreatePageEntitiesResult(
            *input, absl::nullopt));
        break;
      case AnnotationType::kContentVisibility:
        PostNewResult(BatchAnnotationResult::CreateContentVisibilityResult(
            *input, absl::nullopt));
        break;
      case AnnotationType::kUnknown:
        NOTREACHED();
        PostNewResult(
            BatchAnnotationResult::CreateEmptyAnnotationsResult(*input));
        break;
    }
  }
}

void PageContentAnnotationJob::OnComplete() {
  DCHECK(inputs_.empty());
  if (!on_complete_callback_) {
    NOTREACHED();
    return;
  }

  std::move(on_complete_callback_).Run(results_);
}

size_t PageContentAnnotationJob::CountOfRemainingNonNullInputs() const {
  return inputs_.size();
}

absl::optional<std::string> PageContentAnnotationJob::GetNextInput() {
  if (inputs_.empty()) {
    return absl::nullopt;
  }
  std::string next = inputs_.front();
  inputs_.erase(inputs_.begin());
  return next;
}

void PageContentAnnotationJob::PostNewResult(
    const BatchAnnotationResult& result) {
  results_.push_back(result);
}

}  // namespace optimization_guide
