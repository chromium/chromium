// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_store.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;

namespace optimization_guide {

namespace {

constexpr char kDefaultComponentVersion[] = "1.0.0";
constexpr char kUpdateComponentVersion[] = "2.0.0";

std::string GetHost(size_t id) {
  // Host alternates between two different domain types depending on
  // whether the id is odd or even.
  if (id % 2 == 0) {
    return "domain" + base::NumberToString(id) + ".org";
  } else {
    return "different.domain" + base::NumberToString(id) + ".co.in";
  }
}

enum class MetadataSchemaState {
  kMissing,
  kInvalid,
  kValid,
};

}  // namespace

class OptimizationGuideStoreTest : public testing::Test {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<OptimizationGuideStore::EntryKey, StoreEntry>;

  OptimizationGuideStoreTest() = default;

  OptimizationGuideStoreTest(const OptimizationGuideStoreTest&) = delete;
  OptimizationGuideStoreTest& operator=(const OptimizationGuideStoreTest&) =
      delete;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
  }

  void TearDown() override {
    last_loaded_hint_.reset();
  }

  // Initializes the entries contained within the database on startup.
  void SeedInitialData(
      MetadataSchemaState state,
      std::optional<size_t> component_hint_count = std::nullopt,
      std::optional<base::Time> fetched_hints_update = std::nullopt) {
    db_store_.clear();

    // Add a metadata schema entry if its state isn't kMissing. The version
    // entry version is set to the store's current version if the state is
    // kValid; otherwise, it's set to the invalid version of "0".
    if (state == MetadataSchemaState::kValid) {
      db_store_[OptimizationGuideStore::GetMetadataTypeEntryKey(
                    OptimizationGuideStore::MetadataType::kSchema)]
          .set_version(OptimizationGuideStore::kStoreSchemaVersion);
    } else if (state == MetadataSchemaState::kInvalid) {
      db_store_[OptimizationGuideStore::GetMetadataTypeEntryKey(
                    OptimizationGuideStore::MetadataType::kSchema)]
          .set_version("0");
    }

    // If the database is being seeded with component hints, it is indicated
    // with a provided count. Add the component metadata with the default
    // component version and then add the indicated number of component hints.
    // if (component_hint_count && component_hint_count >
    // static_cast<size_t>(0)) {
    if (component_hint_count && component_hint_count > 0u) {
      db_store_[OptimizationGuideStore::GetMetadataTypeEntryKey(
                    OptimizationGuideStore::MetadataType::kComponent)]
          .set_version(kDefaultComponentVersion);
      OptimizationGuideStore::EntryKeyPrefix component_hint_key_prefix =
          OptimizationGuideStore::GetComponentHintEntryKeyPrefix(
              base::Version(kDefaultComponentVersion));
      for (size_t i = 0; i < component_hint_count.value(); ++i) {
        std::string host = GetHost(i);
        StoreEntry& entry = db_store_[component_hint_key_prefix + host];
        entry.set_entry_type(static_cast<proto::StoreEntryType>(
            OptimizationGuideStore::StoreEntryType::kComponentHint));
        proto::Hint* hint = entry.mutable_hint();
        hint->set_key(host);
        hint->set_key_representation(proto::HOST);
        proto::PageHint* page_hint = hint->add_page_hints();
        page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      }
    }
    if (fetched_hints_update) {
      db_store_[OptimizationGuideStore::GetMetadataTypeEntryKey(
                    OptimizationGuideStore::MetadataType::kFetched)]
          .set_update_time_secs(
              fetched_hints_update->ToDeltaSinceWindowsEpoch().InSeconds());
    }
  }

