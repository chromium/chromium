// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/category_classifier_model_handler.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/category_classifier_metadata.pb.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

class CategoryClassifierModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  CategoryClassifierModelProvider() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(
        "components/test/data/page_content_annotations");
    model_file_path_ = test_data_dir.AppendASCII("edu_classifier.tflite");
  }

  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& any,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path_)
                                .SetModelMetadata(model_metadata_)
                                .Build();
      observer->OnModelUpdated(optimization_target, *model_metadata);
      model_observers_.AddObserver(observer);
    }
  }

  void SetModelMetadata(const optimization_guide::proto::Any& model_metadata) {
    model_metadata_ = model_metadata;
    auto model_info = optimization_guide::TestModelInfoBuilder()
                          .SetModelFilePath(model_file_path_)
                          .SetModelMetadata(model_metadata_)
                          .Build();
    model_observers_.Notify(
        &optimization_guide::OptimizationTargetModelObserver::OnModelUpdated,
        optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
        *model_info);
  }

 private:
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      model_observers_;
  base::FilePath model_file_path_;
  optimization_guide::proto::Any model_metadata_;
};

class CategoryClassifierModelHandlerTest : public testing::Test {
 public:
  CategoryClassifierModelHandlerTest() = default;
  ~CategoryClassifierModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<CategoryClassifierModelProvider>();
    model_handler_ = std::make_unique<CategoryClassifierModelHandler>(
        optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner());
  }

  void SetModelMetadata(
      const optimization_guide::proto::CategoryClassifierMetadata& metadata) {
    optimization_guide::proto::Any any;
    any.set_type_url("type.googleapis.com/CategoryClassifierMetadata");
    metadata.SerializeToString(any.mutable_value());
    model_provider_->SetModelMetadata(any);
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  CategoryClassifierModelHandler* model_handler() const {
    return model_handler_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CategoryClassifierModelProvider> model_provider_;
  std::unique_ptr<CategoryClassifierModelHandler> model_handler_;
};

TEST_F(CategoryClassifierModelHandlerTest, ModelExecutes) {
  CategoryClassifierModelHandler* handler = model_handler();

  std::vector<float> input(768, 0.0f);

  base::test::TestFuture<const std::optional<float>&> test_future;
  handler->ExecuteModelWithInput(test_future.GetCallback(), input);

  ASSERT_TRUE(test_future.Wait());
  ASSERT_TRUE(test_future.Get().has_value());
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_NoPassageEmbeddingConcatenation) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  EXPECT_EQ(input_vector, (std::vector<float>{0.1f, 0.1f, 0.1f}));
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_NoPassagesInConcatStrategy) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(0);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MAX);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  EXPECT_EQ(input_vector, (std::vector<float>{0.1f, 0.1f, 0.1f}));
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_UnknownPoolingStrategyInConcatStrategy) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(10);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_UNKNOWN);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  EXPECT_EQ(input_vector, (std::vector<float>{0.1f, 0.1f, 0.1f}));
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_MaxPooling_NoPassagesShouldPadOut) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(10);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MAX);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> padding_vector = {0.0f, 0.0f, 0.0f};
  expected_input_vector.insert(expected_input_vector.end(),
                               padding_vector.begin(), padding_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

TEST_F(CategoryClassifierModelHandlerTest, CreateInputVector_MaxPooling) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(2);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MAX);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  // The third passage should be ignored due to max_passages being 2.
  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
      std::vector<passage_embeddings::Embedding>{
          passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
          passage_embeddings::Embedding({0.3f, 0.3f, 0.3f}),
          passage_embeddings::Embedding({-1.0f, -1.0f, -1.0f})});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> max_pooled_vector = {0.3f, 0.3f, 0.3f};
  expected_input_vector.insert(expected_input_vector.end(),
                               max_pooled_vector.begin(),
                               max_pooled_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_MeanPooling_NoPassagesShouldPadOut) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(10);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MEAN);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> padding_vector = {0.0f, 0.0f, 0.0f};
  expected_input_vector.insert(expected_input_vector.end(),
                               padding_vector.begin(), padding_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

TEST_F(CategoryClassifierModelHandlerTest, CreateInputVector_MeanPooling) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(4);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MEAN);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
      std::vector<passage_embeddings::Embedding>{
          passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
          passage_embeddings::Embedding({0.3f, 0.3f, 0.3f})});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> mean_pooled_vector = {0.2f, 0.2f, 0.2f};
  expected_input_vector.insert(expected_input_vector.end(),
                               mean_pooled_vector.begin(),
                               mean_pooled_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

TEST_F(CategoryClassifierModelHandlerTest,
       CreateInputVector_MeanMaxPooling_NoPassagesShouldPadOutDouble) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(10);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MEAN_MAX);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}), {});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> padding_vector = {0.0f, 0.0f, 0.0f};
  expected_input_vector.insert(expected_input_vector.end(),
                               padding_vector.begin(), padding_vector.end());
  expected_input_vector.insert(expected_input_vector.end(),
                               padding_vector.begin(), padding_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

TEST_F(CategoryClassifierModelHandlerTest, CreateInputVector_MeanMaxPooling) {
  CategoryClassifierModelHandler* handler = model_handler();

  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::
      CategoryClassifierPassageEmbeddingConcatenationStrategy
          concatenation_strategy;
  concatenation_strategy.set_max_passages(2);
  concatenation_strategy.set_pooling_strategy(
      optimization_guide::proto::
          CategoryClassifierPassageEmbeddingConcatenationStrategy::
              POOLING_STRATEGY_MEAN_MAX);
  *metadata.mutable_passage_embedding_concatenation_strategy() =
      concatenation_strategy;
  SetModelMetadata(metadata);

  // The third passage should be ignored due to max_passages being 2.
  std::vector<float> input_vector = handler->ConstructInputVector(
      passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
      std::vector<passage_embeddings::Embedding>{
          passage_embeddings::Embedding({0.1f, 0.1f, 0.1f}),
          passage_embeddings::Embedding({0.3f, 0.3f, 0.3f}),
          passage_embeddings::Embedding({-1.0f, -1.0f, -1.0f})});
  std::vector<float> expected_input_vector = {0.1f, 0.1f, 0.1f};
  std::vector<float> expected_mean_pooled_vector = {0.2f, 0.2f, 0.2f};
  std::vector<float> expected_max_pooled_vector = {0.3f, 0.3f, 0.3f};
  expected_input_vector.insert(expected_input_vector.end(),
                               expected_mean_pooled_vector.begin(),
                               expected_mean_pooled_vector.end());
  expected_input_vector.insert(expected_input_vector.end(),
                               expected_max_pooled_vector.begin(),
                               expected_max_pooled_vector.end());
  EXPECT_EQ(input_vector, expected_input_vector);
}

}  // namespace

}  // namespace page_content_annotations
