// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

const proto::OptimizationTarget kTestOptimizationTargetFoo =
    proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD;
const proto::OptimizationTarget kTestOptimizationTargetBar =
    proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION;

const char kTestLocaleFoo[] = "foo";
const char kTestLocaleBar[] = "bar";

proto::ModelCacheKey CreateModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

struct ModelDetail {
  proto::ModelInfo model_info;
  base::FilePath base_model_dir;
};

}  // namespace

class TestPredictionModelStore : public PredictionModelStore {
 public:
  explicit TestPredictionModelStore(PrefService* local_state)
      : local_state_(local_state) {}

  // PredictionModelStore:
  PrefService* GetLocalState() const override { return local_state_; }

 private:
  raw_ptr<PrefService> local_state_;
};

class PredictionModelStoreTest : public testing::Test {
 public:
  PredictionModelStoreTest() {
    feature_list_.InitWithFeatures(
        {features::kRemoteOptimizationGuideFetching,
         features::kOptimizationGuideModelDownloading},
        {});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_models_dir_.CreateUniqueTempDir());
    local_state_prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterLocalStatePrefs(local_state_prefs_->registry());
    CreateAndInitializePredictionModelStore();
    RunUntilIdle();
  }

  void OnPredictionModelLoaded(
      base::RunLoop* run_loop,
      std::unique_ptr<proto::PredictionModel> loaded_prediction_model) {
    last_loaded_prediction_model_ = std::move(loaded_prediction_model);
    run_loop->Quit();
  }

  proto::PredictionModel* last_loaded_prediction_model() {
    return last_loaded_prediction_model_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Creates model files and returns model details.
  ModelDetail CreateTestModelFiles(
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& model_cache_key,
      const std::vector<const base::FilePath::CharType*>
          additional_file_names) {
    auto base_model_dir =
        prediction_model_store_->GetBaseModelDirForModelCacheKey(
            optimization_target, model_cache_key);
    base::CreateDirectory(base_model_dir);
    base::WriteFile(base_model_dir.Append(GetBaseFileNameForModels()), "");
    proto::ModelInfo model_info;
    model_info.set_optimization_target(optimization_target);
    model_info.set_version(1);
    for (const auto* additional_file_name : additional_file_names) {
      base::WriteFile(base_model_dir.Append(additional_file_name), "");
      model_info.add_additional_files()->set_file_path(
          FilePathToString(base::FilePath(additional_file_name)));
    }
    *model_info.mutable_model_cache_key() = model_cache_key;
    std::string model_info_pb;
    model_info.SerializeToString(&model_info_pb);
    base::WriteFile(base_model_dir.Append(GetBaseFileNameForModelInfo()),
                    model_info_pb);
    return {model_info, base_model_dir};
  }

  void CreateAndInitializePredictionModelStore() {
    prediction_model_store_ =
        std::make_unique<TestPredictionModelStore>(local_state_prefs_.get());
    prediction_model_store_->Initialize(temp_models_dir_.GetPath());
  }

  void WaitForModeLoad(proto::OptimizationTarget optimization_target,
                       const proto::ModelCacheKey& model_cache_key) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    prediction_model_store_->LoadModel(
        optimization_target, model_cache_key,
        base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                       base::Unretained(this), run_loop.get()));
    run_loop->Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_models_dir_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_prefs_;
  std::unique_ptr<proto::PredictionModel> last_loaded_prediction_model_;
  std::unique_ptr<TestPredictionModelStore> prediction_model_store_;
};