  // Moves the specified number of component hints into the update data.
  void SeedComponentUpdateData(StoreUpdateData* update_data,
                               size_t component_hint_count) {
    for (size_t i = 0; i < component_hint_count; ++i) {
      std::string host = GetHost(i);
      proto::Hint hint;
      hint.set_key(host);
      hint.set_key_representation(proto::HOST);
      proto::PageHint* page_hint = hint.add_page_hints();
      page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      update_data->MoveHintIntoUpdateData(std::move(hint));
    }
  }
  // Moves the specified number of fetched hints into the update data.
  void SeedFetchedUpdateData(StoreUpdateData* update_data,
                             size_t fetched_hint_count) {
    for (size_t i = 0; i < fetched_hint_count; ++i) {
      std::string host = GetHost(i);
      proto::Hint hint;
      hint.set_key(host);
      hint.set_key_representation(proto::HOST);
      proto::PageHint* page_hint = hint.add_page_hints();
      page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      update_data->MoveHintIntoUpdateData(std::move(hint));
    }
  }

  void CreateDatabase() {
    // Reset everything.
    db_ = nullptr;
    guide_store_.reset();

    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);
    db_ = db.get();

    guide_store_ = std::make_unique<OptimizationGuideStore>(
        std::move(db), task_environment_.GetMainThreadTaskRunner(),
        pref_service_.get());
  }

  void InitializeDatabase(bool success, bool purge_existing_data = false) {
    EXPECT_CALL(*this, OnInitialized());
    guide_store()->Initialize(
        purge_existing_data,
        base::BindOnce(&OptimizationGuideStoreTest::OnInitialized,
                       base::Unretained(this)));
    // OnDatabaseInitialized callback
    db()->InitStatusCallback(success ? leveldb_proto::Enums::kOK
                                     : leveldb_proto::Enums::kError);
  }

  void InitializeStore(MetadataSchemaState state,
                       bool purge_existing_data = false) {
    InitializeDatabase(true /*=success*/, purge_existing_data);

    if (purge_existing_data) {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
      return;
    }

    // OnLoadMetadata callback
    db()->LoadCallback(true);
    if (state == MetadataSchemaState::kValid) {
      // OnLoadEntryKeys callback
      db()->LoadCallback(true);
    } else {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
    }
  }

  void UpdateComponentHints(std::unique_ptr<StoreUpdateData> component_data,
                            bool update_success = true,
                            bool load_hint_entry_keys_success = true) {
    EXPECT_CALL(*this, OnUpdateStore());
    guide_store()->UpdateComponentHints(
        std::move(component_data),
        base::BindOnce(&OptimizationGuideStoreTest::OnUpdateStore,
                       base::Unretained(this)));
    // OnUpdateStore callback
    db()->UpdateCallback(update_success);
    if (update_success) {
      // OnLoadEntryKeys callback
      db()->LoadCallback(load_hint_entry_keys_success);
    }
  }

  void UpdateFetchedHints(std::unique_ptr<StoreUpdateData> fetched_data,
                          bool update_success = true,
                          bool load_hint_entry_keys_success = true) {
    EXPECT_CALL(*this, OnUpdateStore());
    guide_store()->UpdateFetchedHints(
        std::move(fetched_data),
        base::BindOnce(&OptimizationGuideStoreTest::OnUpdateStore,
                       base::Unretained(this)));
    // OnUpdateStore callback
    db()->UpdateCallback(update_success);
    if (update_success) {
      // OnLoadEntryKeys callback
      db()->LoadCallback(load_hint_entry_keys_success);
    }
  }

  void ClearFetchedHintsFromDatabase() {
    guide_store()->ClearFetchedHintsFromDatabase();
    db()->UpdateCallback(true);
    db()->LoadCallback(true);
  }

  void PurgeExpiredFetchedHints() {
    guide_store()->PurgeExpiredFetchedHints();

    // OnLoadExpiredEntriesToPurge
    db()->LoadCallback(true);
    // OnUpdateStore
    db()->UpdateCallback(true);
    // OnLoadEntryKeys callback
    db()->LoadCallback(true);
  }
  bool IsMetadataSchemaEntryKeyPresent() const {
    return IsKeyPresent(OptimizationGuideStore::GetMetadataTypeEntryKey(
        OptimizationGuideStore::MetadataType::kSchema));
  }

  // Verifies that the fetched metadata has the expected next update time.
  void ExpectFetchedMetadata(base::Time update_time) const {
    const auto& metadata_entry =
        db_store_.find(OptimizationGuideStore::GetMetadataTypeEntryKey(
            OptimizationGuideStore::MetadataType::kFetched));
    if (metadata_entry != db_store_.end()) {
      // The next update time should have same time up to the second as the
      // metadata entry is stored in seconds.
      EXPECT_TRUE(base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(
                      metadata_entry->second.update_time_secs())) -
                      update_time <
                  base::Seconds(1));
    } else {
      FAIL() << "No fetched metadata found";
    }
  }

  // Verifies that the component metadata has the expected version and all
  // expected component hints are present.
  void ExpectComponentHintsPresent(const std::string& version,
                                   int count) const {
    const auto& metadata_entry =
        db_store_.find(OptimizationGuideStore::GetMetadataTypeEntryKey(
            OptimizationGuideStore::MetadataType::kComponent));
    if (metadata_entry != db_store_.end()) {
      EXPECT_EQ(metadata_entry->second.version(), version);
    } else {
      FAIL() << "No component metadata found";
    }

    OptimizationGuideStore::EntryKeyPrefix component_hint_entry_key_prefix =
        OptimizationGuideStore::GetComponentHintEntryKeyPrefix(
            base::Version(version));
    for (int i = 0; i < count; ++i) {
      std::string host = GetHost(i);
      OptimizationGuideStore::EntryKey hint_entry_key =
          component_hint_entry_key_prefix + host;
      const auto& hint_entry = db_store_.find(hint_entry_key);
      if (hint_entry == db_store_.end()) {
        FAIL() << "No entry found for component hint: " << hint_entry_key;
      }

      if (!hint_entry->second.has_hint()) {
        FAIL() << "Component hint entry does not have hint: " << hint_entry_key;
      }

      EXPECT_EQ(hint_entry->second.hint().key(), host);
    }
  }

  // Returns true if the data is present for the given key.
  bool IsKeyPresent(const OptimizationGuideStore::EntryKey& entry_key) const {
    return db_store_.find(entry_key) != db_store_.end();
  }

  size_t GetDBStoreEntryCount() const { return db_store_.size(); }
  size_t GetStoreEntryKeyCount() const {
    return guide_store_->GetEntryKeyCount();
  }

  OptimizationGuideStore* guide_store() { return guide_store_.get(); }
  FakeDB<proto::StoreEntry>* db() { return db_; }

  const OptimizationGuideStore::EntryKey& last_loaded_entry_key() const {
    return last_loaded_entry_key_;
  }

  MemoryHint* last_loaded_hint() { return last_loaded_hint_.get(); }

  void OnHintLoaded(const OptimizationGuideStore::EntryKey& hint_entry_key,
                    std::unique_ptr<MemoryHint> loaded_hint) {
    last_loaded_entry_key_ = hint_entry_key;
    last_loaded_hint_ = std::move(loaded_hint);
  }
  bool IsStoreFilesToDeletePrefEmpty() {
    const base::Value::Dict& pref_dict =
        pref_service_->GetDict(prefs::kStoreFilePathsToDelete);
    return pref_dict.empty();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD0(OnUpdateStore, void());

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  StoreEntryMap db_store_;
  std::unique_ptr<OptimizationGuideStore> guide_store_;
  raw_ptr<FakeDB<proto::StoreEntry>> db_;
  OptimizationGuideStore::EntryKey last_loaded_entry_key_;
  std::unique_ptr<MemoryHint> last_loaded_hint_;
};

