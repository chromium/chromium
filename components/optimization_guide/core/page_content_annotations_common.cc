// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_content_annotations_common.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace optimization_guide {

std::string AnnotationTypeToString(AnnotationType type) {
  switch (type) {
    case AnnotationType::kUnknown:
      return "Unknown";
    case AnnotationType::kPageTopics:
      return "PageTopics";
    case AnnotationType::kContentVisibility:
      return "ContentVisibility";
    case AnnotationType::kPageEntities:
      return "PageEntities";
  }
}

WeightedString::WeightedString(const std::string& value, double weight)
    : value_(value), weight_(weight) {
  DCHECK_GE(weight_, 0.0);
  DCHECK_LE(weight_, 1.0);
}
WeightedString::WeightedString(const WeightedString&) = default;
WeightedString::~WeightedString() = default;

bool WeightedString::operator==(const WeightedString& other) const {
  constexpr double kWeightTolerance = 1e-6;
  return this->value_ == other.value_ &&
         abs(this->weight_ - other.weight_) <= kWeightTolerance;
}

std::string WeightedString::ToString() const {
  return base::StringPrintf("WeightedString{\"%s\",%f}", value().c_str(),
                            weight());
}

std::ostream& operator<<(std::ostream& stream, const WeightedString& ws) {
  stream << ws.ToString();
  return stream;
}

BatchAnnotationResult::BatchAnnotationResult() = default;
BatchAnnotationResult::BatchAnnotationResult(const BatchAnnotationResult&) =
    default;
BatchAnnotationResult::~BatchAnnotationResult() = default;

std::string BatchAnnotationResult::ToString() const {
  std::string output = "nullopt";
  if (topics_) {
    std::vector<std::string> all_weighted_strings;
    for (const WeightedString& ws : *topics_) {
      all_weighted_strings.push_back(ws.ToString());
    }
    output = "{" + base::JoinString(all_weighted_strings, ",") + "}";
  } else if (entities_) {
    std::vector<std::string> all_entities;
    for (const ScoredEntityMetadata& md : *entities_) {
      all_entities.push_back(md.ToString());
    }
    output = "{" + base::JoinString(all_entities, ",") + "}";
  } else if (visibility_score_) {
    output = base::NumberToString(*visibility_score_);
  }
  return base::StringPrintf(
      "BatchAnnotationResult{"
      "\"<input with length %zu>\", "
      "type: %s, "
      "output: %s}",
      input_.size(), AnnotationTypeToString(type_).c_str(), output.c_str());
}

std::ostream& operator<<(std::ostream& stream,
                         const BatchAnnotationResult& result) {
  stream << result.ToString();
  return stream;
}

// static
BatchAnnotationResult BatchAnnotationResult::CreatePageTopicsResult(
    const std::string& input,
    absl::optional<std::vector<WeightedString>> topics) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.topics_ = topics;
  result.type_ = AnnotationType::kPageTopics;

  // Always sort the result (if present) by the given score.
  if (result.topics_) {
    std::sort(result.topics_->begin(), result.topics_->end(),
              [](const WeightedString& a, const WeightedString& b) {
                return a.weight() < b.weight();
              });
  }

  return result;
}

//  static
BatchAnnotationResult BatchAnnotationResult::CreatePageEntitiesResult(
    const std::string& input,
    absl::optional<std::vector<ScoredEntityMetadata>> entities) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.entities_ = entities;
  result.type_ = AnnotationType::kPageEntities;

  // Always sort the result (if present) by the given score.
  if (result.entities_) {
    std::sort(result.entities_->begin(), result.entities_->end(),
              [](const ScoredEntityMetadata& a, const ScoredEntityMetadata& b) {
                return a.score < b.score;
              });
  }

  return result;
}

//  static
BatchAnnotationResult BatchAnnotationResult::CreateContentVisibilityResult(
    const std::string& input,
    absl::optional<double> visibility_score) {
  BatchAnnotationResult result;
  result.input_ = input;
  result.visibility_score_ = visibility_score;
  result.type_ = AnnotationType::kContentVisibility;
  return result;
}

// static
BatchAnnotationResult BatchAnnotationResult::CreateEmptyAnnotationsResult(
    const std::string& input) {
  BatchAnnotationResult result;
  result.input_ = input;
  return result;
}

bool BatchAnnotationResult::operator==(
    const BatchAnnotationResult& other) const {
  return this->input_ == other.input_ && this->type_ == other.type_ &&
         this->topics_ == other.topics_ && this->entities_ == other.entities_ &&
         this->visibility_score_ == other.visibility_score_;
}

std::vector<BatchAnnotationResult> CreateEmptyBatchAnnotationResults(
    const std::vector<std::string>& inputs) {
  std::vector<BatchAnnotationResult> results;
  results.reserve(inputs.size());
  for (const std::string& input : inputs) {
    results.emplace_back(
        BatchAnnotationResult::CreateEmptyAnnotationsResult(input));
  }
  return results;
}

}  // namespace optimization_guide
