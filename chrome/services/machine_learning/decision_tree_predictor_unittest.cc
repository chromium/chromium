// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/decision_tree_predictor.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/services/machine_learning/metrics.h"
#include "chrome/services/machine_learning/public/cpp/decision_tree_model.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

class DecisionTreePredictorTest : public ::testing::Test {
 public:
  DecisionTreePredictorTest() = default;
  ~DecisionTreePredictorTest() override = default;
};

TEST_F(DecisionTreePredictorTest, InstantiateInvalidPredictor) {
  DecisionTreePredictor predictor(nullptr);
  EXPECT_FALSE(predictor.IsValid());
}

TEST_F(DecisionTreePredictorTest, InstantiateValidPredictor) {
  auto model = std::make_unique<DecisionTreeModel>(
      testing::GetModelProtoForPredictionResult(
          mojom::DecisionTreePredictionResult::kTrue));
  DecisionTreePredictor predictor(std::move(model));

  EXPECT_TRUE(predictor.IsValid());
}

TEST_F(DecisionTreePredictorTest, ValidPredictorFromModelSpec) {
  base::HistogramTester histogram_tester;

  auto model_proto = testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue);
  std::string model_string = model_proto->SerializeAsString();
  auto predictor = DecisionTreePredictor::FromModelSpec(
      mojom::DecisionTreeModelSpec::New(model_string));

  EXPECT_TRUE(predictor->IsValid());
  histogram_tester.ExpectTotalCount(
      metrics::kDecisionTreeModelValidationLatency, 1);
}

TEST_F(DecisionTreePredictorTest, InvalidPredictorFromModelSpec) {
  // Scenario 1: initialize from an empty model spec pointer.
  EXPECT_FALSE(DecisionTreePredictor::FromModelSpec({})->IsValid());

  // Scenario 2: failed deserialization due to invalid model string.
  EXPECT_FALSE(DecisionTreePredictor::FromModelSpec(
                   mojom::DecisionTreeModelSpec::New("invalid model string"))
                   ->IsValid());
}

TEST_F(DecisionTreePredictorTest, ModelPrediction) {
  base::HistogramTester histogram_tester;

  mojom::DecisionTreePredictionResult result;
  double score;

  auto model = std::make_unique<DecisionTreeModel>(
      testing::GetModelProtoForPredictionResult(
          mojom::DecisionTreePredictionResult::kTrue));
  std::unique_ptr<mojom::DecisionTreePredictor> predictor =
      std::make_unique<DecisionTreePredictor>(std::move(model));

  predictor->Predict(
      {}, base::BindOnce(
              [](mojom::DecisionTreePredictionResult* p_result, double* p_score,
                 mojom::DecisionTreePredictionResult result, double score) {
                *p_result = result;
                *p_score = score;
              },
              &result, &score));

  histogram_tester.ExpectTotalCount(
      metrics::kDecisionTreeModelEvaluationLatency, 1);
  histogram_tester.ExpectUniqueSample(
      metrics::kDecisionTreeModelPredictionResult,
      mojom::DecisionTreePredictionResult::kTrue, 1);

  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue, result);
  EXPECT_GT(score, testing::kModelThreshold);
}

}  // namespace machine_learning