TEST_F(OptimizationGuideStoreTest, NoInitialization) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnInitializeWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnLoadMetadataWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnUpdateMetadataNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnInitializeWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnLoadMetadataWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnUpdateMetadataWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kInvalid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeFailedOnLoadHintEntryKeysWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10, base::Time().Now());
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnLoadEntryKeys callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(OptimizationGuideStoreTest, InitializeSucceededWithoutSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kMissing;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest, InitializeSucceededWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest, InitializeSucceededWithValidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeSucceededWithInvalidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else, as
  // the initial component hints are all purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest, InitializeSucceededWithPurgeExistingData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeSucceededWithValidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count,
                  base::Time().Now() /* fetch_update_time */);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 3));
  EXPECT_EQ(GetStoreEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeSucceededWithValidSchemaEntryAndComponentDataOnly) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       InitializeSucceededWithValidSchemaEntryAndFetchedMetaData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 0;
  SeedInitialData(schema_state, component_hint_count, base::Time().Now());
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentUpdateDataFailsForUninitializedStore) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();

  // StoreUpdateData for a component update should only be created if the store
  // is initialized.
  EXPECT_FALSE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentUpdateDataFailsForEarlierVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No StoreUpdateData for a component update should be created when the
  // component version of the update is older than the store's component
  // version.
  EXPECT_FALSE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version("0.0.0")));
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentUpdateDataFailsForCurrentVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No StoreUpdateData should be created when the component version of the
  // update is the same as the store's component version.
  EXPECT_FALSE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentUpdateDataSucceedsWithNoPreexistingVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // StoreUpdateData for a component update should be created when there is no
  // pre-existing component in the store.
  EXPECT_TRUE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentUpdateDataSucceedsForNewerVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // StoreUpdateData for a component update should be created when the component
  // version of the update is newer than the store's component version.
  EXPECT_TRUE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(OptimizationGuideStoreTest, UpdateComponentHintsUpdateEntriesFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), 5);

  UpdateComponentHints(std::move(update_data), false /*update_success*/);

  // The store should be purged if the component data update fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(OptimizationGuideStoreTest, UpdateComponentHintsGetKeysFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), 5);

  UpdateComponentHints(std::move(update_data), true /*update_success*/,
                       false /*load_hints_keys_success*/);

  // The store should be purged if loading the keys after the component update
  // fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(OptimizationGuideStoreTest, UpdateComponentHints) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(OptimizationGuideStoreTest,
       UpdateComponentHintsAfterInitializationDataPurge) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(OptimizationGuideStoreTest,
       CreateComponentDataWithAlreadyUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // StoreUpdateData for the component update should not be created for a second
  // component update with the same version as the first component update.
  EXPECT_FALSE(guide_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(OptimizationGuideStoreTest,
       UpdateComponentHintsWithUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count_1 = 5;
  size_t update_hint_count_2 = 15;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Create two updates for the same component version with different counts.
  std::unique_ptr<StoreUpdateData> update_data_1 =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  std::unique_ptr<StoreUpdateData> update_data_2 =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data_1);
  SeedComponentUpdateData(update_data_1.get(), update_hint_count_1);
  ASSERT_TRUE(update_data_2);
  SeedComponentUpdateData(update_data_2.get(), update_hint_count_2);

  // Update the component data with the same component version twice:
  // first with |update_data_1| and then with |update_data_2|.
  UpdateComponentHints(std::move(update_data_1));

  EXPECT_CALL(*this, OnUpdateStore());
  guide_store()->UpdateComponentHints(
      std::move(update_data_2),
      base::BindOnce(&OptimizationGuideStoreTest::OnUpdateStore,
                     base::Unretained(this)));

  // Verify that the store is populated with the component data from
  // |update_data_1| and not |update_data_2|.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count_1 + 2);
  EXPECT_EQ(GetStoreEntryKeyCount(), update_hint_count_1);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count_1);
}

