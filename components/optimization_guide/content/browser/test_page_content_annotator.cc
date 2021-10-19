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
  if (status_ != ExecutionStatus::kSuccess) {
    std::move(callback).Run(
        CreateEmptyBatchAnnotationResultsWithStatus(inputs, status_));
    return;
  }

  std::vector<BatchAnnotationResult> results;

  if (annotation_type == AnnotationType::kPageTopics) {
    for (const std::string& input : inputs) {
      auto it = topics_by_input_.find(input);
      absl::optional<std::vector<WeightedString>> output;
      if (it != topics_by_input_.end()) {
        output = it->second;
      }
      results.emplace_back(BatchAnnotationResult::CreatePageTopicsResult(
          input, status_, output));
    }
  }

  if (annotation_type == AnnotationType::kPageEntities) {
    for (const std::string& input : inputs) {
      auto it = entities_by_input_.find(input);
      absl::optional<std::vector<WeightedString>> output;
      if (it != entities_by_input_.end()) {
        output = it->second;
      }
      results.emplace_back(BatchAnnotationResult::CreatePageEntitiesResult(
          input, status_, output));
    }
  }

  if (annotation_type == AnnotationType::kContentVisibility) {
    for (const std::string& input : inputs) {
      auto it = visibility_scores_for_input_.find(input);
      absl::optional<double> output;
      if (it != visibility_scores_for_input_.end()) {
        output = it->second;
      }
      results.emplace_back(BatchAnnotationResult::CreateContentVisibilityResult(
          input, status_, output));
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
  topics_by_input_ = topics_by_input;
}

void TestPageContentAnnotator::UsePageEntities(
    const base::flat_map<std::string, std::vector<WeightedString>>&
        entities_by_input) {
  entities_by_input_ = entities_by_input;
}

void TestPageContentAnnotator::UseVisibilityScores(
    const base::flat_map<std::string, double>& visibility_scores_for_input) {
  visibility_scores_for_input_ = visibility_scores_for_input;
}

}  // namespace optimization_guide
