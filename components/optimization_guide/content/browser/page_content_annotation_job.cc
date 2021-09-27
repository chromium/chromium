// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotation_job.h"

#include "base/check_op.h"

namespace optimization_guide {

PageContentAnnotationJob::PageContentAnnotationJob(const std::string& input,
                                                   AnnotationType type)
    : input_(input), type_(type) {}

PageContentAnnotationJob::~PageContentAnnotationJob() = default;

void PageContentAnnotationJob::SetStatus(ExecutionStatus status) {
  // TODO(crbug/1249632): Consider adding a DCHECK here to check if the state
  // transition is correct.
  status_ = status;
}

void PageContentAnnotationJob::SetPageTopicsOutput(
    const std::vector<WeightedString>& page_topics) {
  DCHECK_EQ(AnnotationType::kPageTopics, type_);
  page_topics_ =
      std::vector<WeightedString>{page_topics.begin(), page_topics.end()};
}

void PageContentAnnotationJob::SetPageEntitiesOutput(
    const std::vector<WeightedString>& page_entities) {
  DCHECK_EQ(AnnotationType::kPageEntities, type_);
  page_entities_ =
      std::vector<WeightedString>{page_entities.begin(), page_entities.end()};
}

void PageContentAnnotationJob::SetVisibilityScoreOutput(
    double visibility_score) {
  DCHECK_EQ(AnnotationType::kContentVisibility, type_);
  visibility_score_ = visibility_score;
}

}  // namespace optimization_guide
