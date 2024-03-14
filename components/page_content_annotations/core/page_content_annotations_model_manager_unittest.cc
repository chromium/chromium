// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_model_manager.h"

#include "base/containers/flat_map.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

using ::testing::FloatEq;
using ::testing::UnorderedElementsAre;

class ModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
  }

  bool DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget target,
      std::optional<optimization_guide::proto::Any>* out_model_metadata) const {
    auto it = registered_model_metadata_.find(target);
    if (it == registered_model_metadata_.end())
      return false;
    if (out_model_metadata)
      *out_model_metadata = registered_model_metadata_.at(target);
    return true;
  }

 private:
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 std::optional<optimization_guide::proto::Any>>
      registered_model_metadata_;
};

class PageContentAnnotationsModelManagerTest : public testing::Test {
 public:
  PageContentAnnotationsModelManagerTest() {
    // Enable Visibility but disable Entities.
    scoped_feature_list_.InitWithFeatures(
        {features::kPageVisibilityPageContentAnnotations},
        {optimization_guide::features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsModelManagerTest() override = default;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
        model_observer_tracker_.get());
  }

  void TearDown() override {
    model_manager_.reset();
    model_observer_tracker_.reset();
  }

  void SendPageVisibilityModelToExecutor() {
    model_manager()->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kContentVisibility, base::DoNothing());
    // If the feature flag is disabled, the executor won't have been created so
    // skip everything else.
    if (!model_manager()->page_visibility_model_handler_)
      return;

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    // We know that the model executor itself works fine (that's tested
    // elsewhere), so just make sure that all the plumbing for the model
    // execution: job, queue, background sequences, etc, are working correctly.
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("non_existent_model.tflite");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .Build();
    model_manager()->page_visibility_model_handler_->OnModelUpdated(
        optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
        *model_info);
    RunUntilIdle();
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  PageContentAnnotationsModelManager* model_manager() const {
    return model_manager_.get();
  }

  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
};

TEST_F(PageContentAnnotationsModelManagerTest, PageVisibility) {
  base::HistogramTester histogram_tester;
  SendPageVisibilityModelToExecutor();

  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kContentVisibility);
  run_loop.Run();

  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PAGE_VISIBILITY,
      nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "ContentVisibility",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.ContentVisibility",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime."
      "ContentVisibility",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime."
      "ContentVisibility",
      1);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].visibility_score(), std::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, PageVisibilityDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPageVisibilityBatchAnnotations);

  SendPageVisibilityModelToExecutor();

  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kContentVisibility);
  run_loop.Run();

  EXPECT_FALSE(model_observer_tracker()->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PAGE_VISIBILITY,
      nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "ContentVisibility",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.ContentVisibility",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime."
      "ContentVisibility",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime."
      "ContentVisibility",
      1);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].visibility_score(), std::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, CalledTwice) {
  SendPageVisibilityModelToExecutor();

  base::HistogramTester histogram_tester;

  base::RunLoop run_loop1;
  std::vector<BatchAnnotationResult> result1;
  BatchAnnotationCallback callback1 = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop1, &result1);

  model_manager()->Annotate(std::move(callback1), {"input1"},
                            AnnotationType::kContentVisibility);

  base::RunLoop run_loop2;
  std::vector<BatchAnnotationResult> result2;
  BatchAnnotationCallback callback2 = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop2, &result2);

  model_manager()->Annotate(std::move(callback2), {"input2"},
                            AnnotationType::kContentVisibility);

  run_loop1.Run();
  run_loop2.Run();

  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PAGE_VISIBILITY,
      nullptr));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "ContentVisibility",
      1, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.ContentVisibility",
      false, 2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime."
      "ContentVisibility",
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime."
      "ContentVisibility",
      2);

  // The model should have only been loaded once and then used for both jobs.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad.PageVisibility",
      true, 2);

  ASSERT_EQ(result1.size(), 1U);
  EXPECT_EQ(result1[0].input(), "input1");
  EXPECT_EQ(result1[0].type(), AnnotationType::kContentVisibility);
  EXPECT_EQ(result1[0].visibility_score(), std::nullopt);
  ASSERT_EQ(result2.size(), 1U);
  EXPECT_EQ(result2[0].input(), "input2");
  EXPECT_EQ(result2[0].type(), AnnotationType::kContentVisibility);
  EXPECT_EQ(result2[0].visibility_score(), std::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       NotifyWhenModelAvailable_VisibilityOnly) {
  SendPageVisibilityModelToExecutor();

  base::RunLoop visibility_run_loop;
  bool visibility_callback_success = false;

  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kContentVisibility,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &visibility_run_loop, &visibility_callback_success));

  visibility_run_loop.Run();

  EXPECT_TRUE(visibility_callback_success);
}

}  // namespace page_content_annotations
