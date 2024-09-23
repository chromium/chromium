// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_common.h"

#include <algorithm>
#include <ostream>

#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace page_content_annotations {

WeightedIdentifier::WeightedIdentifier(int32_t value, double weight)
    : value_(value), weight_(weight) {
  DCHECK_GE(weight_, 0.0);
  DCHECK_LE(weight_, 1.0);
}
WeightedIdentifier::WeightedIdentifier(const WeightedIdentifier&) = default;
WeightedIdentifier::~WeightedIdentifier() = default;

bool WeightedIdentifier::operator==(const WeightedIdentifier& other) const {
  constexpr double kWeightTolerance = 1e-6;
  return this->value_ == other.value_ &&
         std::abs(this->weight_ - other.weight_) <= kWeightTolerance;
}

std::string WeightedIdentifier::ToString() const {
  return base::StringPrintf("WeightedIdentifier{%d,%f}", value(), weight());
}

base::Value WeightedIdentifier::AsValue() const {
  base::Value::Dict wi;
  wi.Set("value", value());
  wi.Set("weight", weight());
  return base::Value(std::move(wi));
}

std::ostream& operator<<(std::ostream& stream, const WeightedIdentifier& ws) {
  stream << ws.ToString();
  return stream;
}

BatchAnnotationResult::BatchAnnotationResult() = default;
BatchAnnotationResult::BatchAnnotationResult(const BatchAnnotationResult&) =
    default;
BatchAnnotationResult::~BatchAnnotationResult() = default;

bool BatchAnnotationResult::HasOutputForType() const {
  switch (type()) {
    case AnnotationType::kDeprecatedTextEmbedding:
    case AnnotationType::kDeprecatedPageEntities:
    case AnnotationType::kUnknown:
      return false;
    case AnnotationType::kContentVisibility:
      return !!visibility_score();
  }
}

base::Value BatchAnnotationResult::AsValue() const {
  base::Value::Dict result;
  result.Set("input", input());
  result.Set("type", AnnotationTypeToString(type()));

  if (visibility_score()) {
    result.Set("visibility_score", *visibility_score());
  }

  return base::Value(std::move(result));
}

std::string BatchAnnotationResult::ToJSON() const {
  std::string json;
  if (base::JSONWriter::Write(AsValue(), &json)) {
    return json;
  }
  return std::string();
}

std::string BatchAnnotationResult::ToString() const {
  std::string output = "nullopt";
  if (visibility_score_) {
    output = base::NumberToString(*visibility_score_);
  }
  return base::StringPrintf(
      "BatchAnnotationResult{"
      "\"%s\", "
      "type: %s, "
      "output: %s}",
      input_.c_str(), AnnotationTypeToString(type_).c_str(), output.c_str());
}

std::ostream& operator<<(std::ostream& stream,
                         const BatchAnnotationResult& result) {
  stream << result.ToString();
  return stream;
}

//  static
BatchAnnotationResult BatchAnnotationResult::CreateContentVisibilityResult(
    const std::string& input,
    std::optional<double> visibility_score) {
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

// static
PageContentAnnotationsResult
PageContentAnnotationsResult::CreateContentVisibilityScoreResult(
    const PageContentAnnotationsResult::ContentVisibilityScore& score) {
  PageContentAnnotationsResult result;
  result.result_ = score;
  return result;
}

PageContentAnnotationsResult::PageContentAnnotationsResult() = default;

PageContentAnnotationsResult::PageContentAnnotationsResult(
    const PageContentAnnotationsResult&) = default;
PageContentAnnotationsResult& PageContentAnnotationsResult::operator=(
    const PageContentAnnotationsResult&) = default;
PageContentAnnotationsResult::~PageContentAnnotationsResult() = default;

AnnotationType PageContentAnnotationsResult::GetType() const {
  if (absl::holds_alternative<ContentVisibilityScore>(result_)) {
    return AnnotationType::kContentVisibility;
  }
  return AnnotationType::kUnknown;
}

PageContentAnnotationsResult::ContentVisibilityScore
PageContentAnnotationsResult::GetContentVisibilityScore() const {
  DCHECK_EQ(AnnotationType::kContentVisibility, GetType());
  return absl::get<PageContentAnnotationsResult::ContentVisibilityScore>(
      result_);
}

}  // namespace page_content_annotations