TEST_F(PredictionModelStoreTest, BaseModelDirs) {
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto base_model_dir =
      prediction_model_store_->GetBaseModelDirForModelCacheKey(
          kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_TRUE(base_model_dir.IsAbsolute());
  EXPECT_TRUE(temp_models_dir_.GetPath().IsParent(base_model_dir));
}

TEST_F(PredictionModelStoreTest, ModelUpdateAndLoad) {
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);

  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetBar,
                                                 model_cache_key));
  EXPECT_TRUE(prediction_model_store_->HasModelWithVersion(
      kTestOptimizationTargetFoo, model_cache_key, 1));
  EXPECT_FALSE(prediction_model_store_->HasModelWithVersion(
      kTestOptimizationTargetFoo, model_cache_key, 2));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);

  proto::PredictionModel* loaded_model = last_loaded_prediction_model();
  EXPECT_TRUE(loaded_model);
  EXPECT_EQ(kTestOptimizationTargetFoo,
            loaded_model->model_info().optimization_target());
  EXPECT_EQ(StringToFilePath(loaded_model->model().download_url()).value(),
            model_detail.base_model_dir.Append(GetBaseFileNameForModels()));
  EXPECT_EQ(0, loaded_model->model_info().additional_files_size());

  auto metadata_entry = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state_prefs_.get(), kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_EQ(
      model_detail.base_model_dir,
      temp_models_dir_.GetPath().Append(*metadata_entry->GetModelBaseDir()));

  WaitForModeLoad(kTestOptimizationTargetBar, model_cache_key);
  EXPECT_FALSE(last_loaded_prediction_model());
}

// Tests model with an additional file.
TEST_F(PredictionModelStoreTest, ModelWithAdditionalFile) {
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key,
                           {FILE_PATH_LITERAL("valid_additional_file.txt")});

  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);
  auto* loaded_model = last_loaded_prediction_model();
  EXPECT_TRUE(loaded_model);
  EXPECT_EQ(1, loaded_model->model_info().additional_files_size());
  auto additional_file = *StringToFilePath(
      loaded_model->model_info().additional_files(0).file_path());
  EXPECT_EQ(FILE_PATH_LITERAL("valid_additional_file.txt"),
            additional_file.BaseName().value());
  EXPECT_TRUE(additional_file.IsAbsolute());
  EXPECT_TRUE(base::PathExists(additional_file));
}

// Tests model with invalid additional file.
TEST_F(PredictionModelStoreTest, InvalidModelAdditionalFile) {
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key,
                           {FILE_PATH_LITERAL("valid_additional_file.txt"),
                            FILE_PATH_LITERAL("invalid_additional_file.txt")});
  base::DeleteFile(
      model_detail.base_model_dir.AppendASCII("invalid_additional_file.txt"));

  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_FALSE(last_loaded_prediction_model());
}

TEST_F(PredictionModelStoreTest, ModelsSharedBasedOnServerModelCacheKey) {
  auto model_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto model_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key_foo, {});
  prediction_model_store_->UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, model_cache_key_foo, model_cache_key_foo);
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key_foo, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key_foo));

  // Allow the model to be shared for different cache key.
  prediction_model_store_->UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, model_cache_key_bar, model_cache_key_foo);
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key_bar));

  // The model should not be shared for different optimization target.
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetBar,
                                                 model_cache_key_foo));
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetBar,
                                                 model_cache_key_bar));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key_foo);

  auto model_file_foo = last_loaded_prediction_model()->model().download_url();

  last_loaded_prediction_model_.reset();
  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key_bar);
  proto::PredictionModel* loaded_model_bar = last_loaded_prediction_model();
  EXPECT_TRUE(loaded_model_bar);
  EXPECT_EQ(loaded_model_bar->model().download_url(), model_file_foo);
}

TEST_F(PredictionModelStoreTest, UpdateMetadataForExistingModel) {
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);
  proto::PredictionModel* loaded_model = last_loaded_prediction_model();
  EXPECT_TRUE(loaded_model);
  EXPECT_EQ(kTestOptimizationTargetFoo,
            loaded_model->model_info().optimization_target());
  EXPECT_FALSE(loaded_model->model_info().keep_beyond_valid_duration());

  proto::ModelInfo model_info;
  model_info.set_optimization_target(kTestOptimizationTargetFoo);
  model_info.set_version(1);
  model_info.mutable_valid_duration()->set_seconds(
      base::Minutes(100).InSeconds());
  model_info.set_keep_beyond_valid_duration(true);
  prediction_model_store_->UpdateMetadataForExistingModel(
      kTestOptimizationTargetFoo, model_cache_key, model_info);
  RunUntilIdle();
  auto metadata_entry = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state_prefs_.get(), kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_LE(base::Minutes(99),
            metadata_entry->GetExpiryTime() - base::Time::Now());
  EXPECT_EQ(
      model_detail.base_model_dir,
      temp_models_dir_.GetPath().Append(*metadata_entry->GetModelBaseDir()));
  EXPECT_TRUE(metadata_entry->GetKeepBeyondValidDuration());
}

