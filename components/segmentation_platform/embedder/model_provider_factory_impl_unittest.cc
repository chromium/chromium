// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/model_provider_factory_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ModelProviderFactoryImplTest : public testing::Test {
 public:
  ModelProviderFactoryImplTest() = default;
  ~ModelProviderFactoryImplTest() override = default;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    provider_factory_ = std::make_unique<ModelProviderFactoryImpl>(
        model_provider_.get(), configs_, task_runner_);
  }

  void TearDown() override {
    task_runner_->RunPendingTasks();
    provider_factory_.reset();
    model_provider_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  // TODO(ssid): Fxi test to take rael configs
  std::vector<std::unique_ptr<Config>> configs_;

  std::unique_ptr<ModelProviderFactoryImpl> provider_factory_;
};

TEST_F(ModelProviderFactoryImplTest, ProviderCreated) {
  EXPECT_TRUE(provider_factory_->CreateProvider(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
  EXPECT_TRUE(provider_factory_->CreateProvider(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
}

class DummyModelProviderFactoryImplTest : public ModelProviderFactoryImplTest {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    std::vector<std::unique_ptr<Config>> configs;
    provider_factory_ = std::make_unique<ModelProviderFactoryImpl>(
        nullptr, configs, task_runner_);
  }
};

TEST_F(DummyModelProviderFactoryImplTest, ProviderCreated) {
  EXPECT_TRUE(provider_factory_->CreateProvider(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE));

  auto provider = provider_factory_->CreateProvider(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  ASSERT_TRUE(provider);

  EXPECT_FALSE(provider->ModelAvailable());

  // This callback should never be invoked. Send a null callback and chrome
  // should not crash by invoking it.
  provider->InitAndFetchModel(ModelProvider::ModelUpdatedCallback());

  base::RunLoop wait;
  provider->ExecuteModelWithInput(
      {1, 2.5}, base::BindOnce(
                    [](base::OnceClosure quit,
                       const std::optional<ModelProvider::Response>& output) {
                      EXPECT_FALSE(output);
                      std::move(quit).Run();
                    },
                    wait.QuitClosure()));
  wait.Run();
}

}  // namespace segmentation_platform
