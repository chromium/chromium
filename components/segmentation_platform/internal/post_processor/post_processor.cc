// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/post_processor/post_processor.h"

#include "base/time/time.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

namespace {

// UMA constants for various types of classifier outputs.
constexpr int kInvalidResult = -2;
constexpr int kNoWinningLabel = -1;
constexpr int kUnderflowBinIndex = -1;

bool IsValidResult(const proto::PredictionResult& prediction_result) {
  if (metadata_utils::ValidateOutputConfig(prediction_result.output_config()) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    return false;
  }
  int output_length = 1;
  const auto& predictor = prediction_result.output_config().predictor();
  if (predictor.has_multi_class_classifier()) {
    output_length = predictor.multi_class_classifier().class_labels_size();
    int threshold_count = predictor.multi_class_classifier().class_thresholds_size();
    if (threshold_count > 0 && output_length != threshold_count) {
      return false;
    }
  }
  if (predictor.has_generic_predictor()) {
    output_length = predictor.generic_predictor().output_labels_size();
  }
  return prediction_result.result_size() > 0 &&
         prediction_result.has_output_config() &&
         prediction_result.result_size() == output_length;
}

bool IsScoreBelowMultiClassThreshold(
    const proto::Predictor::MultiClassClassifier& multi_class_classifier,
    const float class_score,
    const int class_index) {
  if (multi_class_classifier.class_thresholds_size() > 0) {
    return class_score < multi_class_classifier.class_thresholds(class_index);
  } else {
    return class_score < multi_class_classifier.threshold();
  }
}

}  // namespace

// static
bool PostProcessor::IsClassificationResult(
    const proto::PredictionResult& prediction_result) {
  const proto::Predictor& predictor =
      prediction_result.output_config().predictor();
  switch (predictor.PredictorType_case()) {
    case proto::Predictor::kBinaryClassifier:
    case proto::Predictor::kMultiClassClassifier:
    case proto::Predictor::kBinnedClassifier:
      return true;
    default:
      return false;
  }
}

std::vector<std::string> PostProcessor::GetClassifierResults(
    const proto::PredictionResult& prediction_result) {
  if (!IsValidResult(prediction_result)) {
    return {};
  }
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
      NOTREACHED_IN_MIGRATION();
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
  CHECK(!(multi_class_classifier.has_threshold() &&
          multi_class_classifier.class_thresholds_size() > 0))
      << "threshold and class_thresholds can't be both set at the same time";

  if (multi_class_classifier.class_thresholds_size() > 0) {
    CHECK_EQ(static_cast<int>(model_scores.size()),
             multi_class_classifier.class_thresholds_size());
  }

  std::vector<std::pair<std::string, float>> labeled_results;
  for (int index = 0; index < static_cast<int>(model_scores.size()); index++) {
    if (!IsScoreBelowMultiClassThreshold(multi_class_classifier,
                                         model_scores[index], index)) {
      labeled_results.emplace_back(multi_class_classifier.class_labels(index),
                                   model_scores[index]);
    }
  }
  // Sort the labels in descending order of score.
  std::stable_sort(labeled_results.begin(), labeled_results.end(),
                   [](const std::pair<std::string, float>& a,
                      const std::pair<std::string, float>& b) {
                     return a.second > b.second;
                  });
  int elements_to_return =
      std::min(multi_class_classifier.top_k_outputs(),
               static_cast<int64_t>(labeled_results.size()));

  std::vector<std::string> top_k_output_labels;
  for (int index = 0; index < elements_to_return; index++) {
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
  if (!IsValidResult(prediction_result)) {
    // The post processing failed, mark the result as failure for the clients.
    if (status == PredictionStatus::kSucceeded) {
      status = PredictionStatus::kFailed;
    }
    return ClassificationResult(status);
  }
  ClassificationResult classification_result = ClassificationResult(status);
  classification_result.ordered_labels =
      GetClassifierResults(prediction_result);
  return classification_result;
}

int PostProcessor::GetIndexOfTopLabel(
    const proto::PredictionResult& prediction_result) {
  if (!IsValidResult(prediction_result)) {
    return kInvalidResult;
  }

  const std::vector<std::string>& result_labels =
      GetClassifierResults(prediction_result);
  if (result_labels.empty()) {
    return kNoWinningLabel;
  }

  const std::string& top_label = result_labels[0];
  const auto& predictor = prediction_result.output_config().predictor();

  switch (predictor.PredictorType_case()) {
    case proto::Predictor::kBinaryClassifier: {
      bool bool_result =
          (top_label == predictor.binary_classifier().positive_label());
      return static_cast<int>(bool_result);
    }
    case proto::Predictor::kMultiClassClassifier: {
      const auto& multi_class_classifier = predictor.multi_class_classifier();
      for (int i = 0; i < multi_class_classifier.class_labels_size(); i++) {
        if (top_label == multi_class_classifier.class_labels(i)) {
          return i;
        }
      }
      NOTREACHED_IN_MIGRATION();
      return kInvalidResult;
    }
    case proto::Predictor::kBinnedClassifier: {
      const auto& binned_classifier = predictor.binned_classifier();
      if (top_label == binned_classifier.underflow_label()) {
        return kUnderflowBinIndex;
      }

      for (int i = 0; i < binned_classifier.bins_size(); i++) {
        if (top_label == binned_classifier.bins(i).label()) {
          return i;
        }
      }
      NOTREACHED_IN_MIGRATION();
      return kInvalidResult;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return kInvalidResult;
  }
}

base::TimeDelta PostProcessor::GetTTLForPredictedResult(
    const proto::PredictionResult& prediction_result) {
  std::vector<std::string> ordered_labels;
  if (prediction_result.result_size() > 0 &&
      prediction_result.has_output_config()) {
    if (IsClassificationResult(prediction_result)) {
      ordered_labels = GetClassifierResults(prediction_result);
    }
    if (!prediction_result.output_config().has_predicted_result_ttl()) {
      LOG(ERROR) << "Prediction result has no `predicted_result_ttl` on its "
                    "`output_config`, returning empty TTL.";
      return base::TimeDelta();
    }

    const auto& predicted_result_ttl =
        prediction_result.output_config().predicted_result_ttl();
    const auto& top_label_to_ttl_map =
        predicted_result_ttl.top_label_to_ttl_map();
    auto default_ttl = predicted_result_ttl.default_ttl();
    const auto time_unit = predicted_result_ttl.time_unit();

    if (ordered_labels.empty()) {
      return default_ttl * metadata_utils::ConvertToTimeDelta(time_unit);
    }
    const auto iter = top_label_to_ttl_map.find(ordered_labels[0]);
    int64_t ttl_to_use =
        iter == top_label_to_ttl_map.end() ? default_ttl : iter->second;
    return ttl_to_use * metadata_utils::ConvertToTimeDelta(time_unit);
  }
  return base::TimeDelta();
}

RawResult PostProcessor::GetRawResult(
    const proto::PredictionResult& prediction_result,
    PredictionStatus status) {
  if (status != PredictionStatus::kSucceeded) {
    return RawResult(status);
  }
  if (!IsValidResult(prediction_result)) {
    return RawResult(PredictionStatus::kFailed);
  }
  RawResult result(status);
  result.result = std::move(prediction_result);
  return result;
}

}  // namespace segmentation_platform
