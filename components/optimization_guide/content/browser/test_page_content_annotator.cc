// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/test_page_content_annotator.h"

namespace optimization_guide {

TestPageContentAnnotator::~TestPageContentAnnotator() = default;
TestPageContentAnnotator::TestPageContentAnnotator() = default;

void TestPageContentAnnotator::Annotate(BatchAnnotationCallback callback,
                                        const std::vector<std::string>& inputs,
                                        AnnotationType annotation_type) {
  annotation_requests_.emplace_back(std::make_pair(inputs, annotation_type));
  if (always_hang_) {
    return;
  }

  std::vector<BatchAnnotationResult> results;

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

  if (annotation_type == AnnotationType::kTextEmbedding) {
    for (const std::string& input : inputs) {
      auto it = text_embeddings_for_input_.find(input);
      absl::optional<std::vector<float>> output;
      if (it != text_embeddings_for_input_.end()) {
        output = it->second;
      }
      results.emplace_back(
          BatchAnnotationResult::CreateTextEmbeddingResult(input, output));
    }
  }

  std::move(callback).Run(results);
}

void TestPageContentAnnotator::SetAlwaysHang(bool hang) {
  always_hang_ = hang;
}

absl::optional<ModelInfo> TestPageContentAnnotator::GetModelInfoForType(
    AnnotationType annotation_type) const {
  if (annotation_type == AnnotationType::kPageEntities)
    return entities_model_info_;

  if (annotation_type == AnnotationType::kPageEntities)
    return visibility_scores_model_info_;

  return absl::nullopt;
}

void TestPageContentAnnotator::UsePageEntities(
    const absl::optional<ModelInfo>& model_info,
    const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
        entities_by_input) {
  entities_model_info_ = model_info;
  entities_by_input_ = entities_by_input;
}

void TestPageContentAnnotator::UseVisibilityScores(
    const absl::optional<ModelInfo>& model_info,
    const base::flat_map<std::string, double>& visibility_scores_for_input) {
  visibility_scores_model_info_ = model_info;
  visibility_scores_for_input_ = visibility_scores_for_input;
}

void TestPageContentAnnotator::UseTextEmbeddings(
    const absl::optional<ModelInfo>& model_info,
    const base::flat_map<std::string, std::vector<float>>&
        text_embeddings_for_input) {
  text_embeddings_model_info_ = model_info;
  text_embeddings_for_input_ = text_embeddings_for_input;
}

bool TestPageContentAnnotator::ModelRequestedForType(
    AnnotationType type) const {
  return model_requests_.contains(type);
}

void TestPageContentAnnotator::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
  model_requests_.insert(type);
  std::move(callback).Run(true);
}

}  // namespace optimization_guide
