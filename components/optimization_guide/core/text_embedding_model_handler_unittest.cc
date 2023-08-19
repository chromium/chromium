// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/text_embedding_model_handler.h"

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class TextEmbeddingModelHandlerTest : public testing::Test {
 public:
  TextEmbeddingModelHandlerTest() = default;
  ~TextEmbeddingModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<TestOptimizationGuideModelProvider>();
    model_handler_ = std::make_unique<TextEmbeddingModelHandler>(
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner(),
        /*model_metadata=*/absl::nullopt);
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    RunUntilIdle();
  }

  TextEmbeddingModelHandler* model_handler() const {
    return model_handler_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestOptimizationGuideModelProvider> model_provider_;
  std::unique_ptr<TextEmbeddingModelHandler> model_handler_;
};

TEST_F(TextEmbeddingModelHandlerTest, ShouldExtractEmbedding) {
  tflite::task::processor::EmbeddingResult result;
  auto* embedding = result.add_embeddings();
  embedding->mutable_feature_vector()->add_value_float(1.0);
  embedding->mutable_feature_vector()->add_value_float(2.0);
  embedding->mutable_feature_vector()->add_value_float(3.0);
  embedding->mutable_feature_vector()->add_value_float(4.0);

  BatchAnnotationResult embedding_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string());

  base::OnceCallback<void(const BatchAnnotationResult&)> callback =
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &embedding_result);

  model_handler()->PostprocessEmbeddingsToBatchAnnotationResult(
      std::move(callback), AnnotationType::kTextEmbedding, "input", result);

  ASSERT_TRUE(embedding_result.embeddings());
  EXPECT_EQ(embedding_result.type(), AnnotationType::kTextEmbedding);

  std::vector<float> expected_result = {1.0, 2.0, 3.0, 4.0};
  EXPECT_EQ(embedding_result.embeddings(), expected_result);
}

TEST_F(TextEmbeddingModelHandlerTest, ShouldNotExtractTwoEmbeddings) {
  tflite::task::processor::EmbeddingResult result;
  result.add_embeddings();
  result.add_embeddings();

  BatchAnnotationResult embedding_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string());

  base::OnceCallback<void(const BatchAnnotationResult&)> callback =
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &embedding_result);

  model_handler()->PostprocessEmbeddingsToBatchAnnotationResult(
      std::move(callback), AnnotationType::kTextEmbedding, "input", result);

  EXPECT_EQ(embedding_result.embeddings(), absl::nullopt);
}

TEST_F(TextEmbeddingModelHandlerTest, HasNoEmbeddings) {
  tflite::task::processor::EmbeddingResult result;

  BatchAnnotationResult embedding_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string());

  base::OnceCallback<void(const BatchAnnotationResult&)> callback =
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &embedding_result);

  model_handler()->PostprocessEmbeddingsToBatchAnnotationResult(
      std::move(callback), AnnotationType::kTextEmbedding, "input", result);

  EXPECT_EQ(embedding_result.embeddings(), absl::nullopt);
}

}  // namespace optimization_guide
