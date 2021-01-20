// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/persistent_availability_store.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/internal/proto/availability.pb.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {
const base::Feature kPersistentTestFeatureFoo{
    "test_foo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPersistentTestFeatureBar{
    "test_bar", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPersistentTestFeatureQux{
    "test_qux", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPersistentTestFeatureNop{
    "test_nop", base::FEATURE_DISABLED_BY_DEFAULT};

Availability CreateAvailability(const base::Feature& feature, uint32_t day) {
  Availability availability;
  availability.set_feature_name(feature.name);
  availability.set_day(day);
  return availability;
}

class PersistentAvailabilityStoreTest : public testing::Test {
 public:
  PersistentAvailabilityStoreTest()
      : db_(nullptr),
        storage_dir_(FILE_PATH_LITERAL("/persistent/store/lalala")) {
    load_callback_ = base::Bind(&PersistentAvailabilityStoreTest::LoadCallback,
                                base::Unretained(this));
  }

  ~PersistentAvailabilityStoreTest() override = default;

  // Creates a DB and stores off a pointer to it as a member.
  std::unique_ptr<leveldb_proto::test::FakeDB<Availability>> CreateDB() {
    auto db = std::make_unique<leveldb_proto::test::FakeDB<Availability>>(
        &db_availabilities_);
    db_ = db.get();
    return db;
  }

  void LoadCallback(
      bool success,
      std::unique_ptr<std::map<std::string, uint32_t>> availabilities) {
    load_successful_ = success;
    load_results_ = std::move(availabilities);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The end result of the store pipeline.
  PersistentAvailabilityStore::OnLoadedCallback load_callback_;

  // Callback results.
  base::Optional<bool> load_successful_;
  std::unique_ptr<std::map<std::string, uint32_t>> load_results_;

  // |db_availabilities_| is used during creation of the FakeDB in CreateDB(),
  // to simplify what the DB has stored.
  std::map<std::string, Availability> db_availabilities_;

  // The database that is in use.
  leveldb_proto::test::FakeDB<Availability>* db_;

  // Constant test data.
  base::FilePath storage_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentAvailabilityStoreTest);
};

}  // namespace

TEST_F(PersistentAvailabilityStoreTest, InitFail) {
  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), FeatureVector(), std::move(load_callback_),
      14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_FALSE(load_successful_.value());
  EXPECT_EQ(0u, load_results_->size());
  EXPECT_EQ(0u, db_availabilities_.size());
}

TEST_F(PersistentAvailabilityStoreTest, LoadFail) {
  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), FeatureVector(), std::move(load_callback_),
      14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(false);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_FALSE(load_successful_.value());
  EXPECT_EQ(0u, load_results_->size());
  EXPECT_EQ(0u, db_availabilities_.size());
}

TEST_F(PersistentAvailabilityStoreTest, EmptyDBEmptyFeatureFilterUpdateFailed) {
  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), FeatureVector(), std::move(load_callback_),
      14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  db_->UpdateCallback(false);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_FALSE(load_successful_.value());
  EXPECT_EQ(0u, load_results_->size());
  EXPECT_EQ(0u, db_availabilities_.size());
}

TEST_F(PersistentAvailabilityStoreTest, EmptyDBEmptyFeatureFilterUpdateOK) {
  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), FeatureVector(), std::move(load_callback_),
      14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  db_->UpdateCallback(true);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_TRUE(load_successful_.value());
  EXPECT_EQ(0u, load_results_->size());
  EXPECT_EQ(0u, db_availabilities_.size());
}

TEST_F(PersistentAvailabilityStoreTest, AllNewFeatures) {
  scoped_feature_list_.InitWithFeatures(
      {kPersistentTestFeatureFoo, kPersistentTestFeatureBar},
      {kPersistentTestFeatureQux});

  FeatureVector feature_filter;
  feature_filter.push_back(&kPersistentTestFeatureFoo);  // Enabled. Not in DB.
  feature_filter.push_back(&kPersistentTestFeatureBar);  // Enabled. Not in DB.
  feature_filter.push_back(&kPersistentTestFeatureQux);  // Disabled. Not in DB.

  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), feature_filter, std::move(load_callback_), 14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  db_->UpdateCallback(true);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_TRUE(load_successful_.value());
  ASSERT_EQ(2u, load_results_->size());
  ASSERT_EQ(2u, db_availabilities_.size());

  ASSERT_TRUE(load_results_->find(kPersistentTestFeatureFoo.name) !=
              load_results_->end());
  EXPECT_EQ(14u, (*load_results_)[kPersistentTestFeatureFoo.name]);
  ASSERT_TRUE(db_availabilities_.find(kPersistentTestFeatureFoo.name) !=
              db_availabilities_.end());
  EXPECT_EQ(14u, db_availabilities_[kPersistentTestFeatureFoo.name].day());

  ASSERT_TRUE(load_results_->find(kPersistentTestFeatureBar.name) !=
              load_results_->end());
  EXPECT_EQ(14u, (*load_results_)[kPersistentTestFeatureBar.name]);
  ASSERT_TRUE(db_availabilities_.find(kPersistentTestFeatureBar.name) !=
              db_availabilities_.end());
  EXPECT_EQ(14u, db_availabilities_[kPersistentTestFeatureBar.name].day());
}

