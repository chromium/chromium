// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/result.h"

#include <sstream>
#include <string_view>

namespace segmentation_platform {

namespace {

std::string StatusToString(PredictionStatus status) {
  switch (status) {
    case PredictionStatus::kNotReady:
      return "Not ready";
    case PredictionStatus::kFailed:
      return "Failed";
    case PredictionStatus::kSucceeded:
      return "Succeeded";
  }
}

}  // namespace

ClassificationResult::ClassificationResult(PredictionStatus status)
    : status(status) {}

ClassificationResult::~ClassificationResult() = default;

ClassificationResult::ClassificationResult(const ClassificationResult&) =
    default;

ClassificationResult& ClassificationResult::operator=(
    const ClassificationResult&) = default;

std::string ClassificationResult::ToDebugString() const {
  std::stringstream debug_string;
  debug_string << "Status: " << StatusToString(status);

  for (unsigned i = 0; i < ordered_labels.size(); ++i) {
    debug_string << " output " << i << ": " << ordered_labels.at(i);
  }

  return debug_string.str();
}

AnnotatedNumericResult::AnnotatedNumericResult(PredictionStatus status)
    : status(status) {}

AnnotatedNumericResult::~AnnotatedNumericResult() = default;

AnnotatedNumericResult::AnnotatedNumericResult(const AnnotatedNumericResult&) =
    default;

AnnotatedNumericResult& AnnotatedNumericResult::operator=(
    const AnnotatedNumericResult&) = default;

std::optional<float> AnnotatedNumericResult::GetResultForLabel(
    std::string_view label) const {
  if (status != PredictionStatus::kSucceeded) {
    return std::nullopt;
  }

  if (result.output_config().predictor().has_generic_predictor()) {
    const auto& labels =
        result.output_config().predictor().generic_predictor().output_labels();
    DCHECK_EQ(result.result_size(), labels.size());
    for (int index = 0; index < labels.size(); ++index) {
      if (labels.at(index) == label) {
        return result.result().at(index);
      }
    }
  } else if (result.output_config().predictor().has_multi_class_classifier()) {
    const auto& labels = result.output_config()
                             .predictor()
                             .multi_class_classifier()
                             .class_labels();
    DCHECK_EQ(result.result_size(), labels.size());
    for (int index = 0; index < labels.size(); ++index) {
      if (labels.at(index) == label) {
        return result.result().at(index);
      }
    }
  }

  return std::nullopt;
}

std::string AnnotatedNumericResult::ToDebugString() const {
  std::stringstream debug_string;
  debug_string << "Status: " << StatusToString(status);

  for (int i = 0; i < result.result_size(); ++i) {
    debug_string << " output " << i << ": " << result.result(i);
  }

  return debug_string.str();
}

}  // namespace segmentation_platform
