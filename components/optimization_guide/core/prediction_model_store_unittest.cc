// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
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

class PredictionModelStoreTest : public testing::Test {
 public:
  PredictionModelStoreTest() {
    feature_list_.InitWithFeatures(
        {features::kRemoteOptimizationGuideFetching,
         features::kOptimizationGuideModelDownloading,
         features::kOptimizationGuideInstallWideModelStore},
        {});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_models_dir_.CreateUniqueTempDir());
    local_state_prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterLocalStatePrefs(local_state_prefs_->registry());
    prediction_model_store_ =
        PredictionModelStore::CreatePredictionModelStoreForTesting(
            local_state_prefs_.get(), temp_models_dir_.GetPath());
  }

  void OnPredictionModelLoaded(
      std::unique_ptr<proto::PredictionModel> loaded_prediction_model) {
    last_loaded_prediction_model_ = std::move(loaded_prediction_model);
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
          FilePathToString(base_model_dir.Append(additional_file_name)));
    }
    *model_info.mutable_model_cache_key() = model_cache_key;
    std::string model_info_pb;
    model_info.SerializeToString(&model_info_pb);
    base::WriteFile(base_model_dir.Append(GetBaseFileNameForModelInfo()),
                    model_info_pb);
    return {model_info, base_model_dir};
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_models_dir_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_prefs_;
  std::unique_ptr<proto::PredictionModel> last_loaded_prediction_model_;
  std::unique_ptr<PredictionModelStore> prediction_model_store_;
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

  prediction_model_store_->LoadModel(
      kTestOptimizationTargetFoo, model_cache_key,
      base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                     base::Unretained(this)));
  RunUntilIdle();

  proto::PredictionModel* loaded_model = last_loaded_prediction_model();
  EXPECT_TRUE(loaded_model);
  EXPECT_EQ(kTestOptimizationTargetFoo,
            loaded_model->model_info().optimization_target());
  EXPECT_EQ(StringToFilePath(loaded_model->model().download_url()).value(),
            model_detail.base_model_dir.Append(GetBaseFileNameForModels()));
  EXPECT_EQ(0, loaded_model->model_info().additional_files_size());

  prediction_model_store_->LoadModel(
      kTestOptimizationTargetBar, model_cache_key,
      base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                     base::Unretained(this)));
  RunUntilIdle();
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

  prediction_model_store_->LoadModel(
      kTestOptimizationTargetFoo, model_cache_key,
      base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                     base::Unretained(this)));
  RunUntilIdle();

  EXPECT_TRUE(last_loaded_prediction_model());
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
  EXPECT_TRUE(prediction_model_store_->HasModel(kTestOptimizationTargetFoo,
                                                model_cache_key));

  prediction_model_store_->LoadModel(
      kTestOptimizationTargetFoo, model_cache_key,
      base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                     base::Unretained(this)));
  RunUntilIdle();

  EXPECT_FALSE(last_loaded_prediction_model());
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
  prediction_model_store_->LoadModel(
      kTestOptimizationTargetFoo, model_cache_key,
      base::BindOnce(&PredictionModelStoreTest::OnPredictionModelLoaded,
                     base::Unretained(this)));
  RunUntilIdle();
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
  EXPECT_TRUE(metadata_entry->GetKeepBeyondValidDuration());
}

}  // namespace optimization_guide