TEST_F(OptimizationGuideStoreTest, LoadHintOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  const OptimizationGuideStore::EntryKey kInvalidEntryKey = "invalid";
  guide_store()->LoadHint(
      kInvalidEntryKey,
      base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                     base::Unretained(this)));

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(OptimizationGuideStoreTest, LoadHintFailure) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  const OptimizationGuideStore::EntryKey kInvalidEntryKey = "invalid";
  guide_store()->LoadHint(
      kInvalidEntryKey,
      base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                     base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(false);

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(OptimizationGuideStoreTest, LoadHintSuccessInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all component hints in the initial data can successfully be
  // loaded from the store.
  for (size_t i = 0; i < hint_count; ++i) {
    std::string host = GetHost(i);
    OptimizationGuideStore::EntryKey hint_entry_key;
    if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host: " << host;
    }

    guide_store()->LoadHint(
        hint_entry_key,
        base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                       base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint was null for entry key: " << hint_entry_key;
    }

    EXPECT_EQ(last_loaded_hint()->hint()->key(), host);
    EXPECT_FALSE(last_loaded_hint()->expiry_time().has_value());
  }
}

TEST_F(OptimizationGuideStoreTest, LoadHintSuccessUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // Verify that all component hints within a successful component update can
  // be loaded from the store.
  for (size_t i = 0; i < update_hint_count; ++i) {
    std::string host = GetHost(i);
    OptimizationGuideStore::EntryKey hint_entry_key;
    if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host: " << host;
    }

    guide_store()->LoadHint(
        hint_entry_key,
        base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                       base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint was null for entry key: " << hint_entry_key;
    }

    EXPECT_EQ(last_loaded_hint()->hint()->key(), host);
    EXPECT_FALSE(last_loaded_hint()->expiry_time().has_value());
  }
}

