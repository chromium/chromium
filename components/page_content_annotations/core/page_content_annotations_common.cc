// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_common.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <variant>

#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/noisy_metrics_recorder.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"

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
  base::DictValue wi;
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
    case AnnotationType::kCategoryClassifier:
    case AnnotationType::kDeprecatedTextEmbedding:
    case AnnotationType::kDeprecatedPageEntities:
    case AnnotationType::kUnknown:
      return false;
    case AnnotationType::kContentVisibility:
      return !!visibility_score();
  }
}

base::Value BatchAnnotationResult::AsValue() const {
  base::DictValue result;
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

// static
PageContentAnnotationsResult
PageContentAnnotationsResult::CreateCategoryResults(
    std::vector<Category> categories) {
  PageContentAnnotationsResult result;
  result.result_ = std::move(categories);
  return result;
}

PageContentAnnotationsResult::PageContentAnnotationsResult() = default;

PageContentAnnotationsResult::PageContentAnnotationsResult(
    const PageContentAnnotationsResult&) = default;
PageContentAnnotationsResult& PageContentAnnotationsResult::operator=(
    const PageContentAnnotationsResult&) = default;
PageContentAnnotationsResult::~PageContentAnnotationsResult() = default;

AnnotationType PageContentAnnotationsResult::GetType() const {
  if (std::holds_alternative<ContentVisibilityScore>(result_)) {
    return AnnotationType::kContentVisibility;
  }
  if (std::holds_alternative<std::vector<Category>>(result_)) {
    return AnnotationType::kCategoryClassifier;
  }
  return AnnotationType::kUnknown;
}

PageContentAnnotationsResult::ContentVisibilityScore
PageContentAnnotationsResult::GetContentVisibilityScore() const {
  DCHECK_EQ(AnnotationType::kContentVisibility, GetType());
  return std::get<PageContentAnnotationsResult::ContentVisibilityScore>(
      result_);
}

const std::vector<Category>& PageContentAnnotationsResult::GetCategoryResults()
    const {
  DCHECK_EQ(AnnotationType::kCategoryClassifier, GetType());
  return std::get<std::vector<Category>>(result_);
}

int64_t GenerateRapporNoisedScore(double raw_score) {
  int64_t int_score = base::ClampRound(raw_score * 100);
  uint32_t num_buckets = std::pow(2, features::NumBitsForRAPPORMetrics());
  DCHECK_GT(num_buckets, 0u);
  float bucket_size = 100.0 / num_buckets;
  uint32_t bucketed_score =
      static_cast<uint32_t>(std::floor(int_score / bucket_size));
  DCHECK_LE(bucketed_score, num_buckets);
  uint32_t noisy_score = NoisyMetricsRecorder().GetNoisyMetric(
      features::NoiseProbabilityForRAPPORMetrics(),
      std::min(bucketed_score, num_buckets - 1),
      features::NumBitsForRAPPORMetrics());
  return static_cast<int64_t>(noisy_score);
}

}  // namespace page_content_annotations
