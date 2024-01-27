// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/annotator_impl.h"

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/optimization_guide/proto/page_topics_override_list.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/zlib/google/compression_utils.h"

namespace browsing_topics {

namespace {

const int kTaxonomyVersionV2 = 2;

const char kPageTopicsModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata";

extern const int32_t kTopicsModelVersion = 2;

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
    if (it == registered_model_metadata_.end()) {
      return false;
    }
    *out_model_metadata = registered_model_metadata_.at(target);
    return true;
  }

 private:
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 std::optional<optimization_guide::proto::Any>>
      registered_model_metadata_;
};

class TestAnnotatorImpl : public AnnotatorImpl {
 public:
  TestAnnotatorImpl(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::optional<optimization_guide::proto::Any>& model_metadata)
      : AnnotatorImpl(model_provider, background_task_runner, model_metadata) {}
  ~TestAnnotatorImpl() override = default;

  void ExecuteModelWithInput(ExecutionCallback callback,
                             const std::string& input) override {
    inputs_.push_back(input);
    std::move(callback).Run(std::nullopt);
  }

  const std::vector<std::string>& inputs() const { return inputs_; }

 private:
  std::vector<std::string> inputs_;
};

class BrowsingTopicsAnnotatorImplTest : public testing::Test {
 public:
  BrowsingTopicsAnnotatorImplTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            optimization_guide::features::kPreventLongRunningPredictionModels});
  }
  ~BrowsingTopicsAnnotatorImplTest() override = default;

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    annotator_ = std::make_unique<TestAnnotatorImpl>(
        model_observer_tracker_.get(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        /*model_metadata=*/std::nullopt);
  }

  void TearDown() override {
    annotator_.reset();
    model_observer_tracker_.reset();
    RunUntilIdle();
  }

  void SendModelToAnnotatorSkipWaiting(
      const std::optional<optimization_guide::proto::Any>& model_metadata) {
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
            .SetModelMetadata(model_metadata)
            .Build();
    annotator()->OnModelUpdated(
        optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
        *model_info);
  }

  void SendModelToAnnotator(
      const std::optional<optimization_guide::proto::Any>& model_metadata) {
    SendModelToAnnotatorSkipWaiting(model_metadata);

    base::RunLoop run_loop;
    annotator()->NotifyWhenModelAvailable(run_loop.QuitClosure());
    run_loop.Run();
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  TestAnnotatorImpl* annotator() const { return annotator_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<TestAnnotatorImpl> annotator_;
};

TEST_F(
    BrowsingTopicsAnnotatorImplTest,
    GetContentModelAnnotationsFromOutputNonNumericAndLowWeightCategoriesPruned) {
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendModelToAnnotator(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.0001}, {"1", 0.1}, {"not an int", 0.9}, {"2", 0.2}, {"3", 0.3},
  };

  std::optional<std::vector<int32_t>> categories =
      annotator()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories, testing::UnorderedElementsAre(1, 2, 3));
}

TEST_F(BrowsingTopicsAnnotatorImplTest,
       GetContentModelAnnotationsFromOutputNoneWeightTooStrong) {
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.1);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendModelToAnnotator(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.9999},
      {"0", 0.3},
      {"1", 0.2},
  };

  std::optional<std::vector<int32_t>> categories =
      annotator()->ExtractCategoriesFromModelOutput(model_output);
  EXPECT_FALSE(categories);
}

TEST_F(BrowsingTopicsAnnotatorImplTest,
       GetContentModelAnnotationsFromOutputNoneInTopButNotStrongSoPruned) {
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendModelToAnnotator(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.1}, {"0", 0.3}, {"1", 0.2}, {"2", 0.4}, {"3", 0.05},
  };

  std::optional<std::vector<int32_t>> categories =
      annotator()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories, testing::UnorderedElementsAre(0, 1, 2));
}

TEST_F(BrowsingTopicsAnnotatorImplTest,
       GetContentModelAnnotationsFromOutputPrunedAfterNormalization) {
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendModelToAnnotator(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3},
      {"1", 0.25},
      {"2", 0.4},
      {"3", 0.05},
  };

  std::optional<std::vector<int32_t>> categories =
      annotator()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories, testing::UnorderedElementsAre(0, 1, 2));
}

// Regression test for crbug.com/1303304.
TEST_F(BrowsingTopicsAnnotatorImplTest, NoneCategoryBelowMinWeight) {
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());
  SendModelToAnnotator(any_metadata);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.001}, {"0", 0.001}, {"1", 0.25}, {"2", 0.4}, {"3", 0.05},
  };

  std::optional<std::vector<int32_t>> categories =
      annotator()->ExtractCategoriesFromModelOutput(model_output);
  ASSERT_TRUE(categories);
  EXPECT_THAT(*categories, testing::UnorderedElementsAre(1, 2));
}

