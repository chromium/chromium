// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// A PredictionModel supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating a prediction model.
class PredictionModel {
 public:
  PredictionModel(const PredictionModel&) = delete;
  PredictionModel& operator=(const PredictionModel&) = delete;

  virtual ~PredictionModel();

  // Creates an Prediction model of the correct ModelType specified in
  // |prediction_model|. The validation overhead of this factory can be high and
  // should should be called in the background.
  static std::unique_ptr<PredictionModel> Create(
      const proto::PredictionModel& prediction_model);

  // Returns the OptimizationTargetDecision by evaluating the |model_|
  // using the provided |model_features|. |prediction_score| will be populated
  // with the score output by the model.
  virtual OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) = 0;

  // Provide the version of the |model_| by |this|.
  int64_t GetVersion() const { return version_; }

  // Provide the model features required for evaluation of the |model_| by
  // |this|.
  const base::flat_set<std::string>& GetModelFeatures() const {
    return model_features_;
  }

 protected:
  explicit PredictionModel(const proto::PredictionModel& prediction_model);

  // The in-memory model used for prediction.
  const proto::Model model_;

 private:
  // Determines if the |model_| is complete and can be successfully evaluated by
  // |this|.
  virtual bool ValidatePredictionModel() const = 0;

  // The set of features required by the |model_| to be evaluated.
  const base::flat_set<std::string> model_features_;

  // The version of the |model_|.
  const int64_t version_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_H_
