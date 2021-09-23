// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_COMMON_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_COMMON_H_

#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// The status of a page content annotation execution.
enum class ExecutionStatus {
  // Status is unknown.
  kUnknown = 0,

  // Execution finished successfully.
  kSuccess = 1,

  // Execution is still pending.
  kPending = 2,

  // Execution failed for some reason internal to Opt Guide. These failures
  // should not happen and result in a DCHECK in non-production builds.
  kErrorInternalError = 3,

  // Execution failed because the model file is not available.
  kErrorModelFileNotAvailable = 4,

  // Execution failed because the model file could not be loaded into TFLite.
  kErrorModelFileNotValid = 5,

  // Execution failed because the input was empty or otherwise invalid.
  kErrorEmptyOrInvalidInput = 6,
};

// The type of annotation that is being done on the given input.
enum class AnnotationType {
  // The input will be annotated with the topics on the page. These topics are
  // fairly high-level like "sports" or "news".
  kPageTopics,

  // The input will be annotated for the visibility of the content.
  kContentVisibility,

  // The input will be annotated with the entity IDs on the page, for example
  // listing the IDs of all the proper nouns on a page. To map the IDs back to
  // human-readable strings, use `EntityMetadataProvider`.
  kPageEntities,
};

// A weighted string value.
struct WeightedString {
 public:
  WeightedString(const std::string& value, double weight);
  WeightedString(const WeightedString&);
  ~WeightedString();

  const std::string value;

  // In the range of [0.0, 1.0].
  const double weight = 0;
};

// The result of an execution, and all associated data.
struct BatchAnnotationResult {
 public:
  explicit BatchAnnotationResult(const std::string& input);
  BatchAnnotationResult(const BatchAnnotationResult&);
  ~BatchAnnotationResult();

  const std::string input;

  ExecutionStatus status = ExecutionStatus::kUnknown;

  // Only one of these fields will be populated at a time, depending on the
  // annotation type that was requested.
  absl::optional<std::vector<WeightedString>> topics;
  absl::optional<std::vector<WeightedString>> entites;
  absl::optional<double> visibility_score;
};

// Creates a vector of |BatchAnnotationResult| from the given |inputs| where
// each result's status is set to |status|. Useful for creating an Annotation
// response with a single error.
std::vector<BatchAnnotationResult> FillResultWithStatus(
    const std::vector<std::string>& inputs,
    ExecutionStatus status);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_COMMON_H_
