// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_entities_model_handler_impl.h"

#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/entity_annotator_native_library.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using ::testing::ElementsAre;

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
    registered_observers_.AddObserver(observer);
  }

  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      OptimizationTargetModelObserver* observer) override {
    registered_observers_.RemoveObserver(observer);
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

  void PushModelInfoToObservers(const ModelInfo& model_info) {
    for (auto& observer : registered_observers_) {
      observer.OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES,
                              model_info);
    }
  }

 private:
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_model_metadata_;
  base::ObserverList<OptimizationTargetModelObserver> registered_observers_;
};

class PageEntitiesModelHandlerImplTest : public testing::Test {
 public:
  PageEntitiesModelHandlerImplTest() {
    PageEntitiesModelHandlerConfig config;
    // The false variation is tested in a src-internal test.
    config.should_provide_filter_path = true;
    SetPageEntitiesModelHandlerConfigForTesting(config);
  }

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();

    model_executor_ = std::make_unique<PageEntitiesModelHandlerImpl>(
        model_observer_tracker_.get(),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));

    // Wait for PageEntitiesModelHandler to set everything up.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    model_executor_.reset();
    model_observer_tracker_.reset();

    // Wait for PageEntitiesModelHandler to clean everything up.
    task_environment_.RunUntilIdle();
  }

  absl::optional<std::vector<ScoredEntityMetadata>> ExecuteModel(
      const std::string& text) {
    absl::optional<std::vector<ScoredEntityMetadata>> entity_metadata;

    base::RunLoop run_loop;
    model_executor_->ExecuteModelWithInput(
        text, base::BindOnce(
                  [](base::RunLoop* run_loop,
                     absl::optional<std::vector<ScoredEntityMetadata>>*
                         out_entity_metadata,
                     const absl::optional<std::vector<ScoredEntityMetadata>>&
                         entity_metadata) {
                    *out_entity_metadata = entity_metadata;
                    run_loop->Quit();
                  },
                  &run_loop, &entity_metadata));
    run_loop.Run();

    return entity_metadata;
  }

  absl::optional<EntityMetadata> GetMetadataForEntityId(
      const std::string& entity_id) {
    absl::optional<EntityMetadata> entity_metadata;

    base::RunLoop run_loop;
    model_executor_->GetMetadataForEntityId(
        entity_id,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               absl::optional<EntityMetadata>* out_entity_metadata,
               const absl::optional<EntityMetadata>& entity_metadata) {
              *out_entity_metadata = entity_metadata;
              run_loop->Quit();
            },
            &run_loop, &entity_metadata));
    run_loop.Run();

    return entity_metadata;
  }

  base::flat_map<std::string, EntityMetadata> GetMetadataForEntityIds(
      const base::flat_set<std::string>& entity_ids) {
    base::flat_map<std::string, EntityMetadata> entity_metadata_map;

    base::RunLoop run_loop;
    model_executor_->GetMetadataForEntityIds(
        entity_ids, base::BindOnce(
                        [](base::RunLoop* run_loop,
                           base::flat_map<std::string, EntityMetadata>*
                               out_entity_metadata_map,
                           const base::flat_map<std::string, EntityMetadata>&
                               entity_metadata_map) {
                          *out_entity_metadata_map = entity_metadata_map;
                          run_loop->Quit();
                        },
                        &run_loop, &entity_metadata_map));
    run_loop.Run();

    return entity_metadata_map;
  }

  PageEntitiesModelHandler* model_executor() { return model_executor_.get(); }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  base::FilePath GetModelTestDataDir() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("components")
        .AppendASCII("optimization_guide")
        .AppendASCII("internal")
        .AppendASCII("testdata");
  }

  void PushModelInfoToObservers(const ModelInfo& model_info) {
    model_observer_tracker_->PushModelInfoToObservers(model_info);
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageEntitiesModelHandlerImpl> model_executor_;
};

TEST_F(PageEntitiesModelHandlerImplTest, CreateNoMetadata) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder().Build();
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteModel("Taylor Swift singer"), absl::nullopt);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreatedSuccessfully", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreationStatus",
      EntityAnnotatorCreationStatus::kMissingModelMetadata, 1);
}

TEST_F(PageEntitiesModelHandlerImplTest, CreateMetadataWrongType) {
  base::HistogramTester histogram_tester;

  proto::Any any;
  any.set_type_url(any.GetTypeName());
  proto::FieldTrial garbage;
  garbage.SerializeToString(any.mutable_value());

  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder()
          .SetModelFilePath(GetModelTestDataDir().AppendASCII("model.tflite"))
          .SetVersion(123)
          .SetModelMetadata(any)
          .Build();
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteModel("Taylor Swift singer"), absl::nullopt);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreatedSuccessfully", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreationStatus",
      EntityAnnotatorCreationStatus::kMissingEntitiesModelMetadata, 1);
}

TEST_F(PageEntitiesModelHandlerImplTest, CreateNoSlices) {
  base::HistogramTester histogram_tester;

  proto::Any any;
  proto::PageEntitiesModelMetadata metadata;
  any.set_type_url(metadata.GetTypeName());
  metadata.SerializeToString(any.mutable_value());

  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder()
          .SetModelFilePath(GetModelTestDataDir().AppendASCII("model.tflite"))
          .SetVersion(123)
          .SetModelMetadata(any)
          .Build();
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteModel("Taylor Swift singer"), absl::nullopt);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreatedSuccessfully", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageEntitiesModelHandler.CreationStatus",
      EntityAnnotatorCreationStatus::
          kMissingEntitiesModelMetadataSliceSpecification,
      1);
}

