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
  std::vector<BatchAnnotationResult> results;

  if (annotation_type == AnnotationType::kPageTopics) {
    for (const std::string& input : inputs) {
      auto it = topics_by_input_.find(input);
      absl::optional<std::vector<WeightedIdentifier>> output;
      if (it != topics_by_input_.end()) {
        output = it->second;
      }
      results.emplace_back(
          BatchAnnotationResult::CreatePageTopicsResult(input, output));
    }
  }

  if (annotation_type == AnnotationType::kPageEntities) {
    for (const std::string& input : inputs) {
      auto it = entities_by_input_.find(input);
      absl::optional<std::vector<ScoredEntityMetadata>> output;
      if (it != entities_by_input_.end()) {
        output = it->second;
      }
      results.emplace_back(
          BatchAnnotationResult::CreatePageEntitiesResult(input, output));
    }
  }

  if (annotation_type == AnnotationType::kContentVisibility) {
    for (const std::string& input : inputs) {
      auto it = visibility_scores_for_input_.find(input);
      absl::optional<double> output;
      if (it != visibility_scores_for_input_.end()) {
        output = it->second;
      }
      results.emplace_back(
          BatchAnnotationResult::CreateContentVisibilityResult(input, output));
    }
  }

  std::move(callback).Run(results);
}

void TestPageContentAnnotator::UsePageTopics(
    const base::flat_map<std::string, std::vector<WeightedIdentifier>>&
        topics_by_input) {
  topics_by_input_ = topics_by_input;
}

void TestPageContentAnnotator::UsePageEntities(
    const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
        entities_by_input) {
  entities_by_input_ = entities_by_input;
}

void TestPageContentAnnotator::UseVisibilityScores(
    const base::flat_map<std::string, double>& visibility_scores_for_input) {
  visibility_scores_for_input_ = visibility_scores_for_input;
}

}  // namespace optimization_guide