TEST_F(BrowsingTopicsAnnotatorImplTest, HostPreprocessingV1) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"www.chromium.org", "chromium org"},
      {"foo-bar.com", "foo bar com"},
      {"foo_bar.com", "foo bar com"},
      {"cats.co.uk", "cats co uk"},
      {"cats+dogs.com", "cats dogs com"},
      {"www.foo-bar_.baz.com", "foo bar  baz com"},
      {"www.foo-bar-baz.com", "foo bar baz com"},
      {"WwW.LOWER-CASE.com", "lower case com"},
      {"m.foo.com", "m foo com"},
      {"www2.foo.com", "www2 foo com"},
  };

  for (const auto& test : tests) {
    std::string raw_host = test.first;
    std::string processed_host = test.second;

    std::string got_input;
    // The callback is run synchronously in this test.
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](std::string* got_input_out,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              *got_input_out = annotations[0].input;
            },
            &got_input),
        {raw_host});
    EXPECT_EQ(raw_host, got_input);
    EXPECT_EQ(processed_host, annotator()->inputs().back());
  }
}

TEST_F(BrowsingTopicsAnnotatorImplTest, HostPreprocessingV2) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"www.chromium.org", "chromium org"},
      {"foo-bar.com", "foo bar com"},
      {"foo_bar.com", "foo bar com"},
      {"cats.co.uk", "cats co uk"},
      {"cats+dogs.com", "cats dogs com"},
      {"www.foo-bar_.baz.com", "foo bar  baz com"},
      {"www.foo-bar-baz.com", "foo bar baz com"},
      {"WwW.LOWER-CASE.com", "lower case com"},
      {"m.foo.com", "foo com"},
      {"web.foo.com", "foo com"},
      {"ftp.foo.com", "foo com"},
      {"www2.foo.com", "foo com"},
      {"home.foo.com", "foo com"},
      {"amp.foo.com", "foo com"},
      {"mobile.foo.com", "foo com"},
      {"wap.foo.com", "foo com"},
      {"w.foo.com", "foo com"},
      {"www-blaah.foo.com", "www blaah foo com"},
      {"www123.foo.com", "foo com"},
      {"www.com", "www com"},
      {"m.com", "m com"},
      {"WEB.foo.com", "foo com"},
  };

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(kTopicsModelVersion);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  model_metadata.SerializeToString(any_metadata.mutable_value());

  SendModelToAnnotator(any_metadata);

  for (const auto& test : tests) {
    std::string raw_host = test.first;
    std::string processed_host = test.second;

    std::string got_input;
    // The callback is run synchronously in this test.
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](std::string* got_input_out,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              *got_input_out = annotations[0].input;
            },
            &got_input),
        {raw_host});
    EXPECT_EQ(raw_host, got_input);
    EXPECT_EQ(processed_host, annotator()->inputs().back());
  }
}

TEST_F(BrowsingTopicsAnnotatorImplTest, PreprocessingNewVersion) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"www.chromium.org", "chromium org"},
      {"foo-bar.com", "foo bar com"},
      {"foo_bar.com", "foo bar com"},
      {"cats.co.uk", "cats co uk"},
      {"cats+dogs.com", "cats dogs com"},
      {"www.foo-bar_.baz.com", "foo bar  baz com"},
      {"www.foo-bar-baz.com", "foo bar baz com"},
      {"WwW.LOWER-CASE.com", "lower case com"},
      {"m.foo.com", "foo com"},
      {"web.foo.com", "foo com"},
      {"ftp.foo.com", "foo com"},
      {"www2.foo.com", "foo com"},
      {"home.foo.com", "foo com"},
      {"amp.foo.com", "foo com"},
      {"mobile.foo.com", "foo com"},
      {"wap.foo.com", "foo com"},
      {"w.foo.com", "foo com"},
      {"www-blaah.foo.com", "www blaah foo com"},
      {"www123.foo.com", "foo com"},
      {"www.com", "www com"},
      {"m.com", "m com"},
      {"WEB.foo.com", "foo com"},
  };

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(kTopicsModelVersion + 1);
  model_metadata.set_taxonomy_version(kTaxonomyVersionV2);
  model_metadata.SerializeToString(any_metadata.mutable_value());

  SendModelToAnnotator(any_metadata);

  for (const auto& test : tests) {
    std::string raw_host = test.first;
    std::string processed_host = test.second;

    std::string got_input;
    // The callback is run synchronously in this test.
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](std::string* got_input_out,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              *got_input_out = annotations[0].input;
            },
            &got_input),
        {raw_host});
    EXPECT_EQ(raw_host, got_input);
    EXPECT_EQ(processed_host, annotator()->inputs().back());
  }
}

