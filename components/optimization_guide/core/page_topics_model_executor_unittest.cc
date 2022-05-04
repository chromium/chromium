// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_topics_model_executor.h"

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/page_entities_model_executor.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/optimization_guide/proto/page_topics_override_list.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

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

// Regression test for crbug.com/1303304.
TEST_F(PageTopicsModelExecutorTest, NoneCategoryBelowMinWeight) {
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
      {"-2", 0.001}, {"0", 0.001}, {"1", 0.25}, {"2", 0.4}, {"3", 0.05},
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

class PageTopicsModelExecutorOverrideListTest
    : public PageTopicsModelExecutorTest {
 public:
  PageTopicsModelExecutorOverrideListTest() = default;
  ~PageTopicsModelExecutorOverrideListTest() override = default;

  void SetUp() override {
    PageTopicsModelExecutorTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath WriteToTempFile(const std::string& base_name,
                                 const std::string& contents) {
    base::FilePath abs_path = temp_dir_.GetPath().AppendASCII(base_name);
    EXPECT_TRUE(base::WriteFile(abs_path, contents));
    return abs_path;
  }

  std::string Compress(const std::string& data) {
    std::string compressed;
    EXPECT_TRUE(compression::GzipCompress(data, &compressed));
    return compressed;
  }

  void SendModelWithAdditionalFilesToExecutor(
      const base::flat_set<base::FilePath>& additional_files) {
    proto::PageTopicsModelMetadata model_metadata;
    model_metadata.set_version(123);

    proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    model_metadata.SerializeToString(any_metadata.mutable_value());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            // These tests don't need a valid model to execute as we don't care
            // about the model output or execution.
            .AppendASCII("model_doesnt_exist.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(any_metadata)
            .SetAdditionalFiles(additional_files)
            .Build();
    model_executor()->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
                                     *model_info);
    RunUntilIdle();
  }

  const base::FilePath& temp_file_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(PageTopicsModelExecutorOverrideListTest, NoAdditionalFiles) {
  base::HistogramTester histogram_tester;
  SendModelWithAdditionalFilesToExecutor({});

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", false, 1);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, WrongAdditionalFileName) {
  base::HistogramTester histogram_tester;

  base::FilePath add_file =
      WriteToTempFile("tsil_eidrrevo.pb.gz", "file contents");
  SendModelWithAdditionalFilesToExecutor({add_file});

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", false, 1);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, FileDoesntExist) {
  base::HistogramTester histogram_tester;

  base::FilePath doesnt_exist = temp_file_path().Append(
      base::FilePath(FILE_PATH_LITERAL("override_list.pb.gz")));
  SendModelWithAdditionalFilesToExecutor({doesnt_exist});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::DoNothing(), std::vector<std::string>{"inputs"},
          AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotReadFile=*/2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", 0);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, BadGzip) {
  base::HistogramTester histogram_tester;

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", std::string());
  SendModelWithAdditionalFilesToExecutor({add_file});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::DoNothing(), std::vector<std::string>{"inputs"},
          AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotUncompressFile=*/3, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", 0);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, BadProto) {
  base::HistogramTester histogram_tester;

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress("bad protobuf"));
  SendModelWithAdditionalFilesToExecutor({add_file});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::DoNothing(), std::vector<std::string>{"inputs"},
          AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotUnmarshalProtobuf=*/4, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", 0);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  proto::PageTopicsOverrideList override_list;
  proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
  entry->set_domain("input");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToExecutor({add_file});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::BindOnce([](const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].input(), "input");
            EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
            ASSERT_TRUE(results[0].topics());
            EXPECT_EQ(*results[0].topics(), (std::vector<WeightedIdentifier>{
                                                WeightedIdentifier(1337, 1.0),
                                            }));
          }),
          std::vector<std::string>{"input"}, AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", true, 1);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, InputNotInOverride) {
  base::HistogramTester histogram_tester;

  proto::PageTopicsOverrideList override_list;
  proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
  entry->set_domain("other");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToExecutor({add_file});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::BindOnce([](const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].input(), "input");
            EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
            // The passed model file isn't valid so we don't expect an output
            // here.
            EXPECT_FALSE(results[0].topics());
          }),
          std::vector<std::string>{"input"}, AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", false, 1);
}

