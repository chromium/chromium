// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/decision_tree_model.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

class DecisionTreeModelTest : public ::testing::Test {
 public:
  DecisionTreeModelTest() = default;
  ~DecisionTreeModelTest() override = default;
};

TEST_F(DecisionTreeModelTest, InstantiateValidModel) {
  DecisionTreeModel model(testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue));
  EXPECT_TRUE(model.IsValid());
}

TEST_F(DecisionTreeModelTest, InstantiateInvalidModel) {
  DecisionTreeModel model(nullptr);
  EXPECT_FALSE(model.IsValid());
}

TEST_F(DecisionTreeModelTest, ValidModelFromSpec) {
  auto model_proto = testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue);
  std::string model_string = model_proto->SerializeAsString();
  auto model = DecisionTreeModel::FromModelSpec(
      mojom::DecisionTreeModelSpec::New(model_string));

  EXPECT_TRUE(model->IsValid());
}

TEST_F(DecisionTreeModelTest, InvalidModelFromSpec) {
  // Scenario 1: initialize from an empty model spec pointer.
  EXPECT_FALSE(DecisionTreeModel::FromModelSpec({}));

  // Scenario 2: failed deserialization due to invalid model string.
  EXPECT_FALSE(DecisionTreeModel::FromModelSpec(
      mojom::DecisionTreeModelSpec::New("invalid model string")));
}

TEST_F(DecisionTreeModelTest, ValidModelPrediction) {
  DecisionTreeModel model_true(testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue));
  double prediction_score;
  mojom::DecisionTreePredictionResult decision_true =
      model_true.Predict({}, &prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue, decision_true);
  EXPECT_GT(prediction_score, testing::kModelThreshold);

  DecisionTreeModel model_false(testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kFalse));
  mojom::DecisionTreePredictionResult decision_false =
      model_false.Predict({}, &prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse, decision_false);
  EXPECT_LT(prediction_score, testing::kModelThreshold);
}

TEST_F(DecisionTreeModelTest, InvalidModelPrediction) {
  DecisionTreeModel model_invalid(nullptr);
  double prediction_score;
  mojom::DecisionTreePredictionResult decision =
      model_invalid.Predict({}, &prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kUnknown, decision);
}

}  // namespace machine_learning
