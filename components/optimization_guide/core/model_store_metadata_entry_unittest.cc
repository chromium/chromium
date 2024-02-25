// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_store_metadata_entry.h"

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
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

}  // namespace

class ModelStoreMetadataEntryTest : public testing::Test {
 public:
  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterLocalStatePrefs(local_state_->registry());
  }

  PrefService* local_state() const { return local_state_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

TEST_F(ModelStoreMetadataEntryTest, PurgeAllMetadata) {
  auto model_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto model_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);

  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo,
      model_cache_key_foo);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar,
      model_cache_key_bar);
  auto expired_time = base::Time::Now() - base::Hours(1);
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetBar, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetBar, model_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  EXPECT_THAT(
      ModelStoreMetadataEntry::GetValidModelDirs(local_state()),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_bar"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_bar")));

  ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(local_state());

  // All entries should be purged.
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo));
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetBar, model_cache_key_foo));
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar));
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetBar, model_cache_key_bar));
  EXPECT_TRUE(
      ModelStoreMetadataEntry::GetValidModelDirs(local_state()).empty());
}

TEST_F(ModelStoreMetadataEntryTest, PurgeExpiredMetadata) {
  auto model_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto model_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);

  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo,
      model_cache_key_foo);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar,
      model_cache_key_bar);
  auto expired_time = base::Time::Now() - base::Hours(1);
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(base::Time::Now() + base::Hours(1));
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetBar, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(base::Time::Now() + base::Hours(1));
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetBar, model_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  EXPECT_THAT(
      ModelStoreMetadataEntry::GetValidModelDirs(local_state()),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_bar"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_bar")));

  ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(local_state());

  // Only expired entries will be purged.
  EXPECT_TRUE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo));
  EXPECT_TRUE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetBar, model_cache_key_foo));
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar));
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetBar, model_cache_key_bar));
  EXPECT_THAT(
      ModelStoreMetadataEntry::GetValidModelDirs(local_state()),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo")));
}

TEST_F(ModelStoreMetadataEntryTest, PurgeMetadataInKillSwitch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuidePredictionModelKillswitch,
      {{"OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD", "5"}});

  auto model_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto model_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo,
      model_cache_key_foo);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar,
      model_cache_key_bar);

  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetVersion(5);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetFoo, model_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetVersion(6);
  }
  {
    ModelStoreMetadataEntryUpdater updater(
        local_state(), kTestOptimizationTargetBar, model_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
  }

  ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(local_state());
  EXPECT_FALSE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_foo));
  EXPECT_TRUE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetFoo, model_cache_key_bar));
  EXPECT_TRUE(ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state(), kTestOptimizationTargetBar, model_cache_key_foo));
}

}  // namespace optimization_guide
