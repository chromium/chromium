// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() = default;
  ~ModelExecutionManagerTest() override = default;

  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        /*on_device_model_service_controller_=*/nullptr,
        &optimization_guide_logger_);
  }

  ModelExecutionManager* model_execution_manager() {
    return model_execution_manager_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  base::test::TestMessage test_message;
  test_message.set_test("some test");
  base::RunLoop run_loop;
  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test_message,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result) {
            ASSERT_FALSE(result.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.RunUntilIdle();
}

}  // namespace

}  // namespace optimization_guide
