// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/on_device_category_classifier.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/category_classifier_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

using ::testing::_;

class MockEmbedderMetadataProvider
    : public passage_embeddings::EmbedderMetadataProvider {
 public:
  MOCK_METHOD(void,
              AddObserver,
              (passage_embeddings::EmbedderMetadataObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (passage_embeddings::EmbedderMetadataObserver * observer),
              (override));
};

class FakeOptimizationGuideModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    observers_[optimization_target] = observer;
  }

  void PushModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) {
    if (observers_.count(optimization_target)) {
      observers_[optimization_target]->OnModelUpdated(optimization_target,
                                                      model_info);
    }
  }

 private:
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 optimization_guide::OptimizationTargetModelObserver*>
      observers_;
};

class TestObserver : public OnDeviceCategoryClassifier::Observer {
 public:
  void OnCategoriesClassified(
      const GURL& url,
      ukm::SourceId source_id,
      const std::vector<Category>& categories) override {
    last_url_ = url;
    last_categories_ = categories;
    future_.SetValue();
  }

  void Wait() { ASSERT_TRUE(future_.Wait()); }

  const GURL& last_url() const { return last_url_; }
  const std::vector<Category>& last_categories() const {
    return last_categories_;
  }

 private:
  GURL last_url_;
  std::vector<Category> last_categories_;
  base::test::TestFuture<void> future_;
};

class OnDeviceCategoryClassifierTest : public testing::Test {
 public:
  OnDeviceCategoryClassifierTest() = default;
  ~OnDeviceCategoryClassifierTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<FakeOptimizationGuideModelProvider>();
    classifier_ = std::make_unique<OnDeviceCategoryClassifier>(
        model_provider_.get(), &embedder_metadata_provider_);
  }

  void TearDown() override {
    classifier_.reset();
    model_provider_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeOptimizationGuideModelProvider> model_provider_;
  MockEmbedderMetadataProvider embedder_metadata_provider_;
  std::unique_ptr<OnDeviceCategoryClassifier> classifier_;
};

TEST_F(OnDeviceCategoryClassifierTest, SkipsIfEmbedderVersionMissing) {
  // Model version is not set yet.
  TestObserver observer;
  classifier_->AddObserver(&observer);

  passage_embeddings::Embedding embedding(std::vector<float>(768, 0.1f));
  classifier_->OnPageEmbeddingAvailable(GURL("https://example.com"),
                                        /*source_id=*/0, embedding, {});

  observer.Wait();
  EXPECT_TRUE(observer.last_categories().empty());
  classifier_->RemoveObserver(&observer);
}

TEST_F(OnDeviceCategoryClassifierTest, ExecutesIfVersionsMatch) {
  base::HistogramTester histogram_tester;
  classifier_->EmbedderMetadataUpdated(passage_embeddings::EmbedderMetadata(
      /*model_version=*/1, /*output_size=*/768));

  // Update model with metadata.
  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::Any any;
  any.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.CategoryClassifierMetadata");
  metadata.SerializeToString(any.mutable_value());

  auto model_info =
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(base::FilePath(FILE_PATH_LITERAL("model.tflite")))
          .SetModelMetadata(any)
          .Build();

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
      *model_info);

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_SHOPPING_CLASSIFIER,
      *model_info);

  TestObserver observer;
  classifier_->AddObserver(&observer);

  passage_embeddings::Embedding embedding(std::vector<float>(768, 0.1f));
  classifier_->OnPageEmbeddingAvailable(GURL("https://example.com"),
                                        /*source_id=*/0, embedding, {});

  observer.Wait();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency.EduClassifier", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency.ShoppingClassifier",
      1);

  classifier_->RemoveObserver(&observer);
}

TEST_F(OnDeviceCategoryClassifierTest, NoTitleUrlEmbedding) {
  base::HistogramTester histogram_tester;
  classifier_->EmbedderMetadataUpdated(passage_embeddings::EmbedderMetadata(
      /*model_version=*/1, /*output_size=*/768));

  // Update model with metadata.
  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::Any any;
  any.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.CategoryClassifierMetadata");
  metadata.SerializeToString(any.mutable_value());

  auto model_info =
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(base::FilePath(FILE_PATH_LITERAL("model.tflite")))
          .SetModelMetadata(any)
          .Build();

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
      *model_info);

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_SHOPPING_CLASSIFIER,
      *model_info);

  TestObserver observer;
  classifier_->AddObserver(&observer);

  classifier_->OnPageEmbeddingAvailable(GURL("https://example.com"),
                                        /*source_id=*/0, {}, {});

  observer.Wait();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency.EduClassifier", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency.ShoppingClassifier",
      0);

  classifier_->RemoveObserver(&observer);
}

TEST_F(OnDeviceCategoryClassifierTest, SkipsIfVersionsMismatch) {
  classifier_->EmbedderMetadataUpdated(passage_embeddings::EmbedderMetadata(
      /*model_version=*/2, /*output_size=*/768));

  // Update model with metadata for version 1.
  optimization_guide::proto::CategoryClassifierMetadata metadata;
  metadata.set_required_embedder_version(1);
  optimization_guide::proto::Any any;
  any.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.CategoryClassifierMetadata");
  metadata.SerializeToString(any.mutable_value());

  auto model_info =
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(base::FilePath(FILE_PATH_LITERAL("model.tflite")))
          .SetModelMetadata(any)
          .Build();

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
      *model_info);

  TestObserver observer;
  classifier_->AddObserver(&observer);

  passage_embeddings::Embedding embedding(std::vector<float>(768, 0.1f));
  classifier_->OnPageEmbeddingAvailable(GURL("https://example.com"),
                                        /*source_id=*/0, embedding, {});

  observer.Wait();
  EXPECT_TRUE(observer.last_categories().empty());
  classifier_->RemoveObserver(&observer);
}

TEST_F(OnDeviceCategoryClassifierTest, SkipsIfModelMetadataMissing) {
  classifier_->EmbedderMetadataUpdated(passage_embeddings::EmbedderMetadata(
      /*model_version=*/1, /*output_size=*/768));

  // Update model without metadata.
  auto model_info =
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(base::FilePath(FILE_PATH_LITERAL("model.tflite")))
          .Build();

  model_provider_->PushModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
      *model_info);

  TestObserver observer;
  classifier_->AddObserver(&observer);

  passage_embeddings::Embedding embedding(std::vector<float>(768, 0.1f));
  classifier_->OnPageEmbeddingAvailable(GURL("https://example.com"),
                                        /*source_id=*/0, embedding, {});

  observer.Wait();
  EXPECT_TRUE(observer.last_categories().empty());
  classifier_->RemoveObserver(&observer);
}

}  // namespace

}  // namespace page_content_annotations
