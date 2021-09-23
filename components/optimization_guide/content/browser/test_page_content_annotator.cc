// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/test_page_content_annotator.h"

namespace optimization_guide {

TestPageContentAnnotator::~TestPageContentAnnotator() = default;
TestPageContentAnnotator::TestPageContentAnnotator() = default;

void TestPageContentAnnotator::Annotate(BatchAnnotationCallback callback,
                                        const std::vector<std::string>& inputs,
                                        AnnotationType annotation_type) {
  std::vector<BatchAnnotationResult> results =
      FillResultWithStatus(inputs, status_);
  if (status_ != ExecutionStatus::kSuccess) {
    std::move(callback).Run(results);
    return;
  }

  if (annotation_type == AnnotationType::kPageTopics) {
    for (BatchAnnotationResult& result : results) {
      auto it = topics_by_input_.find(result.input);
      if (it != topics_by_input_.end()) {
        result.topics =
            std::vector<WeightedString>{it->second.front(), it->second.back()};
      }
    }
  }

  if (annotation_type == AnnotationType::kPageEntities) {
    for (BatchAnnotationResult& result : results) {
      auto it = entities_by_input_.find(result.input);
      if (it != entities_by_input_.end()) {
        result.entites =
            std::vector<WeightedString>{it->second.front(), it->second.back()};
      }
    }
  }

  if (annotation_type == AnnotationType::kContentVisibility) {
    for (BatchAnnotationResult& result : results) {
      auto it = visibility_scores_for_input_.find(result.input);
      if (it != visibility_scores_for_input_.end()) {
        result.visibility_score = it->second;
      }
    }
  }

  std::move(callback).Run(results);
}

void TestPageContentAnnotator::UseExecutionStatus(ExecutionStatus status) {
  status_ = status;
}

void TestPageContentAnnotator::UsePageTopics(
    const base::flat_map<std::string, std::vector<WeightedString>>&
        topics_by_input) {
  topics_by_input_ = base::flat_map<std::string, std::vector<WeightedString>>{
      topics_by_input.begin(), topics_by_input.end()};
}

void TestPageContentAnnotator::UsePageEntities(
    const base::flat_map<std::string, std::vector<WeightedString>>&
        entities_by_input) {
  entities_by_input_ = base::flat_map<std::string, std::vector<WeightedString>>{
      entities_by_input.begin(), entities_by_input.end()};
}

void TestPageContentAnnotator::UseVisibilityScores(
    const base::flat_map<std::string, double>& visibility_scores_for_input) {
  visibility_scores_for_input_ = visibility_scores_for_input;
}

}  // namespace optimization_guide
