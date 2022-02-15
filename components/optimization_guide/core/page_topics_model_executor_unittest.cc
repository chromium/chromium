// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_topics_model_executor.h"

#include "base/containers/flat_map.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/page_entities_model_executor.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
  }

  bool DidRegisterForTarget(
      proto::OptimizationTarget target,
      absl::optional<proto::Any>* out_model_metadata) const {
    auto it = registered_model_metadata_.find(target);
    if (it == registered_model_metadata_.end())
      return false;
    *out_model_metadata = registered_model_metadata_.at(target);
    return true;
  }

 private:
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_model_metadata_;
};

class PageTopicsModelExecutorTest : public testing::Test {
 public:
  PageTopicsModelExecutorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPageContentAnnotations);
  }
  ~PageTopicsModelExecutorTest() override = default;

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    model_executor_ = std::make_unique<PageTopicsModelExecutor>(
        model_observer_tracker_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        /*model_metadata=*/absl::nullopt);
  }

  void TearDown() override {
    model_executor_.reset();
    model_observer_tracker_.reset();
    RunUntilIdle();
  }

  void SendPageTopicsModelToExecutor(
      const absl::optional<proto::Any>& model_metadata) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("bert_page_topics_model.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(model_metadata)
            .Build();
    model_executor()->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
                                     *model_info);
    RunUntilIdle();
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  PageTopicsModelExecutor* model_executor() const {
    return model_executor_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageTopicsModelExecutor> model_executor_;
};

TEST_F(
    PageTopicsModelExecutorTest,
    GetContentModelAnnotationsFromOutputNonNumericAndLowWeightCategoriesPruned) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.0001}, {"1", 0.1}, {"not an int", 0.9}, {"2", 0.2}, {"3", 0.3},
  };

  absl::optional<std::vector<WeightedIdentifier>> categories =
      model_executor()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories,
              testing::UnorderedElementsAre(WeightedIdentifier(1, 0.1),
                                            WeightedIdentifier(2, 0.2),
                                            WeightedIdentifier(3, 0.3)));
}

TEST_F(PageTopicsModelExecutorTest,
       GetContentModelAnnotationsFromOutputNoneWeightTooStrong) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.1);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.9999},
      {"0", 0.3},
      {"1", 0.2},
  };

  absl::optional<std::vector<WeightedIdentifier>> categories =
      model_executor()->ExtractCategoriesFromModelOutput(model_output);
  EXPECT_FALSE(categories);
}

TEST_F(PageTopicsModelExecutorTest,
       GetContentModelAnnotationsFromOutputNoneInTopButNotStrongSoPruned) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.1}, {"0", 0.3}, {"1", 0.2}, {"2", 0.4}, {"3", 0.05},
  };

  absl::optional<std::vector<WeightedIdentifier>> categories =
      model_executor()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories,
              testing::UnorderedElementsAre(WeightedIdentifier(0, 0.3),
                                            WeightedIdentifier(1, 0.2),
                                            WeightedIdentifier(2, 0.4)));
}

TEST_F(PageTopicsModelExecutorTest,
       GetContentModelAnnotationsFromOutputPrunedAfterNormalization) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3},
      {"1", 0.25},
      {"2", 0.4},
      {"3", 0.05},
  };

  absl::optional<std::vector<WeightedIdentifier>> categories =
      model_executor()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories,
              testing::UnorderedElementsAre(WeightedIdentifier(0, 0.3),
                                            WeightedIdentifier(1, 0.25),
                                            WeightedIdentifier(2, 0.4)));
}

TEST_F(PageTopicsModelExecutorTest,
       PostprocessCategoriesToBatchAnnotationResult) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3},
      {"1", 0.25},
      {"2", 0.4},
      {"3", 0.05},
  };

  BatchAnnotationResult topics_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult("");
  model_executor()->PostprocessCategoriesToBatchAnnotationResult(
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &topics_result),
      AnnotationType::kPageTopics, "input", model_output);
  EXPECT_EQ(topics_result, BatchAnnotationResult::CreatePageTopicsResult(
                               "input", std::vector<WeightedIdentifier>{
                                            WeightedIdentifier(0, 0.3),
                                            WeightedIdentifier(1, 0.25),
                                            WeightedIdentifier(2, 0.4),
                                        }));
}

TEST_F(PageTopicsModelExecutorTest,
       NullPostprocessCategoriesToBatchAnnotationResult) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  BatchAnnotationResult topics_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult("");
  model_executor()->PostprocessCategoriesToBatchAnnotationResult(
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &topics_result),
      AnnotationType::kPageTopics, "", absl::nullopt);
  EXPECT_EQ(topics_result,
            BatchAnnotationResult::CreatePageTopicsResult("", absl::nullopt));
}

}  // namespace optimization_guide