TEST_F(PredictionModelStoreTest, ModelStorageMetrics) {
  RunUntilIdle();

  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  model_detail =
      CreateTestModelFiles(kTestOptimizationTargetBar, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetBar, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  // Recreate the model store, and that should record model storage metrics.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelCount.PainfulPageLoad", 1,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelStore.TotalDirectorySize."
      "PainfulPageLoad",
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelCount.ModelValidation", 1,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelStore.TotalDirectorySize."
      "ModelValidation",
      1);
  EXPECT_EQ(2U, histogram_tester
                    .GetTotalCountsForPrefix(
                        "OptimizationGuide.PredictionModelStore.ModelCount.")
                    .size());
  EXPECT_EQ(
      2U, histogram_tester
              .GetTotalCountsForPrefix(
                  "OptimizationGuide.PredictionModelStore.TotalDirectorySize.")
              .size());
}

TEST_F(PredictionModelStoreTest, ExpiredModelRemoved) {
  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_TRUE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));

  // Fast forward so that the model has expired.
  task_environment_.FastForwardBy(features::StoredModelsValidDuration() +
                                  base::Seconds(1));

  // Recreate the store and it will remove the expired model.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kModelExpired, 1);
  EXPECT_FALSE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_FALSE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));
  EXPECT_TRUE(
      local_state_prefs_->GetDict(prefs::localstate::kStoreFilePathsToDelete)
          .empty());
}

TEST_F(PredictionModelStoreTest, ExpiredModelRemovedOnLoadModel) {
  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  // Fast forward so that the model has expired.
  task_environment_.FastForwardBy(features::StoredModelsValidDuration() +
                                  base::Seconds(1));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_FALSE(last_loaded_prediction_model());
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kModelExpiredOnLoadModel, 1);
  // The model dirs will still not be removed, but will be queued to be deleted
  // at next startup.
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_TRUE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));
  EXPECT_EQ(1U, local_state_prefs_
                    ->GetDict(prefs::localstate::kStoreFilePathsToDelete)
                    .size());
  EXPECT_TRUE(
      local_state_prefs_->GetDict(prefs::localstate::kStoreFilePathsToDelete)
          .FindBool(FilePathToString(ConvertToRelativePath(
              temp_models_dir_.GetPath(), model_detail.base_model_dir))));

  // Recreate the store and it will remove the model slated for deletion
  // earlier.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  EXPECT_FALSE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_FALSE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));
  EXPECT_TRUE(
      local_state_prefs_->GetDict(prefs::localstate::kStoreFilePathsToDelete)
          .empty());
}

TEST_F(PredictionModelStoreTest, OldModelRemovedOnNewModelUpdate) {
  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));

  // Update the model.
  auto new_model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, new_model_detail.model_info,
      new_model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(
      new_model_detail.base_model_dir.Append(GetBaseFileNameForModels())));

  // The old model dirs will still not be removed, but will be queued to be
  // deleted at next startup.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kNewModelUpdate, 1);
  EXPECT_TRUE(base::PathExists(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels())));
  EXPECT_EQ(1U, local_state_prefs_
                    ->GetDict(prefs::localstate::kStoreFilePathsToDelete)
                    .size());
  EXPECT_TRUE(
      local_state_prefs_->GetDict(prefs::localstate::kStoreFilePathsToDelete)
          .FindBool(FilePathToString(ConvertToRelativePath(
              temp_models_dir_.GetPath(), model_detail.base_model_dir))));

  // Recreate the store and it will remove the old model slated for deletion.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  EXPECT_TRUE(
      local_state_prefs_->GetDict(prefs::localstate::kStoreFilePathsToDelete)
          .empty());
}

