// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_store_metadata_entry.h"

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
    prefs::RegisterLocalStatePrefs(local_state_.registry());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  ModelStoreLedger ledger_{local_state_};
};

TEST_F(ModelStoreMetadataEntryTest, PurgeAllMetadata) {
  ClientCacheKey client_cache_key_foo =
      ClientCacheKey::FromLocale(kTestLocaleFoo);
  ClientCacheKey client_cache_key_bar =
      ClientCacheKey::FromLocale(kTestLocaleBar);
  auto server_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto server_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);

  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_foo, server_cache_key_foo);
  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_bar, server_cache_key_bar);
  auto expired_time = base::Time::Now() - base::Hours(1);
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetBar, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetBar, client_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  EXPECT_THAT(
      ledger_.GetValidModelDirs(),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_bar"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_bar")));

  ledger_.PurgeAllInactiveMetadata();

  // All entries should be purged.
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                        client_cache_key_foo));
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetBar,
                                        client_cache_key_foo));
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                        client_cache_key_bar));
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetBar,
                                        client_cache_key_bar));
  EXPECT_TRUE(ledger_.GetValidModelDirs().empty());
}

TEST_F(ModelStoreMetadataEntryTest, PurgeExpiredMetadata) {
  ClientCacheKey client_cache_key_foo =
      ClientCacheKey::FromLocale(kTestLocaleFoo);
  ClientCacheKey client_cache_key_bar =
      ClientCacheKey::FromLocale(kTestLocaleBar);
  auto server_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto server_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);

  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_foo, server_cache_key_foo);
  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_bar, server_cache_key_bar);
  auto expired_time = base::Time::Now() - base::Hours(1);
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(base::Time::Now() + base::Hours(1));
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetBar, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetExpiryTime(base::Time::Now() + base::Hours(1));
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetBar, client_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_bar")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetExpiryTime(expired_time);
  }
  EXPECT_THAT(
      ledger_.GetValidModelDirs(),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_bar"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_bar")));

  ledger_.PurgeAllInactiveMetadata();

  // Only expired entries will be purged.
  EXPECT_TRUE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                       client_cache_key_foo));
  EXPECT_TRUE(ledger_.GetEntryIfExists(kTestOptimizationTargetBar,
                                       client_cache_key_foo));
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                        client_cache_key_bar));
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetBar,
                                        client_cache_key_bar));
  EXPECT_THAT(
      ledger_.GetValidModelDirs(),
      testing::UnorderedElementsAre(base::FilePath::FromASCII("opt_target_foo")
                                        .AppendASCII("model_cache_key_foo"),
                                    base::FilePath::FromASCII("opt_target_bar")
                                        .AppendASCII("model_cache_key_foo")));
}

TEST_F(ModelStoreMetadataEntryTest, PurgeMetadataInKillSwitch) {
  ClientCacheKey client_cache_key_foo =
      ClientCacheKey::FromLocale(kTestLocaleFoo);
  ClientCacheKey client_cache_key_bar =
      ClientCacheKey::FromLocale(kTestLocaleBar);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuidePredictionModelKillswitch,
      {{"OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD", "5"}});

  auto server_cache_key_foo = CreateModelCacheKey(kTestLocaleFoo);
  auto server_cache_key_bar = CreateModelCacheKey(kTestLocaleBar);
  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_foo, server_cache_key_foo);
  ledger_.UpdateModelCacheKeyMapping(
      kTestOptimizationTargetFoo, client_cache_key_bar, server_cache_key_bar);

  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
    updater.SetVersion(5);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetFoo, client_cache_key_bar);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_bar"));
    updater.SetVersion(6);
  }
  {
    ModelStoreMetadataEntryUpdater updater =
        ledger_.UpdateEntry(kTestOptimizationTargetBar, client_cache_key_foo);
    updater.SetModelBaseDir(base::FilePath::FromASCII("opt_target_foo")
                                .AppendASCII("model_cache_key_foo"));
  }

  ledger_.PurgeAllInactiveMetadata();
  EXPECT_FALSE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                        client_cache_key_foo));
  EXPECT_TRUE(ledger_.GetEntryIfExists(kTestOptimizationTargetFoo,
                                       client_cache_key_bar));
  EXPECT_TRUE(ledger_.GetEntryIfExists(kTestOptimizationTargetBar,
                                       client_cache_key_foo));
}

}  // namespace optimization_guide
