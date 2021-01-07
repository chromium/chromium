// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"

#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace machine_learning {
namespace testing {

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetModelProtoForPredictionResult(mojom::DecisionTreePredictionResult decision) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> model;
  switch (decision) {
    case mojom::DecisionTreePredictionResult::kTrue:
      model = optimization_guide::GetSingleLeafDecisionTreePredictionModel(
          kModelThreshold, kModelWeight,
          (kModelThreshold + kModelValueDiff) / kModelWeight);
      break;
    case mojom::DecisionTreePredictionResult::kFalse:
      model = optimization_guide::GetSingleLeafDecisionTreePredictionModel(
          kModelThreshold, kModelWeight,
          (kModelThreshold - kModelValueDiff) / kModelWeight);
      break;
    default:
      return nullptr;
  }
  model->mutable_model_info()->set_version(1);
  model->mutable_model_info()->add_supported_model_types(
      optimization_guide::proto::MODEL_TYPE_DECISION_TREE);
  return model;
}

}  // namespace testing
}  // namespace machine_learning
