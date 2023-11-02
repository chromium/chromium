// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_
#define COMPONENTS_ASSIST_RANKER_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_

#include "components/assist_ranker/proto/generic_logistic_regression_model.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace assist_ranker {

float Sigmoid(float x);

// TODO(hamelphi): Implement an interface base class
// BinaryClassificationInferenceModule.
//
// Implements inference for a GenericLogisticRegressionModel.
class GenericLogisticRegressionInference {
 public:
  explicit GenericLogisticRegressionInference(
      GenericLogisticRegressionModel model_proto);
  // Returns a boolean decision given a RankerExample. Uses the same logic as
  // PredictScore, and then applies the model decision threshold.
  bool Predict(const RankerExample& example);
  // Returns a score between 0 and 1 given a RankerExample.
  float PredictScore(const RankerExample& example);

 private:
  // Returns the decision threshold. If no threshold is specified in the proto,
  // use 0.5.
  float GetThreshold();
  const GenericLogisticRegressionModel proto_;
};

}  // namespace assist_ranker
#endif  // COMPONENTS_ASSIST_RANKER_GENERIC_LOGISTIC_REGRESSION_INFERENCE_H_
