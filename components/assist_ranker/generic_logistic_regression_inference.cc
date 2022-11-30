// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/generic_logistic_regression_inference.h"

#include <cmath>

#include "base/logging.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/ranker_example_util.h"

namespace assist_ranker {

const float kDefaultThreshold = 0.5f;

float Sigmoid(float x) {
  return 1.0f / (1.0f + exp(-x));
}

GenericLogisticRegressionInference::GenericLogisticRegressionInference(
    GenericLogisticRegressionModel model_proto)
    : proto_(std::move(model_proto)) {}

float GenericLogisticRegressionInference::PredictScore(
    const RankerExample& example) {
  float activation = 0.0f;
  if (!proto_.is_preprocessed_model()) {
    for (const auto& weight_it : proto_.weights()) {
      const std::string& feature_name = weight_it.first;
      const FeatureWeight& feature_weight = weight_it.second;
      switch (feature_weight.feature_type_case()) {
        case FeatureWeight::FEATURE_TYPE_NOT_SET: {
          DVLOG(0) << "Feature type not set for " << feature_name;
          break;
        }
        case FeatureWeight::kScalar: {
          float value;
          if (GetFeatureValueAsFloat(feature_name, example, &value)) {
            const float weight = feature_weight.scalar();
            activation += value * weight;
          } else {
            DVLOG(1) << "Feature not in example: " << feature_name;
          }
          break;
        }
        case FeatureWeight::kOneHot: {
          std::string value;
          if (GetOneHotValue(feature_name, example, &value)) {
            const auto& category_weights = feature_weight.one_hot().weights();
            auto category_it = category_weights.find(value);
            if (category_it != category_weights.end()) {
              activation += category_it->second;
            } else {
              // If the category is not found, use the default weight.
              activation += feature_weight.one_hot().default_weight();
              DVLOG(1) << "Unknown feature value for " << feature_name << ": "
                       << value;
            }
          } else {
            // If the feature is missing, use the default weight.
            activation += feature_weight.one_hot().default_weight();
            DVLOG(1) << "Feature not in example: " << feature_name;
          }
          break;
        }
        case FeatureWeight::kSparse: {
          DVLOG(0) << "Sparse features not implemented yet.";
          break;
        }
        case FeatureWeight::kBucketized: {
          DVLOG(0) << "Bucketized features not implemented yet.";
          break;
        }
      }
    }
  } else {
    RankerExample processed_example = example;
    ExamplePreprocessor::Process(proto_.preprocessor_config(),
                                 &processed_example);
    for (const auto& field : ExampleFloatIterator(processed_example)) {
      if (field.error != ExamplePreprocessor::kSuccess)
        continue;
      const auto& find_weight = proto_.fullname_weights().find(field.fullname);
      if (find_weight != proto_.fullname_weights().end()) {
        activation += find_weight->second * field.value;
      }
    }
  }

  return Sigmoid(proto_.bias() + activation);
}

bool GenericLogisticRegressionInference::Predict(const RankerExample& example) {
  return PredictScore(example) >= GetThreshold();
}

float GenericLogisticRegressionInference::GetThreshold() {
  return (proto_.has_threshold() && proto_.threshold() > 0) ? proto_.threshold()
                                                            : kDefaultThreshold;
}

}  // namespace assist_ranker
