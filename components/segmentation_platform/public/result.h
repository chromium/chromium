// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_RESULT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_RESULT_H_

#include <string>
#include <vector>

#include "base/callback_helpers.h"

namespace segmentation_platform {

// Various status for PredictionResult.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.segmentation_platform.prediction_status)
enum class PredictionStatus {
  kNotReady = 0,
  kFailed = 1,
  kSucceeded = 2,
};

// ClassificationResult is returned when Predictor specified by the client in
// OutputConfig is one of BinaryClassifier, MultiClassClassifier or
// BinnedClassifier.
struct ClassificationResult {
  explicit ClassificationResult(PredictionStatus status);
  ~ClassificationResult();

  ClassificationResult(const ClassificationResult&);
  ClassificationResult& operator=(const ClassificationResult&);

  // Various error codes such as model failed or insufficient data collection.
  PredictionStatus status;

  // The list of labels arranged in descending order of result from model
  // evaluation. For BinaryClassifier, it is eithier a `positive_label` or
  // `negative_label`. For MultiClassClassifier, it is list of `top_k_outputs`
  // labels based on the score for the label. For BinnedClassifier, it is a
  // label from one of the bin depending on where the score from the model
  // evaluation lies.
  std::vector<std::string> ordered_labels;
};

// RegressionResult is returned when Predictor specified by the client in
// OutputConfig is Regressor.
struct RegressionResult {
  explicit RegressionResult(PredictionStatus status);
  ~RegressionResult();

  RegressionResult(const RegressionResult&);
  RegressionResult& operator=(const RegressionResult&);

  // Various error codes such as model failed or insufficient data collection.
  PredictionStatus status;

  // The result of regression.
  float regression_result{0.0f};
};

using ClassificationResultCallback =
    base::OnceCallback<void(const ClassificationResult&)>;
using RegressionResultCallback =
    base::OnceCallback<void(const RegressionResult&)>;

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_RESULT_H_