TEST_F(OptimizationGuideStoreTest, FindHintEntryKeyOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  std::string host = GetHost(0);
  OptimizationGuideStore::EntryKey hint_entry_key;

  // Verify that hint entry keys can't be found when the store is unavailable.
  EXPECT_FALSE(guide_store()->FindHintEntryKey(host, &hint_entry_key));
}

TEST_F(OptimizationGuideStoreTest, FindHintEntryKeyInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all hints contained within the initial store data are reported
  // as being found and hints that are not containd within the initial data are
  // properly reported as not being found.
  for (size_t i = 0; i < hint_count * 2; ++i) {
    std::string host = GetHost(i);
    OptimizationGuideStore::EntryKey hint_entry_key;
    bool success = guide_store()->FindHintEntryKey(host, &hint_entry_key);
    EXPECT_EQ(success, i < hint_count);
  }
}

TEST_F(OptimizationGuideStoreTest, FindHintEntryKeyUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // Verify that all hints contained within the component update are reported
  // by the store as being found and hints that are not containd within the
  // component update are properly reported as not being found.
  for (size_t i = 0; i < update_hint_count * 2; ++i) {
    std::string host = GetHost(i);
    OptimizationGuideStore::EntryKey hint_entry_key;
    bool success = guide_store()->FindHintEntryKey(host, &hint_entry_key);
    EXPECT_EQ(success, i < update_hint_count);
  }
}

TEST_F(OptimizationGuideStoreTest, FetchedHintsMetadataStored) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, 10, update_time);
  CreateDatabase();
  InitializeStore(schema_state);

  ExpectFetchedMetadata(update_time);
}

TEST_F(OptimizationGuideStoreTest, FindHintEntryKeyForFetchedHints) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t update_hint_count = 5;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, 0);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->CreateUpdateDataForFetchedHints(update_time);
  ASSERT_TRUE(update_data);
  SeedFetchedUpdateData(update_data.get(), update_hint_count);
  UpdateFetchedHints(std::move(update_data));

  for (size_t i = 0; i < update_hint_count; ++i) {
    std::string host = GetHost(i);
    OptimizationGuideStore::EntryKey hint_entry_key;
    bool success = guide_store()->FindHintEntryKey(host, &hint_entry_key);
    EXPECT_EQ(success, i < update_hint_count);
  }
}

TEST_F(OptimizationGuideStoreTest,
       FindHintEntryKeyCheckFetchedBeforeComponentHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint hint;
  hint.set_key("host.domain2.org");
  hint.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host = "host.domain2.org";
  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }

  EXPECT_EQ(hint_entry_key, "3_host.domain2.org");

  host = "domain1.org";

  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }

  EXPECT_EQ(hint_entry_key, "2_2.0.0_domain1.org");
}