TEST_F(BrowsingTopicsAnnotatorImplTest,
       SameTaxonomyVersions_ModelUpdateSuccess) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopicsParameters,
        {{"taxonomy_version", "12345"}}}},
      /*disabled_features=*/{
          optimization_guide::features::kPreventLongRunningPredictionModels});

  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_taxonomy_version(12345);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());

  SendModelToAnnotatorSkipWaiting(any_metadata);

  std::optional<optimization_guide::ModelInfo> model_info =
      annotator()->GetBrowsingTopicsModelInfo();
  EXPECT_TRUE(model_info);
}

TEST_F(BrowsingTopicsAnnotatorImplTest,
       TaxonomyConfiguredVersion1ServerVersionEmpty_ModelUpdateSuccess) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopicsParameters,
        {{"taxonomy_version", "1"}}}},
      /*disabled_features=*/{
          optimization_guide::features::kPreventLongRunningPredictionModels});

  optimization_guide::proto::PageTopicsModelMetadata model_metadata;

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  model_metadata.SerializeToString(any_metadata.mutable_value());

  SendModelToAnnotatorSkipWaiting(any_metadata);

  std::optional<optimization_guide::ModelInfo> model_info =
      annotator()->GetBrowsingTopicsModelInfo();
  EXPECT_TRUE(model_info);
}

class BrowsingTopicsAnnotatorOverrideListTest
    : public BrowsingTopicsAnnotatorImplTest {
 public:
  BrowsingTopicsAnnotatorOverrideListTest() = default;
  ~BrowsingTopicsAnnotatorOverrideListTest() override = default;

  void SetUp() override {
    BrowsingTopicsAnnotatorImplTest::SetUp();
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

  void SendModelWithAdditionalFilesToAnnotator(
      const base::flat_set<base::FilePath>& additional_files) {
    optimization_guide::proto::PageTopicsModelMetadata model_metadata;
    model_metadata.set_version(123);
    model_metadata.set_taxonomy_version(kTaxonomyVersionV2);

    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    model_metadata.SerializeToString(any_metadata.mutable_value());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("browsing_topics")
            // These tests don't need a valid model to execute as we don't care
            // about the model output or execution.
            .AppendASCII("model_doesnt_exist.tflite");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(any_metadata)
            .SetAdditionalFiles(additional_files)
            .Build();
    annotator()->OnModelUpdated(
        optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
        *model_info);

    base::RunLoop run_loop;
    annotator()->NotifyWhenModelAvailable(run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::FilePath& temp_file_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, FileDoesntExist) {
  base::HistogramTester histogram_tester;

  base::FilePath doesnt_exist = temp_file_path().Append(
      base::FilePath(FILE_PATH_LITERAL("override_list.pb.gz")));
  SendModelWithAdditionalFilesToAnnotator({doesnt_exist});

  base::RunLoop run_loop;
  annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "input");
            EXPECT_TRUE(annotations[0].topics.empty());
            run_loop->Quit();
          },
          &run_loop),
      {"input"});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotReadFile=*/2, 1);
  histogram_tester.ExpectTotalCount("BrowsingTopics.OverrideList.UsedOverride",
                                    0);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, BadGzip) {
  base::HistogramTester histogram_tester;

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", std::string());
  SendModelWithAdditionalFilesToAnnotator({add_file});

  base::RunLoop run_loop;
  annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "input");
            EXPECT_TRUE(annotations[0].topics.empty());
            run_loop->Quit();
          },
          &run_loop),
      {"input"});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotUncompressFile=*/3, 1);
  histogram_tester.ExpectTotalCount("BrowsingTopics.OverrideList.UsedOverride",
                                    0);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, BadProto) {
  base::HistogramTester histogram_tester;

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress("bad protobuf"));
  SendModelWithAdditionalFilesToAnnotator({add_file});

  base::RunLoop run_loop;
  annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "input");
            EXPECT_TRUE(annotations[0].topics.empty());
            run_loop->Quit();
          },
          &run_loop),
      {"input"});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kCouldNotUnmarshalProtobuf=*/4, 1);
  histogram_tester.ExpectTotalCount("BrowsingTopics.OverrideList.UsedOverride",
                                    0);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::PageTopicsOverrideList override_list;
  optimization_guide::proto::PageTopicsOverrideEntry* entry =
      override_list.add_entries();
  entry->set_domain("input com");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToAnnotator({add_file});

  base::RunLoop run_loop;
  annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "www.input.com");
            EXPECT_EQ(annotations[0].topics, std::vector<int32_t>{1337});
            run_loop->Quit();
          },
          &run_loop),
      {"www.input.com"});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 1);
  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.UsedOverride", true, 1);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, InputNotInOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::PageTopicsOverrideList override_list;
  optimization_guide::proto::PageTopicsOverrideEntry* entry =
      override_list.add_entries();
  entry->set_domain("other");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToAnnotator({add_file});

  base::RunLoop run_loop;
  annotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "input");
            EXPECT_TRUE(annotations[0].topics.empty());
            run_loop->Quit();
          },
          &run_loop),
      {"input"});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 1);
  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.UsedOverride", false, 1);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, ModelUnloadsOverrideList) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::PageTopicsOverrideList override_list;
  optimization_guide::proto::PageTopicsOverrideEntry* entry =
      override_list.add_entries();
  entry->set_domain("input");
  entry->mutable_topics()->add_topic_ids(1337);

  std::string enc_pb;
  ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

  base::FilePath add_file =
      WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
  SendModelWithAdditionalFilesToAnnotator({add_file});

  {
    base::RunLoop run_loop;
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](base::RunLoop* run_loop,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              EXPECT_EQ(annotations[0].input, "input");
              EXPECT_EQ(annotations[0].topics, std::vector<int32_t>{1337});
              run_loop->Quit();
            },
            &run_loop),
        {"input"});
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "BrowsingTopics.OverrideList.UsedOverride", true, 1);
  }

  // Retry an execution and check that the UMA reports the override list being
  // loaded twice.
  {
    base::RunLoop run_loop;
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](base::RunLoop* run_loop,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              EXPECT_EQ(annotations[0].input, "input");
              EXPECT_EQ(annotations[0].topics, std::vector<int32_t>{1337});
              run_loop->Quit();
            },
            &run_loop),
        {"input"});
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "BrowsingTopics.OverrideList.UsedOverride", true, 2);
  }

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 2);
}

