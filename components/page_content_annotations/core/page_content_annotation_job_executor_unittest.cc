// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotation_job_executor.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {
const double kOutput = 0.5;
}

class TestJobExecutor : public PageContentAnnotationJobExecutor {
 public:
  TestJobExecutor() = default;
  virtual ~TestJobExecutor() = default;

  // PageContentAnnotationJobExecutor:
  void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback)
      override {
    std::move(callback).Run(
        BatchAnnotationResult::CreateContentVisibilityResult(input, kOutput));
  }
};

class PageContentAnnotationJobExecutorTest : public testing::Test {
 public:
  PageContentAnnotationJobExecutorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPageContentAnnotations);
  }
  ~PageContentAnnotationJobExecutorTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationJobExecutorTest, FullFlow) {
  TestJobExecutor job_executor;
  std::vector<BatchAnnotationResult> results;

  // This callback will be run before the run loop quit closure below.
  BatchAnnotationCallback outside_callers_result_callback = base::BindOnce(
      [](std::vector<BatchAnnotationResult>* out_results,
         const std::vector<BatchAnnotationResult>& in_results) {
        *out_results = in_results;
      },
      &results);

  std::unique_ptr<PageContentAnnotationJob> job =
      std::make_unique<PageContentAnnotationJob>(
          std::move(outside_callers_result_callback),
          std::vector<std::string>{"input1", "input2"},
          AnnotationType::kContentVisibility);

  // Actual model execution can take a little while, so try to keep tests from
  // flaking.
  base::test::ScopedRunLoopTimeout scoped_timeout(FROM_HERE, base::Seconds(60));

  base::RunLoop run_loop;
  job_executor.ExecuteJob(run_loop.QuitClosure(), std::move(job));
  run_loop.Run();

  ASSERT_EQ(2U, results.size());
  EXPECT_EQ(results[0].input(), "input1");
  EXPECT_EQ(results[0].visibility_score(), std::make_optional(kOutput));
  EXPECT_EQ(results[1].input(), "input2");
  EXPECT_EQ(results[1].visibility_score(), std::make_optional(kOutput));
}

}  // namespace page_content_annotations
