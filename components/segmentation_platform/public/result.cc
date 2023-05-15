// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/result.h"
#include <sstream>

namespace segmentation_platform {

ClassificationResult::ClassificationResult(PredictionStatus status)
    : status(status) {}

ClassificationResult::~ClassificationResult() = default;

ClassificationResult::ClassificationResult(const ClassificationResult&) =
    default;

ClassificationResult& ClassificationResult::operator=(
    const ClassificationResult&) = default;

std::string ClassificationResult::ToDebugString() const {
  std::stringstream debug_string;
  debug_string << "Status: ";

  switch (status) {
    case segmentation_platform::PredictionStatus::kNotReady:
      debug_string << "Not ready";
      break;
    case segmentation_platform::PredictionStatus::kFailed:
      debug_string << "Failed";
      break;
    case segmentation_platform::PredictionStatus::kSucceeded:
      debug_string << "Succeeded";
      break;
  }

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

absl::optional<float> AnnotatedNumericResult::GetResultForLabel(
    base::StringPiece label) const {
  if (status != PredictionStatus::kSucceeded ||
      !result.output_config().predictor().has_generic_predictor()) {
    return absl::nullopt;
  }

  const auto& labels =
      result.output_config().predictor().generic_predictor().output_labels();
  DCHECK_EQ(result.result_size(), labels.size());
  for (int index = 0; index < labels.size(); ++index) {
    if (labels.at(index) == label) {
      return result.result().at(index);
    }
  }
  return absl::nullopt;
}

}  // namespace segmentation_platform
