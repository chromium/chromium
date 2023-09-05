// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSOR_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

// Handles post processing of model evaluation results.
// Postprocessing layer gives the result to the client based on the predictor
// they supplied in the config.
class PostProcessor {
 public:
  PostProcessor() = default;
  ~PostProcessor() = default;

  // Disallow copy/assign.
  PostProcessor(const PostProcessor&) = delete;
  PostProcessor& operator=(const PostProcessor&) = delete;

  static bool IsClassificationResult(
      const proto::PredictionResult& prediction_result);

  // Called when the result from model execution are ready. Gives list of
  // ordered `output_labels` based on the classifier given by the client in the
  // OutputConfig.
  std::vector<std::string> GetClassifierResults(
      const proto::PredictionResult& prediction_result);

  // Calls GetClassifieResults to get post processed result from model execution
  // and wrap them as ClassificationResult.
  ClassificationResult GetPostProcessedClassificationResult(
      const proto::PredictionResult& prediction_result,
      PredictionStatus status);

  // Get TTL for the top label in the prediction result for the client.
  base::TimeDelta GetTTLForPredictedResult(
      const proto::PredictionResult& prediction_result);

  // Used for metrics collection. Returns the index of the winning label in the
  // list of labels as defined in the metadata. For binary classifier: 0 for
  // false, 1 for true. For binned classifier: -1 for underflow label, otherwise
  // index of the bin that it falls into. For multiclass classifier: -1 when no
  // winning label, otherwise the index of the label in the labels list.
  // Returns -2 for all kinds of invalid cases.
  int GetIndexOfTopLabel(const proto::PredictionResult& prediction_result);

  // Converts the prediction result into RawResult usable by
  // clients.
  RawResult GetRawResult(const proto::PredictionResult& prediction_result,
                         PredictionStatus status);

 private:
  std::vector<std::string> GetBinaryClassifierResults(
      const std::vector<float>& model_scores,
      const proto::Predictor::BinaryClassifier& binary_classifier) const;

  std::vector<std::string> GetMultiClassClassifierResults(
      const std::vector<float>& model_scores,
      const proto::Predictor::MultiClassClassifier& multi_class_classifier)
      const;

  std::vector<std::string> GetBinnedClassifierResults(
      const std::vector<float>& model_scores,
      const proto::Predictor::BinnedClassifier& binned_classifier) const;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSOR_H_
