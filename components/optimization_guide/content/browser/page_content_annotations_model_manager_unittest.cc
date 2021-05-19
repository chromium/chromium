// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"

#include "base/path_service.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using testing::UnorderedElementsAre;

class PageTopicsModelObserverTracker
    : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    // Make sure we send what is expected.
    if (target != proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_TOPICS) {
      return;
    }
    registered_model_metadata_ = model_metadata;
  }

  absl::optional<proto::Any> registered_model_metadata() const {
    return registered_model_metadata_;
  }

 private:
  absl::optional<proto::Any> registered_model_metadata_;
};

class PageContentAnnotationsModelManagerTest : public testing::Test {
 public:
  PageContentAnnotationsModelManagerTest() = default;
  ~PageContentAnnotationsModelManagerTest() override = default;

  void SetUp() override {
    model_observer_tracker_ =
        std::make_unique<PageTopicsModelObserverTracker>();
    model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
        model_observer_tracker_.get());
  }

  void TearDown() override {
    model_manager_.reset();
    model_observer_tracker_.reset();
  }

  void SendPageTopicsModelToExecutor(const proto::Any& model_metadata) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("bert_page_topics_model.tflite");
    model_manager()->page_topics_model_executor_handle_->OnModelFileUpdated(
        proto::OPTIMIZATION_TARGET_PAGE_TOPICS, model_metadata,
        model_file_path);
    RunUntilIdle();
  }

  PageTopicsModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  PageContentAnnotationsModelManager* model_manager() const {
    return model_manager_.get();
  }

  history::VisitContentModelAnnotations GetContentModelAnnotationsFromOutput(
      const proto::PageTopicsModelMetadata& metadata,
      const std::vector<tflite::task::core::Category>& model_output) const {
    return model_manager()->GetContentModelAnnotationsFromOutput(metadata,
                                                                 model_output);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<PageTopicsModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
};

TEST_F(PageContentAnnotationsModelManagerTest, RegistersCorrectModelMetadata) {
  absl::optional<proto::Any> registered_model_metadata =
      model_observer_tracker()->registered_model_metadata();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  EXPECT_TRUE(registered_model_metadata.has_value());
  absl::optional<proto::PageTopicsModelMetadata> page_topics_model_metadata =
      ParsedAnyMetadata<proto::PageTopicsModelMetadata>(
          *registered_model_metadata);
  EXPECT_TRUE(page_topics_model_metadata.has_value());
  EXPECT_EQ(page_topics_model_metadata->supported_output_size(), 2);
  EXPECT_THAT(
      page_topics_model_metadata->supported_output(),
      UnorderedElementsAre(proto::PAGE_TOPICS_SUPPORTED_OUTPUT_FLOC_PROTECTED,
                           proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES));
#else
  EXPECT_FALSE(registered_model_metadata.has_value());
#endif
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetPageTopicsModelVersionNoPushedModel) {
  EXPECT_FALSE(model_manager()->GetPageTopicsModelVersion().has_value());
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetPageTopicsModelVersionFromExecutor) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageTopicsModelToExecutor(any_metadata);

  absl::optional<int64_t> model_version =
      model_manager()->GetPageTopicsModelVersion();
  EXPECT_TRUE(model_version.has_value());
  EXPECT_EQ(model_version.value(), 123);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetPageTopicsModelVersionFromExecutorBadMetadata) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.whatevernotpagetopics");
  any_metadata.set_value("123");
  SendPageTopicsModelToExecutor(any_metadata);

  absl::optional<int64_t> model_version =
      model_manager()->GetPageTopicsModelVersion();
  EXPECT_FALSE(model_version.has_value());
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetContentModelAnnotationsFromOutputFlocProtectedOnly) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_floc_protected_params()
      ->set_category_name("SOMECATEGORY");

  std::vector<tflite::task::core::Category> model_output = {
      {"SOMECATEGORY", 0.5},
      {"-2", 0.3},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_TRUE(annotations.categories.empty());
  EXPECT_EQ(annotations.floc_protected_score, 0.5);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(
    PageContentAnnotationsModelManagerTest,
    GetContentModelAnnotationsFromOutputFlocProtectedOnlyCategoryNotInOutput) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_floc_protected_params()
      ->set_category_name("SOMECATEGORY");

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.3},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_TRUE(annotations.categories.empty());
  EXPECT_EQ(annotations.floc_protected_score, -1.0);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(
    PageContentAnnotationsModelManagerTest,
    GetContentModelAnnotationsFromOutputNonNumericAndLowWeightCategoriesPruned) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.0001}, {"1", 0.1}, {"SOMECATEGORY", 0.9}, {"2", 0.2}, {"3", 0.3},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_THAT(annotations.categories,
              UnorderedElementsAre(
                  history::VisitContentModelAnnotations::Category(1, 10),
                  history::VisitContentModelAnnotations::Category(2, 20),
                  history::VisitContentModelAnnotations::Category(3, 30)));
  EXPECT_EQ(annotations.floc_protected_score, -1.0);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetContentModelAnnotationsFromOutputNoneWeightTooStrong) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.1);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.9999},
      {"0", 0.3},
      {"1", 0.2},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_TRUE(annotations.categories.empty());
  EXPECT_EQ(annotations.floc_protected_score, -1.0);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetContentModelAnnotationsFromOutputNoneInTopButNotStrongSoPruned) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.1);

  std::vector<tflite::task::core::Category> model_output = {
      {"-2", 0.1}, {"0", 0.3}, {"1", 0.2}, {"2", 0.4}, {"3", 0.05},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_THAT(annotations.categories,
              UnorderedElementsAre(
                  history::VisitContentModelAnnotations::Category(0, 30),
                  history::VisitContentModelAnnotations::Category(1, 20),
                  history::VisitContentModelAnnotations::Category(2, 40)));
  EXPECT_EQ(annotations.floc_protected_score, -1.0);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetContentModelAnnotationsFromOutputPrunedAfterNormalization) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3},
      {"1", 0.25},
      {"2", 0.4},
      {"3", 0.05},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_THAT(annotations.categories,
              UnorderedElementsAre(
                  history::VisitContentModelAnnotations::Category(0, 30),
                  history::VisitContentModelAnnotations::Category(1, 25),
                  history::VisitContentModelAnnotations::Category(2, 40)));
  EXPECT_EQ(annotations.floc_protected_score, -1.0);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetContentModelAnnotationsFromOutputCategoriesAndFlocProtected) {
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(123);
  auto* category_params = model_metadata.mutable_output_postprocessing_params()
                              ->mutable_category_params();
  category_params->set_max_categories(4);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.01);
  category_params->set_min_normalized_weight_within_top_n(0.25);
  model_metadata.mutable_output_postprocessing_params()
      ->mutable_floc_protected_params()
      ->set_category_name("SOMECATEGORY");

  std::vector<tflite::task::core::Category> model_output = {
      {"0", 0.3}, {"1", 0.25}, {"2", 0.4}, {"3", 0.05}, {"SOMECATEGORY", 0.5},
  };
  history::VisitContentModelAnnotations annotations =
      GetContentModelAnnotationsFromOutput(model_metadata, model_output);
  EXPECT_THAT(annotations.categories,
              UnorderedElementsAre(
                  history::VisitContentModelAnnotations::Category(0, 30),
                  history::VisitContentModelAnnotations::Category(1, 25),
                  history::VisitContentModelAnnotations::Category(2, 40)));
  EXPECT_EQ(annotations.floc_protected_score, 0.5);
  EXPECT_EQ(annotations.page_topics_model_version, 123);
}

}  // namespace optimization_guide
