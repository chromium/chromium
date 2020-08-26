// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/services/machine_learning/decision_tree_predictor.h"
#include "chrome/services/machine_learning/metrics.h"
#include "chrome/services/machine_learning/public/cpp/decision_tree_model.h"

namespace machine_learning {

DecisionTreePredictor::DecisionTreePredictor(
    std::unique_ptr<DecisionTreeModel> model)
    : model_(std::move(model)) {}

// static
std::unique_ptr<DecisionTreePredictor> DecisionTreePredictor::FromModelSpec(
    mojom::DecisionTreeModelSpecPtr spec) {
  metrics::ScopedLatencyRecorder recorder(
      metrics::kDecisionTreeModelValidationLatency);
  return std::make_unique<DecisionTreePredictor>(
      DecisionTreeModel::FromModelSpec(std::move(spec)));
}

DecisionTreePredictor::~DecisionTreePredictor() = default;

bool DecisionTreePredictor::IsValid() const {
  return model_ && model_->IsValid();
}

void DecisionTreePredictor::Predict(
    const base::flat_map<std::string, float>& model_features,
    PredictCallback callback) {
  DCHECK(IsValid());

  metrics::ScopedLatencyRecorder recorder(
      metrics::kDecisionTreeModelEvaluationLatency);

  double score = 0.0;
  mojom::DecisionTreePredictionResult result =
      model_->Predict(model_features, &score);

  recorder.RecordTimeElapsed();

  UMA_HISTOGRAM_ENUMERATION(metrics::kDecisionTreeModelPredictionResult,
                            result);

  std::move(callback).Run(result, score);
}

}  // namespace machine_learning