// Regression test for crbug.com/1321808.
TEST_F(PageTopicsModelExecutorOverrideListTest, KeepsOrdering) {
  base::HistogramTester histogram_tester;

  proto::PageTopicsOverrideList override_list;
  proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
  entry->set_domain("in list");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToExecutor({add_file});

  base::RunLoop run_loop;
  model_executor()->ExecuteJob(
      run_loop.QuitClosure(),
      std::make_unique<PageContentAnnotationJob>(
          base::BindOnce([](const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 2U);

            EXPECT_EQ(results[0].input(), "not in list");
            EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
            EXPECT_FALSE(results[0].topics());

            EXPECT_EQ(results[1].input(), "in list");
            EXPECT_EQ(results[1].type(), AnnotationType::kPageTopics);
            EXPECT_TRUE(results[1].topics());
          }),
          std::vector<std::string>{"not in list", "in list"},
          AnnotationType::kPageTopics));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, ModelUnloadsOverrideList) {
  base::HistogramTester histogram_tester;

  proto::PageTopicsOverrideList override_list;
  proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
  entry->set_domain("input");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToExecutor({add_file});

  {
    base::RunLoop run_loop;
    model_executor()->ExecuteJob(
        run_loop.QuitClosure(),
        std::make_unique<PageContentAnnotationJob>(
            base::DoNothing(), std::vector<std::string>{"input"},
            AnnotationType::kPageTopics));
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageTopicsOverrideList.UsedOverride", true, 1);
  }

  // Request the model to be unloaded, which should also unload the override
  // list.
  model_executor()->UnloadModel();

  // Retry an execution and check that the UMA reports the override list being
  // loaded twice.
  {
    base::RunLoop run_loop;
    model_executor()->ExecuteJob(
        run_loop.QuitClosure(),
        std::make_unique<PageContentAnnotationJob>(
            base::DoNothing(), std::vector<std::string>{"input"},
            AnnotationType::kPageTopics));
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageTopicsOverrideList.UsedOverride", true, 2);
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 1);
}

TEST_F(PageTopicsModelExecutorOverrideListTest, NewModelUnloadsOverrideList) {
  base::HistogramTester histogram_tester;

  {
    proto::PageTopicsOverrideList override_list;
    proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
    entry->set_domain("input");
    entry->mutable_topics()->add_topic_ids(1337);

    std::string enc_pb;
    ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

    base::FilePath add_file =
        WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
    SendModelWithAdditionalFilesToExecutor({add_file});

    base::RunLoop run_loop;
    model_executor()->ExecuteJob(
        run_loop.QuitClosure(),
        std::make_unique<PageContentAnnotationJob>(
            base::BindOnce(
                [](const std::vector<BatchAnnotationResult>& results) {
                  ASSERT_EQ(results.size(), 1U);
                  EXPECT_EQ(results[0].input(), "input");
                  EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
                  ASSERT_TRUE(results[0].topics());
                  EXPECT_EQ(*results[0].topics(),
                            (std::vector<WeightedIdentifier>{
                                WeightedIdentifier(1337, 1.0),
                            }));
                }),
            std::vector<std::string>{"input"}, AnnotationType::kPageTopics));
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageTopicsOverrideList.UsedOverride", true, 1);
  }

  // Retry an execution and check that the UMA reports the override list being
  // loaded twice, and that the topics are now different.
  {
    proto::PageTopicsOverrideList override_list;
    proto::PageTopicsOverrideEntry* entry = override_list.add_entries();
    entry->set_domain("input");
    entry->mutable_topics()->add_topic_ids(7331);

    std::string enc_pb;
    ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

    base::FilePath add_file =
        WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
    SendModelWithAdditionalFilesToExecutor({add_file});

    base::RunLoop run_loop;
    model_executor()->ExecuteJob(
        run_loop.QuitClosure(),
        std::make_unique<PageContentAnnotationJob>(
            base::BindOnce(
                [](const std::vector<BatchAnnotationResult>& results) {
                  ASSERT_EQ(results.size(), 1U);
                  EXPECT_EQ(results[0].input(), "input");
                  EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
                  ASSERT_TRUE(results[0].topics());
                  EXPECT_EQ(*results[0].topics(),
                            (std::vector<WeightedIdentifier>{
                                WeightedIdentifier(7331, 1.0),
                            }));
                }),
            std::vector<std::string>{"input"}, AnnotationType::kPageTopics));
    run_loop.Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.GotFile", true, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTopicsOverrideList.UsedOverride", true, 2);
}

}  // namespace optimization_guide