TEST_F(PersistentAvailabilityStoreTest, TestAllFilterCombinations) {
  scoped_feature_list_.InitWithFeatures(
      {kPersistentTestFeatureFoo, kPersistentTestFeatureBar},
      {kPersistentTestFeatureQux, kPersistentTestFeatureNop});

  FeatureVector feature_filter;
  feature_filter.push_back(&kPersistentTestFeatureFoo);  // Enabled. Not in DB.
  feature_filter.push_back(&kPersistentTestFeatureBar);  // Enabled. In DB.
  feature_filter.push_back(&kPersistentTestFeatureQux);  // Disabled. Not in DB.
  feature_filter.push_back(&kPersistentTestFeatureNop);  // Disabled. In DB.

  db_availabilities_[kPersistentTestFeatureBar.name] =
      CreateAvailability(kPersistentTestFeatureBar, 10u);
  db_availabilities_[kPersistentTestFeatureNop.name] =
      CreateAvailability(kPersistentTestFeatureNop, 8u);

  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), feature_filter, std::move(load_callback_), 14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  db_->UpdateCallback(true);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_TRUE(load_successful_.value());
  ASSERT_EQ(2u, load_results_->size());
  ASSERT_EQ(2u, db_availabilities_.size());

  ASSERT_TRUE(load_results_->find(kPersistentTestFeatureFoo.name) !=
              load_results_->end());
  EXPECT_EQ(14u, (*load_results_)[kPersistentTestFeatureFoo.name]);
  ASSERT_TRUE(db_availabilities_.find(kPersistentTestFeatureFoo.name) !=
              db_availabilities_.end());
  EXPECT_EQ(14u, db_availabilities_[kPersistentTestFeatureFoo.name].day());

  ASSERT_TRUE(load_results_->find(kPersistentTestFeatureBar.name) !=
              load_results_->end());
  EXPECT_EQ(10u, (*load_results_)[kPersistentTestFeatureBar.name]);
  ASSERT_TRUE(db_availabilities_.find(kPersistentTestFeatureBar.name) !=
              db_availabilities_.end());
  EXPECT_EQ(10u, db_availabilities_[kPersistentTestFeatureBar.name].day());
}

TEST_F(PersistentAvailabilityStoreTest, TestAllCombinationsEmptyFilter) {
  scoped_feature_list_.InitWithFeatures(
      {kPersistentTestFeatureFoo, kPersistentTestFeatureBar},
      {kPersistentTestFeatureQux, kPersistentTestFeatureNop});

  // Empty filter, but the following setup:
  // kPersistentTestFeatureFoo: Enabled. Not in DB.
  // kPersistentTestFeatureBar: Enabled. In DB.
  // kPersistentTestFeatureQux: Disabled. Not in DB.
  // kPersistentTestFeatureNop: Disabled. In DB.

  db_availabilities_[kPersistentTestFeatureBar.name] =
      CreateAvailability(kPersistentTestFeatureBar, 10u);
  db_availabilities_[kPersistentTestFeatureNop.name] =
      CreateAvailability(kPersistentTestFeatureNop, 8u);

  PersistentAvailabilityStore::LoadAndUpdateStore(
      storage_dir_, CreateDB(), FeatureVector(), std::move(load_callback_),
      14u);

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  db_->LoadCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  db_->UpdateCallback(true);

  EXPECT_TRUE(load_successful_.has_value());
  EXPECT_TRUE(load_successful_.value());
  EXPECT_EQ(0u, load_results_->size());
  EXPECT_EQ(0u, db_availabilities_.size());
}

}  // namespace feature_engagement
