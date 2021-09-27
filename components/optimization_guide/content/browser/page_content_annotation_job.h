// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_H_

#include <string>

#include "base/callback.h"
#include "components/optimization_guide/content/browser/page_content_annotations_common.h"

namespace optimization_guide {

// A single page content annotation job with all request and response data
// throughout the progression of the model execution.
class PageContentAnnotationJob {
 public:
  using OnJobCompleteCallback =
      base::OnceCallback<void(const PageContentAnnotationJob&)>;

  PageContentAnnotationJob(const std::string& input, AnnotationType type);
  ~PageContentAnnotationJob();

  // Updates the execution status.
  void SetStatus(ExecutionStatus status);

  // Sets the corresponding output.
  void SetPageTopicsOutput(const std::vector<WeightedString>& page_topics);
  void SetPageEntitiesOutput(const std::vector<WeightedString>& page_entities);
  void SetVisibilityScoreOutput(double visibility_score);

  std::string input() const { return input_; }
  AnnotationType type() const { return type_; }
  ExecutionStatus status() const { return status_; }
  absl::optional<std::vector<WeightedString>> page_topics() const {
    return page_topics_;
  }
  absl::optional<std::vector<WeightedString>> page_entities() const {
    return page_entities_;
  }
  absl::optional<double> visibility_score() const { return visibility_score_; }

  PageContentAnnotationJob(const PageContentAnnotationJob&) = delete;
  PageContentAnnotationJob& operator=(const PageContentAnnotationJob&) = delete;

 private:
  // Initial request data.
  const std::string input_;
  const AnnotationType type_;

  // The current execution status.
  ExecutionStatus status_ = ExecutionStatus::kUnknown;

  // The corresponding output. Only the output for |type_| will be set on a
  // successful execution.
  absl::optional<std::vector<WeightedString>> page_topics_;
  absl::optional<std::vector<WeightedString>> page_entities_;
  absl::optional<double> visibility_score_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_H_