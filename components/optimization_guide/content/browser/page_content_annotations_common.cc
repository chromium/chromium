// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_common.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace optimization_guide {

WeightedString::WeightedString(const std::string& value, double weight)
    : value(value), weight(weight) {
  DCHECK_GE(weight, 0.0);
  DCHECK_LE(weight, 1.0);
}
WeightedString::WeightedString(const WeightedString&) = default;
WeightedString::~WeightedString() = default;

BatchAnnotationResult::BatchAnnotationResult(const std::string& input)
    : input(input) {}
BatchAnnotationResult::BatchAnnotationResult(const BatchAnnotationResult&) =
    default;
BatchAnnotationResult::~BatchAnnotationResult() = default;

std::vector<BatchAnnotationResult> FillResultWithStatus(
    const std::vector<std::string>& inputs,
    ExecutionStatus status) {
  std::vector<BatchAnnotationResult> results;
  results.reserve(inputs.size());
  for (const std::string& input : inputs) {
    BatchAnnotationResult result(input);
    result.status = status;
    results.push_back(std::move(result));
  }
  return results;
}

}  // namespace optimization_guide