TEST_F(BrowsingTopicsAnnotatorOverrideListTest, NewModelUnloadsOverrideList) {
  base::HistogramTester histogram_tester;

  {
    optimization_guide::proto::PageTopicsOverrideList override_list;
    optimization_guide::proto::PageTopicsOverrideEntry* entry =
        override_list.add_entries();
    entry->set_domain("input");
    entry->mutable_topics()->add_topic_ids(1337);

    std::string enc_pb;
    ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

    base::FilePath add_file =
        WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
    SendModelWithAdditionalFilesToAnnotator({add_file});

    base::RunLoop run_loop;
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](base::RunLoop* run_loop,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              EXPECT_EQ(annotations[0].input, "input");
              EXPECT_EQ(annotations[0].topics, std::vector<int32_t>{1337});
              run_loop->Quit();
            },
            &run_loop),
        {"input"});
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "BrowsingTopics.OverrideList.UsedOverride", true, 1);
  }

  // Retry an execution and check that the UMA reports the override list being
  // loaded twice, and that the topics are now different.
  {
    optimization_guide::proto::PageTopicsOverrideList override_list;
    optimization_guide::proto::PageTopicsOverrideEntry* entry =
        override_list.add_entries();
    entry->set_domain("input");
    entry->mutable_topics()->add_topic_ids(7331);

    std::string enc_pb;
    ASSERT_TRUE(override_list.SerializeToString(&enc_pb));

    base::FilePath add_file =
        WriteToTempFile("override_list.pb.gz", Compress(enc_pb));
    SendModelWithAdditionalFilesToAnnotator({add_file});

    base::RunLoop run_loop;
    annotator()->BatchAnnotate(
        base::BindOnce(
            [](base::RunLoop* run_loop,
               const std::vector<Annotation>& annotations) {
              ASSERT_EQ(annotations.size(), 1U);
              EXPECT_EQ(annotations[0].input, "input");
              EXPECT_EQ(annotations[0].topics, std::vector<int32_t>{7331});
              run_loop->Quit();
            },
            &run_loop),
        {"input"});
    run_loop.Run();
  }

  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.FileLoadResult",
      /*OverrideListFileLoadResult::kSuccess=*/1, 2);
  histogram_tester.ExpectUniqueSample(
      "BrowsingTopics.OverrideList.UsedOverride", true, 2);
}

}  // namespace

}  // namespace browsing_topics
