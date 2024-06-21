// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/test_page_content_annotator.h"

#include "base/task/sequenced_task_runner.h"

namespace page_content_annotations {

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

  if (annotation_type == AnnotationType::kContentVisibility) {
    for (const std::string& input : inputs) {
      auto it = visibility_scores_for_input_.find(input);
      std::optional<double> output;
      if (it != visibility_scores_for_input_.end()) {
        output = it->second;
      }
      results.emplace_back(
          BatchAnnotationResult::CreateContentVisibilityResult(input, output));
    }
  }

  // The annotations model usually runs in bg thread and callbacks are run
  // async.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

void TestPageContentAnnotator::SetAlwaysHang(bool hang) {
  always_hang_ = hang;
}

std::optional<optimization_guide::ModelInfo>
TestPageContentAnnotator::GetModelInfoForType(
    AnnotationType annotation_type) const {
  if (annotation_type == AnnotationType::kContentVisibility) {
    return visibility_scores_model_info_;
  }

  return std::nullopt;
}

void TestPageContentAnnotator::UseVisibilityScores(
    const std::optional<optimization_guide::ModelInfo>& model_info,
    const base::flat_map<std::string, double>& visibility_scores_for_input) {
  visibility_scores_model_info_ = model_info;
  visibility_scores_for_input_ = visibility_scores_for_input;
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

}  // namespace page_content_annotations
