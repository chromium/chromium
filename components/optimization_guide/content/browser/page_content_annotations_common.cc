// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_common.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace optimization_guide {

WeightedString::WeightedString(const std::string& value, double weight)
    : value_(value), weight_(weight) {
  DCHECK_GE(weight_, 0.0);
  DCHECK_LE(weight_, 1.0);
}
WeightedString::WeightedString(const WeightedString&) = default;
WeightedString::~WeightedString() = default;

bool WeightedString::operator==(const WeightedString& other) const {
  return this->value_ == other.value_ && this->weight_ == other.weight_;
}

BatchAnnotationResult::BatchAnnotationResult() = default;
BatchAnnotationResult::BatchAnnotationResult(const BatchAnnotationResult&) =
    default;
BatchAnnotationResult::~BatchAnnotationResult() = default;

// static
BatchAnnotationResult BatchAnnotationResult::CreatePageTopicsResult(
    const std::string& input,
    ExecutionStatus status,
    absl::optional<std::vector<WeightedString>> topics) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.status_ = status;
  result.topics_ = topics;
  result.type_ = AnnotationType::kPageTopics;
  return result;
}

//  static
BatchAnnotationResult BatchAnnotationResult::CreatePageEntitiesResult(
    const std::string& input,
    ExecutionStatus status,
    absl::optional<std::vector<WeightedString>> entities) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.status_ = status;
  result.entities_ = entities;
  result.type_ = AnnotationType::kPageEntities;
  return result;
}

//  static
BatchAnnotationResult BatchAnnotationResult::CreateContentVisibilityResult(
    const std::string& input,
    ExecutionStatus status,
    absl::optional<double> visibility_score) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.status_ = status;
  result.visibility_score_ = visibility_score;
  result.type_ = AnnotationType::kContentVisibility;
  return result;
}

// static
BatchAnnotationResult BatchAnnotationResult::CreateEmptyAnnotationsResult(
    const std::string& input,
    ExecutionStatus status) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.status_ = status;
  return result;
}

bool BatchAnnotationResult::operator==(
    const BatchAnnotationResult& other) const {
  return this->input_ == other.input_ && this->status_ == other.status_ &&
         this->type_ == other.type_ && this->topics_ == other.topics_ &&
         this->entities_ == other.entities_ &&
         this->visibility_score_ == other.visibility_score_;
}

std::vector<BatchAnnotationResult> CreateEmptyBatchAnnotationResultsWithStatus(
    const std::vector<std::string>& inputs,
    ExecutionStatus status) {
  std::vector<BatchAnnotationResult> results;
  results.reserve(inputs.size());
  for (const std::string& input : inputs) {
    results.emplace_back(
        BatchAnnotationResult::CreateEmptyAnnotationsResult(input, status));
  }
  return results;
}

}  // namespace optimization_guide
