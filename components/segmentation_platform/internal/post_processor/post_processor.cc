// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/post_processor/post_processor.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace segmentation_platform {

std::vector<std::string> PostProcessor::GetClassifierResults(
    const proto::PredictionResult& prediction_result) {
  const std::vector<float> model_scores(prediction_result.result().begin(),
                                        prediction_result.result().end());
  const proto::Predictor& predictor =
      prediction_result.output_config().predictor();
  switch (predictor.PredictorType_case()) {
    case proto::Predictor::kBinaryClassifier:
      return GetBinaryClassifierResults(model_scores,
                                        predictor.binary_classifier());
    case proto::Predictor::kMultiClassClassifier:
      return GetMultiClassClassifierResults(model_scores,
                                            predictor.multi_class_classifier());
    case proto::Predictor::kBinnedClassifier:
      return GetBinnedClassifierResults(model_scores,
                                        predictor.binned_classifier());
    default:
      NOTREACHED();
      return std::vector<std::string>();
  }
}

std::vector<std::string> PostProcessor::GetBinaryClassifierResults(
    const std::vector<float>& model_scores,
    const proto::Predictor::BinaryClassifier& binary_classifier) const {
  DCHECK_EQ(1u, model_scores.size());

  const std::string& winning_label =
      (model_scores[0] >= binary_classifier.threshold()
           ? binary_classifier.positive_label()
           : binary_classifier.negative_label());
  return std::vector<std::string>(1, winning_label);
}

std::vector<std::string> PostProcessor::GetMultiClassClassifierResults(
    const std::vector<float>& model_scores,
    const proto::Predictor::MultiClassClassifier& multi_class_classifier)
    const {
  DCHECK_EQ(static_cast<int>(model_scores.size()),
            multi_class_classifier.class_labels_size());

  std::vector<std::pair<std::string, float>> labeled_results;
  for (int index = 0; index < static_cast<int>(model_scores.size()); index++) {
    labeled_results.emplace_back(multi_class_classifier.class_labels(index),
                                 model_scores[index]);
  }
  // Sort the labels in descending order of score.
  std::sort(labeled_results.begin(), labeled_results.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });
  float threshold = multi_class_classifier.threshold();
  int top_k_outputs = multi_class_classifier.top_k_outputs();

  std::vector<std::string> top_k_output_labels;
  for (int index = 0; index < top_k_outputs; index++) {
    if (labeled_results[index].second < threshold) {
      break;
    }
    top_k_output_labels.emplace_back(labeled_results[index].first);
  }
  return top_k_output_labels;
}

std::vector<std::string> PostProcessor::GetBinnedClassifierResults(
    const std::vector<float>& model_scores,
    const proto::Predictor::BinnedClassifier& binned_classifier) const {
  DCHECK_EQ(1u, model_scores.size());
  DCHECK_LE(1, binned_classifier.bins_size());

  std::string winning_bin_label = binned_classifier.underflow_label();

  for (int index = 0; index < binned_classifier.bins_size(); index++) {
    if (model_scores[0] >= binned_classifier.bins(index).min_range()) {
      winning_bin_label = binned_classifier.bins(index).label();
    }
  }
  return std::vector<std::string>(1, winning_bin_label);
}

ClassificationResult PostProcessor::GetPostProcessedClassificationResult(
    const proto::PredictionResult& prediction_result,
    PredictionStatus status) {
  std::vector<std::string> ordered_labels;
  if (prediction_result.result_size() > 0) {
    ordered_labels = GetClassifierResults(prediction_result);
  }
  ClassificationResult classification_result = ClassificationResult(status);
  classification_result.ordered_labels = ordered_labels;
  return classification_result;
}

}  // namespace segmentation_platform