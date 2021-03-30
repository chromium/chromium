// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/test_support/fake_service_connection.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/services/machine_learning/machine_learning_service.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom-shared.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom-shared.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

TEST(FakeServiceConnectionTest, MultipleLaunchesReusesSameService) {
  testing::FakeServiceConnection service_connection;

  ASSERT_EQ(&service_connection, ServiceConnection::GetInstance());

  auto* service_ptr1 = service_connection.GetService();
  EXPECT_TRUE(service_ptr1);
  EXPECT_TRUE(service_connection.is_service_running());

  auto* service_ptr2 = service_connection.GetService();
  EXPECT_EQ(service_ptr1, service_ptr2);

  service_connection.ResetServiceForTesting();
  EXPECT_FALSE(service_connection.is_service_running());
}

TEST(FakeServiceConnectionTest, LoadInvalidDecisionTree) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::FakeServiceConnection service_connection;

  mojo::Remote<mojom::DecisionTreePredictor> predictor;
  mojom::LoadModelResult result = mojom::LoadModelResult::kLoadModelError;

  service_connection.LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New("some model string"),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult* p_result,
                        mojom::LoadModelResult result) { *p_result = result; },
                     &result));

  service_connection.SetLoadModelResult(
      mojom::LoadModelResult::kModelSpecError);
  service_connection.RunScheduledCalls();

  EXPECT_TRUE(service_connection.is_service_running());
  EXPECT_EQ(mojom::LoadModelResult::kModelSpecError, result);

  predictor.FlushForTesting();
  // Invalid models doesn't lead to a predictor connection.
  EXPECT_FALSE(predictor.is_connected());
}

TEST(FakeServiceConnectionTest, LoadValidDecisionTreeAndPredict) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::FakeServiceConnection service_connection;

  // Making an actual model for consistency even though we are mocking the
  // result.
  auto model_proto = testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue);

  mojo::Remote<mojom::DecisionTreePredictor> predictor;
  mojom::LoadModelResult result = mojom::LoadModelResult::kLoadModelError;

  service_connection.LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New(model_proto->SerializeAsString()),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult* p_result,
                        mojom::LoadModelResult result) { *p_result = result; },
                     &result));

  service_connection.SetLoadModelResult(mojom::LoadModelResult::kOk);
  service_connection.RunScheduledCalls();

  EXPECT_TRUE(service_connection.is_service_running());
  EXPECT_EQ(mojom::LoadModelResult::kOk, result);

  predictor.FlushForTesting();
  // Valid models leads to a predictor connection.
  EXPECT_TRUE(predictor.is_connected());

  // Normal prediction call.
  const mojom::DecisionTreePredictionResult prediction_result_expected =
      mojom::DecisionTreePredictionResult::kTrue;
  const double prediction_score_expected = 2.33;

  predictor->Predict(
      {}, base::BindOnce(
              [](mojom::DecisionTreePredictionResult result_expected,
                 double score_expected,
                 mojom::DecisionTreePredictionResult result, double score) {
                EXPECT_EQ(result_expected, result);
                EXPECT_EQ(score_expected, score);
              },
              prediction_result_expected, prediction_score_expected));
  predictor.FlushForTesting();

  service_connection.SetDecisionTreePredictionResult(prediction_result_expected,
                                                     prediction_score_expected);
  service_connection.RunScheduledCalls();

  // Predictor launches a call but the service got disconnected.
  predictor->Predict(
      {}, base::BindOnce(
              [](mojom::DecisionTreePredictionResult result, double score) {
                // Callback should not run.
                FAIL();
              }));
  predictor.FlushForTesting();

  service_connection.ResetServiceForTesting();
  service_connection.RunScheduledCalls();

  predictor.FlushForTesting();
  EXPECT_FALSE(predictor.is_connected());
}

}  // namespace machine_learning