TEST_F(PageEntitiesModelHandlerImplTest, ModelInfoUpdated) {
  bool callback_run = false;
  model_executor()->AddOnModelUpdatedCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_run));

  EXPECT_FALSE(callback_run);
  EXPECT_FALSE(model_executor()->GetModelInfo());

  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder()
          .SetModelFilePath(GetModelTestDataDir().AppendASCII("test.tflite"))
          .SetVersion(1337)
          .Build();
  PushModelInfoToObservers(*model_info);
  EXPECT_TRUE(callback_run);

  ASSERT_TRUE(model_executor()->GetModelInfo());
  EXPECT_EQ(model_executor()->GetModelInfo()->GetModelFilePath(),
            model_info->GetModelFilePath());
  EXPECT_EQ(model_executor()->GetModelInfo()->GetVersion(),
            model_info->GetVersion());

  bool immediate_callback_run = false;
  model_executor()->AddOnModelUpdatedCallback(base::BindOnce(
      [](bool* flag) { *flag = true; }, &immediate_callback_run));
  // This callback should be run immediately because the model is loaded.
  EXPECT_TRUE(immediate_callback_run);
}

TEST_F(PageEntitiesModelHandlerImplTest, CreateMissingFiles) {
  proto::Any any;
  proto::PageEntitiesModelMetadata metadata;
  metadata.add_slice("global");
  any.set_type_url(metadata.GetTypeName());
  metadata.SerializeToString(any.mutable_value());

  base::FilePath dir_path = GetModelTestDataDir();
  // A map from the additional file and the creation status if the file was
  // missing.
  base::flat_map<std::string, EntityAnnotatorCreationStatus>
      expected_additional_files = {
          {FilePathToString(dir_path.AppendASCII("model_metadata.pb")),
           EntityAnnotatorCreationStatus::
               kMissingAdditionalEntitiesModelMetadataPath},
          {FilePathToString(dir_path.AppendASCII("word_embeddings")),
           EntityAnnotatorCreationStatus::kMissingAdditionalWordEmbeddingsPath},
          {FilePathToString(dir_path.AppendASCII("global-entities_names")),
           EntityAnnotatorCreationStatus::kMissingAdditionalNameTablePath},
          {FilePathToString(dir_path.AppendASCII("global-entities_metadata")),
           EntityAnnotatorCreationStatus::kMissingAdditionalMetadataTablePath},
          {FilePathToString(
               dir_path.AppendASCII("global-entities_names_filter")),
           EntityAnnotatorCreationStatus::kMissingAdditionalNameFilterPath},
          {FilePathToString(
               dir_path.AppendASCII("global-entities_prefixes_filter")),
           EntityAnnotatorCreationStatus::kMissingAdditionalPrefixFilterPath},
      };
  // Remove one file for each iteration and make sure it fails.
  for (const auto& missing_file_and_status : expected_additional_files) {
    base::HistogramTester histogram_tester;

    base::flat_set<base::FilePath> additional_files;
    for (const auto& additional_file : expected_additional_files) {
      if (additional_file.first != missing_file_and_status.first) {
        additional_files.insert(*StringToFilePath(additional_file.first));
      }
    }
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(GetModelTestDataDir().AppendASCII("model.tflite"))
            .SetVersion(123)
            .SetModelMetadata(any)
            .SetAdditionalFiles(additional_files)
            .Build();
    ASSERT_TRUE(model_info);
    PushModelInfoToObservers(*model_info);

    // We expect that there will be no model to evaluate even for this input
    // that has output in the test model.
    EXPECT_EQ(ExecuteModel("Taylor Swift singer"), absl::nullopt);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageEntitiesModelHandler.CreatedSuccessfully", false,
        1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageEntitiesModelHandler.CreationStatus",
        missing_file_and_status.second, 1);
  }
}

TEST_F(PageEntitiesModelHandlerImplTest, GetMetadataForEntityIdNoModel) {
  EXPECT_EQ(GetMetadataForEntityId("/m/0dl567"), absl::nullopt);
}

TEST_F(PageEntitiesModelHandlerImplTest, GetMetadataForEntityIdsNoModel) {
  EXPECT_TRUE(GetMetadataForEntityIds({"/m/0dl567"}).empty());
}

TEST_F(PageEntitiesModelHandlerImplTest, ExecuteModelNoModel) {
  EXPECT_EQ(ExecuteModel("Taylor Swift singer"), absl::nullopt);
}

TEST_F(PageEntitiesModelHandlerImplTest,
       SetsUpModelCorrectlyBasedOnFeatureParams) {
  absl::optional<proto::Any> registered_model_metadata;
  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, &registered_model_metadata));
  EXPECT_TRUE(registered_model_metadata.has_value());
  absl::optional<proto::PageEntitiesModelMetadata>
      page_entities_model_metadata =
          ParsedAnyMetadata<proto::PageEntitiesModelMetadata>(
              *registered_model_metadata);
  EXPECT_TRUE(page_entities_model_metadata.has_value());
}

}  // namespace
}  // namespace optimization_guide