TEST_F(OptimizationGuideStoreTest, SuccessfulRemovedFetchedHintsByKey) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> component_update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(component_update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  component_update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("domain2.org");
  hint2.set_key_representation(proto::HOST);
  component_update_data->MoveHintIntoUpdateData(std::move(hint2));
  UpdateComponentHints(std::move(component_update_data));

  std::unique_ptr<StoreUpdateData> fetched_update_data =
      guide_store()->CreateUpdateDataForFetchedHints(base::Time().Now());

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  fetched_update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));
  UpdateFetchedHints(std::move(fetched_update_data));

  base::RunLoop run_loop;
  guide_store()->RemoveFetchedHintsByKey(run_loop.QuitClosure(),
                                         {
                                             "domain1.org",
                                             "domain2.org",
                                             "domain3.org",
                                             "domain4.org",
                                         });
  db()->UpdateCallback(/*success=*/true);
  run_loop.Run();

  OptimizationGuideStore::EntryKey hint_entry_key;

  // Check for keys that should exist.
  EXPECT_TRUE(guide_store()->FindHintEntryKey("domain1.org", &hint_entry_key));
  EXPECT_EQ("2_2.0.0_domain1.org", hint_entry_key);
  EXPECT_TRUE(guide_store()->FindHintEntryKey("domain2.org", &hint_entry_key));
  EXPECT_EQ("2_2.0.0_domain2.org", hint_entry_key);

  // Check for keys that should not exist.
  EXPECT_FALSE(guide_store()->FindHintEntryKey("domain3.org", &hint_entry_key));
}

TEST_F(OptimizationGuideStoreTest, FailedRemovedFetchedHintsByKey) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<StoreUpdateData> component_update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(component_update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  component_update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("domain2.org");
  hint2.set_key_representation(proto::HOST);
  component_update_data->MoveHintIntoUpdateData(std::move(hint2));
  UpdateComponentHints(std::move(component_update_data));

  std::unique_ptr<StoreUpdateData> fetched_update_data =
      guide_store()->CreateUpdateDataForFetchedHints(base::Time().Now());
  ASSERT_TRUE(fetched_update_data);

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  fetched_update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));
  UpdateFetchedHints(std::move(fetched_update_data));

  bool did_callback_run = false;
  base::OnceClosure callback = base::BindOnce(
      [](bool* set_when_run) { *set_when_run = true; }, &did_callback_run);
  guide_store()->RemoveFetchedHintsByKey(std::move(callback), {
                                                                  "domain1.org",
                                                                  "domain2.org",
                                                                  "domain3.org",
                                                                  "domain4.org",
                                                              });
  RunUntilIdle();
  db()->UpdateCallback(/*success=*/false);
  RunUntilIdle();
  EXPECT_FALSE(did_callback_run);

  // The callback did not succeed, so the store should no longer be available.
  EXPECT_EQ(0U, GetStoreEntryKeyCount());
  EXPECT_FALSE(guide_store()->IsAvailable());
}