TEST_F(PredictionModelStoreTest, InvalidModelDirModelRemoved) {
  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  // Delete the model file to make it invalid.
  base::DeleteFile(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels()));

  WaitForModeLoad(kTestOptimizationTargetFoo, model_cache_key);
  EXPECT_FALSE(last_loaded_prediction_model());
  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kModelLoadFailed, 1);
}

TEST_F(PredictionModelStoreTest, InvalidModelDirModelUpdate) {
  base::HistogramTester histogram_tester;

  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  // Delete the model file to make it invalid.
  base::DeleteFile(
      model_detail.base_model_dir.Append(GetBaseFileNameForModels()));
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();

  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kModelUpdateFilePathVerifyFailed,
      1);
}

TEST_F(PredictionModelStoreTest, InconsistentModelDirsRemoved) {
  base::HistogramTester histogram_tester;
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);

  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));

  // Create some inconsistent model dirs that exist in the store, but not in the
  // local state.
  auto model_detail_inconsistent_foo =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  auto model_detail_inconsistent_bar =
      CreateTestModelFiles(kTestOptimizationTargetBar, model_cache_key, {});
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_TRUE(
      base::DirectoryExists(model_detail_inconsistent_foo.base_model_dir));
  EXPECT_TRUE(
      base::DirectoryExists(model_detail_inconsistent_bar.base_model_dir));

  // Recreate the store and it will remove the inconsistent model dirs.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kInconsistentModelDir, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason." +
          GetStringNameForOptimizationTarget(kTestOptimizationTargetFoo),
      PredictionModelStoreModelRemovalReason::kInconsistentModelDir, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason." +
          GetStringNameForOptimizationTarget(kTestOptimizationTargetBar),
      PredictionModelStoreModelRemovalReason::kInconsistentModelDir, 1);
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_FALSE(
      base::DirectoryExists(model_detail_inconsistent_foo.base_model_dir));
  EXPECT_FALSE(
      base::DirectoryExists(model_detail_inconsistent_bar.base_model_dir));
}

TEST_F(PredictionModelStoreTest, InconsistentOptTargetDirsRemoved) {
  base::HistogramTester histogram_tester;
  auto model_cache_key = CreateModelCacheKey(kTestLocaleFoo);

  EXPECT_FALSE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                 model_cache_key));
  auto model_detail =
      CreateTestModelFiles(kTestOptimizationTargetFoo, model_cache_key, {});
  prediction_model_store_->UpdateModel(
      kTestOptimizationTargetFoo, model_cache_key, model_detail.model_info,
      model_detail.base_model_dir, base::DoNothing());
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));

  // Create some invalid models and dirs within the model store dir.
  auto model_detail_unknown = CreateTestModelFiles(
      proto::OPTIMIZATION_TARGET_UNKNOWN, model_cache_key, {});
  EXPECT_TRUE(base::DirectoryExists(model_detail_unknown.base_model_dir));
  auto invalid_dir = temp_models_dir_.GetPath().AppendASCII("baz");
  base::CreateDirectory(invalid_dir);
  base::WriteFile(invalid_dir.Append(GetBaseFileNameForModels()), "");
  EXPECT_TRUE(base::DirectoryExists(invalid_dir));

  // Recreate the store and it will remove the inconsistent model dirs.
  CreateAndInitializePredictionModelStore();
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      PredictionModelStoreModelRemovalReason::kInconsistentModelDir, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_UNKNOWN),
      PredictionModelStoreModelRemovalReason::kInconsistentModelDir, 2);
  EXPECT_TRUE(base::DirectoryExists(model_detail.base_model_dir));
  EXPECT_FALSE(base::DirectoryExists(model_detail_unknown.base_model_dir));
  EXPECT_FALSE(base::DirectoryExists(invalid_dir));
}

}  // namespace optimization_guide
