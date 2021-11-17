// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_visibility_model_executor.h"

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

class PageVisibilityModelExecutorTest : public testing::Test {
 public:
  PageVisibilityModelExecutorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPageContentAnnotations);
  }
  ~PageVisibilityModelExecutorTest() override = default;

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    model_executor_ = std::make_unique<PageVisibilityModelExecutor>(
        model_observer_tracker_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        /*model_metadata=*/absl::nullopt);
  }

  void TearDown() override {
    model_executor_.reset();
    model_observer_tracker_.reset();
    RunUntilIdle();
  }

  void SendPageVisibilityModelToExecutor(
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
    model_executor()->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
                                     *model_info);
    RunUntilIdle();
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  PageVisibilityModelExecutor* model_executor() const {
    return model_executor_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageVisibilityModelExecutor> model_executor_;
};

TEST_F(PageVisibilityModelExecutorTest, NoModelMetadataNoOutput) {
  // Note that |SendPageVisibilityModelToExecutor| is not called so no metadata
  // has been loaded.

  std::vector<tflite::task::core::Category> model_output = {
      {"VISIBILITY_HERE", 0.3},
  };

  absl::optional<double> score =
      model_executor()->ExtractContentVisibilityFromModelOutput(model_output);
  EXPECT_FALSE(score);
}

TEST_F(PageVisibilityModelExecutorTest, NoParamsNoOutput) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"VISIBILITY_HERE", 0.3},
  };

  absl::optional<double> score =
      model_executor()->ExtractContentVisibilityFromModelOutput(model_output);
  EXPECT_FALSE(score);
}

TEST_F(PageVisibilityModelExecutorTest, VisibilityNotEvaluated) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("VISIBILITY_HERE");

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"something else", 0.3},
  };

  absl::optional<double> score =
      model_executor()->ExtractContentVisibilityFromModelOutput(model_output);
  ASSERT_TRUE(score);
  EXPECT_THAT(*score, testing::DoubleEq(-1));
}

TEST_F(PageVisibilityModelExecutorTest, SuccessCase) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("VISIBILITY_HERE");

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"VISIBILITY_HERE", 0.3},
      {"0", 0.4},
      {"1", 0.5},
  };

  absl::optional<double> score =
      model_executor()->ExtractContentVisibilityFromModelOutput(model_output);
  ASSERT_TRUE(score);
  EXPECT_THAT(*score, testing::DoubleEq(0.7));
}

TEST_F(PageVisibilityModelExecutorTest,
       PostprocessCategoriesToBatchAnnotationResult) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("VISIBILITY_HERE");

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3},
      {"1", 0.25},
      {"2", 0.4},
      {"3", 0.05},
      {"VISIBILITY_HERE", 0.4},
  };

  BatchAnnotationResult viz_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult("");
  model_executor()->PostprocessCategoriesToBatchAnnotationResult(
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &viz_result),
      AnnotationType::kContentVisibility, "input", model_output);
  EXPECT_EQ(viz_result,
            BatchAnnotationResult::CreateContentVisibilityResult("input", 0.6));
}

TEST_F(PageVisibilityModelExecutorTest,
       NullPostprocessCategoriesToBatchAnnotationResult) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("VISIBILITY_HERE");

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  BatchAnnotationResult viz_result =
      BatchAnnotationResult::CreateEmptyAnnotationsResult("");
  model_executor()->PostprocessCategoriesToBatchAnnotationResult(
      base::BindOnce(
          [](BatchAnnotationResult* out_result,
             const BatchAnnotationResult& in_result) {
            *out_result = in_result;
          },
          &viz_result),
      AnnotationType::kContentVisibility, "input", absl::nullopt);
  EXPECT_EQ(viz_result, BatchAnnotationResult::CreateContentVisibilityResult(
                            "input", absl::nullopt));
}

}  // namespace optimization_guide