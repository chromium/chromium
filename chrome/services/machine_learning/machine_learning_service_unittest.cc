// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/machine_learning_service.h"

#include <utility>

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

class MachineLearningServiceTest : public ::testing::Test {
 public:
  MachineLearningServiceTest() = default;
  ~MachineLearningServiceTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MachineLearningServiceTest, LoadDecisionTreeModel) {
  auto model_proto = testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue);
  auto model_spec =
      mojom::DecisionTreeModelSpec::New(model_proto->SerializeAsString());

  MachineLearningService service{mojo::NullReceiver()};
  // Get a pointer to the base class to get access to public member functions.
  mojom::MachineLearningService* p_service = &service;

  mojo::Remote<mojom::DecisionTreePredictor> predictor_remote;
  mojom::LoadModelResult result;
  auto callback =
      base::BindOnce([](mojom::LoadModelResult* p_result,
                        mojom::LoadModelResult result) { *p_result = result; },
                     &result);

  p_service->LoadDecisionTree(std::move(model_spec),
                              predictor_remote.BindNewPipeAndPassReceiver(),
                              std::move(callback));

  EXPECT_EQ(result, mojom::LoadModelResult::kOk);
}

}  // namespace machine_learning