TEST_F(OptimizationGuideStoreTest, ClearFetchedHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("host.domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host = "host.domain2.org";
  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }

  EXPECT_EQ(hint_entry_key, "3_host.domain2.org");

  host = "domain1.org";

  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }

  EXPECT_EQ(hint_entry_key, "2_2.0.0_domain1.org");

  // Remove the fetched hints from the OptimizationGuideStore.
  ClearFetchedHintsFromDatabase();
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ClearFetchedHints.StoreAvailable", true, 1);

  host = "domain1.org";
  // Component hint should still exist.
  EXPECT_TRUE(guide_store()->FindHintEntryKey(host, &hint_entry_key));

  host = "domain3.org";
  // Fetched hint should not still exist.
  EXPECT_FALSE(guide_store()->FindHintEntryKey(host, &hint_entry_key));

  // Add Components back - newer version.
  base::Version version3("3.0.0");
  std::unique_ptr<StoreUpdateData> update_data2 =
      guide_store()->MaybeCreateUpdateDataForComponentHints(version3);

  ASSERT_TRUE(update_data2);

  proto::Hint new_hint2;
  new_hint2.set_key("host.domain2.org");
  new_hint2.set_key_representation(proto::HOST);
  update_data2->MoveHintIntoUpdateData(std::move(new_hint2));

  UpdateComponentHints(std::move(update_data2));

  host = "host.domain2.org";
  EXPECT_TRUE(guide_store()->FindHintEntryKey(host, &hint_entry_key));

  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);
  proto::Hint new_hint;
  new_hint.set_key("domain1.org");
  new_hint.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(new_hint));

  UpdateFetchedHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  host = "domain1.org";

  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }

  EXPECT_EQ(hint_entry_key, "3_domain1.org");
}

TEST_F(OptimizationGuideStoreTest, FetchHintsPurgeExpiredFetchedHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_hint1.mutable_max_cache_duration()->set_seconds(
      base::Days(7).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  fetched_hint2.mutable_max_cache_duration()->set_seconds(
      base::Days(7).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Add expired fetched hints to the store.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint fetched_hint3;
  fetched_hint1.set_key("domain4.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_hint1.mutable_max_cache_duration()->set_seconds(
      base::Days(-7).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint4;
  fetched_hint2.set_key("domain5.org");
  fetched_hint2.set_key_representation(proto::HOST);
  fetched_hint2.mutable_max_cache_duration()->set_seconds(
      base::Days(-7).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  PurgeExpiredFetchedHints();

  OptimizationGuideStore::EntryKey hint_entry_key;
  EXPECT_FALSE(guide_store()->FindHintEntryKey("domain4.org", &hint_entry_key));
  EXPECT_FALSE(guide_store()->FindHintEntryKey("domain5.org", &hint_entry_key));
  EXPECT_TRUE(guide_store()->FindHintEntryKey("domain2.org", &hint_entry_key));
  EXPECT_TRUE(guide_store()->FindHintEntryKey("domain3.org", &hint_entry_key));
}

TEST_F(OptimizationGuideStoreTest, FetchedHintsLoadExpiredHint) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that expired.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("host.domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_hint1.mutable_max_cache_duration()->set_seconds(
      base::Days(-10).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host = "host.domain2.org";
  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }
  EXPECT_EQ(hint_entry_key, "3_host.domain2.org");
  guide_store()->LoadHint(
      hint_entry_key, base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                                     base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(true);

  // |hint_entry_key| will be a fetched hint but the entry will be empty.
  EXPECT_EQ(last_loaded_entry_key(), hint_entry_key);
  EXPECT_FALSE(last_loaded_hint());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheStore.OnLoadHint.FetchedHintExpired", true,
      1);
}

TEST_F(OptimizationGuideStoreTest, FetchedHintsLoadPopulatesExpiryTime) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      guide_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that expired.
  update_data = guide_store()->CreateUpdateDataForFetchedHints(update_time);

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("host.domain2.org");
  fetched_hint1.set_key_representation(proto::HOST);
  fetched_hint1.mutable_max_cache_duration()->set_seconds(
      base::Days(10).InSeconds());
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host = "host.domain2.org";
  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!guide_store()->FindHintEntryKey(host, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host: " << host;
  }
  EXPECT_EQ(hint_entry_key, "3_host.domain2.org");
  guide_store()->LoadHint(
      hint_entry_key, base::BindOnce(&OptimizationGuideStoreTest::OnHintLoaded,
                                     base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(true);

  // |hint_entry_key| will be a fetched hint but the entry will be empty.
  EXPECT_EQ(last_loaded_entry_key(), hint_entry_key);
  EXPECT_TRUE(last_loaded_hint());
  EXPECT_TRUE(last_loaded_hint()->expiry_time().has_value());
}

}  // namespace optimization_guide
