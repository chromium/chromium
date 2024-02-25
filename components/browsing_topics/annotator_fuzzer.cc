// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browsing_topics/annotator_impl.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"

// Sends a fully working model and configuration to the calling observer.
class ModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  ModelProvider() = default;
  ~ModelProvider() override = default;

  // optimization_guide::TestOptimizationGuideModelProvider:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    optimization_guide::proto::PageTopicsModelMetadata
        page_topics_model_metadata;
    page_topics_model_metadata.set_version(123);
    page_topics_model_metadata.add_supported_output(
        optimization_guide::proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
    auto* output_params =
        page_topics_model_metadata.mutable_output_postprocessing_params();
    auto* category_params = output_params->mutable_category_params();
    category_params->set_max_categories(5);
    category_params->set_min_none_weight(0.8);
    category_params->set_min_category_weight(0.1);
    category_params->set_min_normalized_weight_within_top_n(0.1);
    page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("browsing_topics")
            .AppendASCII("golden_data_model.tflite");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(any_metadata)
            .Build();

    observer->OnModelUpdated(
        optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
        *model_info);
  }
};

// An AnnotatorImpl that never unloads the model, thus keeping the executions
// per second high.
class TestAnnotator : public browsing_topics::AnnotatorImpl {
 public:
  TestAnnotator(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::optional<optimization_guide::proto::Any>& model_metadata)
      : browsing_topics::AnnotatorImpl(model_provider,
                                       std::move(background_task_runner),
                                       model_metadata) {}

 protected:
  // AnnotatorImpl:
  void UnloadModel() override {
    // Do nothing so that the model stays loaded.
  }
};

// Static test fixture to maintain the state needed to run repeated fuzz tests.
class AnnotatorFuzzerTest {
 public:
  explicit AnnotatorFuzzerTest()
      : annotator_(std::make_unique<TestAnnotator>(
            &model_provider_,
            task_environment_.GetMainThreadTaskRunner(),
            std::nullopt)) {
    scoped_feature_list_.InitAndDisableFeature(
        optimization_guide::features::kPreventLongRunningPredictionModels);
  }
  ~AnnotatorFuzzerTest() {
    annotator_.reset();
    // To prevent ASAN issues, ensure the ModelExecutor gets destroyed since
    // that is done using a DeleteSoon PostTask.
    task_environment_.RunUntilIdle();
  }

  browsing_topics::Annotator* annotator() { return annotator_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ModelProvider model_provider_;
  std::unique_ptr<browsing_topics::AnnotatorImpl> annotator_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static AnnotatorFuzzerTest test;
  static bool had_successful_run = false;

  std::string input(reinterpret_cast<const char*>(data), size);

  base::RunLoop run_loop;
  test.annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<browsing_topics::Annotation>& annotations) {
            if (!annotations[0].topics.empty() && !had_successful_run) {
              had_successful_run = true;
              // Print a single debug message so that its obvious things are
              // working (or able to at least once) when running locally.
              LOG(INFO) << "Congrats! Got a successful model execution. This "
                           "message will not be printed again.";
            }

            run_loop->Quit();
          },
          &run_loop),
      {input});
  run_loop.Run();

  // The model executor does some PostTask'ing to manage its state. While these
  // tasks are not important for fuzzing, we don't want to queue up a ton of
  // them.
  test.RunUntilIdle();

  return 0;
}
