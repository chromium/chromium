// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotation_job.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"

namespace page_content_annotations {

PageContentAnnotationJob::PageContentAnnotationJob(
    BatchAnnotationCallback on_complete_callback,
    const std::vector<std::string>& inputs,
    AnnotationType type)
    : on_complete_callback_(std::move(on_complete_callback)),
      type_(type),
      inputs_(inputs.begin(), inputs.end()),
      job_creation_time_(base::TimeTicks::Now()) {
  DCHECK(!inputs_.empty());

  // Allow the results to be populated in any order by filling the output vector
  // with placeholder objects.
  results_.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); i++) {
    results_.push_back(
        BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string()));
  }
}

PageContentAnnotationJob::~PageContentAnnotationJob() {
  if (!job_execution_start_time_)
    return;

  base::TimeDelta job_scheduling_wait_time =
      *job_execution_start_time_ - job_creation_time_;
  base::TimeDelta job_exec_time =
      base::TimeTicks::Now() - *job_execution_start_time_;

  base::UmaHistogramMediumTimes(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime." +
          AnnotationTypeToString(type()),
      job_exec_time);

  base::UmaHistogramMediumTimes(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime." +
          AnnotationTypeToString(type()),
      job_scheduling_wait_time);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess." +
          AnnotationTypeToString(type()),
      HadAnySuccess());
}

void PageContentAnnotationJob::FillWithNullOutputs() {
  size_t remaining = CountOfRemainingNonNullInputs();
  for (size_t i = 0; i < remaining; i++) {
    std::string input = *GetNextInput();
    switch (type()) {
      case AnnotationType::kContentVisibility:
        PostNewResult(BatchAnnotationResult::CreateContentVisibilityResult(
                          input, std::nullopt),
                      i);
        break;
      case AnnotationType::kDeprecatedTextEmbedding:
      case AnnotationType::kDeprecatedPageEntities:
      case AnnotationType::kUnknown:
        NOTREACHED_IN_MIGRATION();
        PostNewResult(
            BatchAnnotationResult::CreateEmptyAnnotationsResult(input), i);
        break;
    }
  }
}

void PageContentAnnotationJob::OnComplete() {
  DCHECK(inputs_.empty());
  if (!on_complete_callback_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::move(on_complete_callback_).Run(results_);
}

size_t PageContentAnnotationJob::CountOfRemainingNonNullInputs() const {
  return inputs_.size();
}

std::optional<std::string> PageContentAnnotationJob::GetNextInput() {
  if (!job_execution_start_time_) {
    job_execution_start_time_ = base::TimeTicks::Now();
  }

  if (inputs_.empty()) {
    return std::nullopt;
  }
  std::string next = inputs_.front();
  inputs_.erase(inputs_.begin());
  return next;
}

void PageContentAnnotationJob::PostNewResult(
    const BatchAnnotationResult& result,
    size_t index) {
  results_[index] = result;
}

bool PageContentAnnotationJob::HadAnySuccess() const {
  for (const BatchAnnotationResult& result : results_) {
    if (result.type() == AnnotationType::kContentVisibility &&
        result.visibility_score()) {
      return true;
    }
  }
  return false;
}

}  // namespace page_content_annotations
