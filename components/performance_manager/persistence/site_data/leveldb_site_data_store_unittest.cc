// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"

#include <limits>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "components/performance_manager/persistence/site_data/site_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

class ScopedReadOnlyDirectory {
 public:
  explicit ScopedReadOnlyDirectory(const base::FilePath& root_dir);
  ~ScopedReadOnlyDirectory() {
    permission_restorer_.reset();
    EXPECT_TRUE(base::DeletePathRecursively(read_only_path_));
  }

  const base::FilePath& GetReadOnlyPath() { return read_only_path_; }

 private:
  base::FilePath read_only_path_;
  std::unique_ptr<base::FilePermissionRestorer> permission_restorer_;
};

ScopedReadOnlyDirectory::ScopedReadOnlyDirectory(
    const base::FilePath& root_dir) {
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      root_dir, FILE_PATH_LITERAL("read_only_path"), &read_only_path_));
  permission_restorer_ =
      std::make_unique<base::FilePermissionRestorer>(read_only_path_);
#if BUILDFLAG(IS_WIN)
  base::DenyFilePermission(read_only_path_, GENERIC_WRITE);
#else  // BUILDFLAG(IS_WIN)
  EXPECT_TRUE(base::MakeFileUnwritable(read_only_path_));
#endif
  EXPECT_FALSE(base::PathIsWritable(read_only_path_));
}

// Initialize a SiteDataProto object with a test value (the same
// value is used to initialize all fields).
void InitSiteDataProto(SiteDataProto* proto,
                       ::google::protobuf::int64 test_value) {
  proto->set_last_loaded(test_value);

  SiteDataFeatureProto feature_proto;
  feature_proto.set_observation_duration(test_value);
  feature_proto.set_use_timestamp(test_value);

  proto->mutable_updates_favicon_in_background()->CopyFrom(feature_proto);
  proto->mutable_updates_title_in_background()->CopyFrom(feature_proto);
  proto->mutable_uses_audio_in_background()->CopyFrom(feature_proto);
}

}  // namespace

class LevelDBSiteDataStoreTest : public ::testing::Test {
 public:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    OpenDB();
  }

  void TearDown() override {
    db_.reset();
    WaitForAsyncOperationsToComplete();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  void OpenDB() {
    OpenDB(temp_dir_.GetPath().Append(FILE_PATH_LITERAL("LocalDB")));
  }

  void OpenDB(base::FilePath path) {
    db_ = std::make_unique<LevelDBSiteDataStore>(path);
    WaitForAsyncOperationsToComplete();
    EXPECT_TRUE(db_);
    db_path_ = path;
  }

  bool DbIsInitialized() {
    base::RunLoop run_loop;
    bool ret = false;
    auto cb = base::BindOnce(
        [](bool* ret, base::OnceClosure quit_closure, bool db_is_initialized) {
          *ret = db_is_initialized;
          std::move(quit_closure).Run();
        },
        base::Unretained(&ret), run_loop.QuitClosure());
    db_->DatabaseIsInitializedForTesting(std::move(cb));
    run_loop.Run();

    return ret;
  }

  const base::FilePath& GetTempPath() { return temp_dir_.GetPath(); }
  const base::FilePath& GetDBPath() { return db_path_; }

 protected:
  // Try to read an entry from the data store, returns true if the entry is
  // present and false otherwise. |receiving_proto| will receive the protobuf
  // corresponding to this entry on success.
  bool ReadFromDB(const url::Origin& origin, SiteDataProto* receiving_proto) {
    EXPECT_TRUE(receiving_proto);
    bool success = false;
    auto init_callback = base::BindOnce(
        [](SiteDataProto* receiving_proto, bool* success,
           std::optional<SiteDataProto> proto_opt) {
          *success = proto_opt.has_value();
          if (proto_opt)
            receiving_proto->CopyFrom(proto_opt.value());
        },
        base::Unretained(receiving_proto), base::Unretained(&success));
    db_->ReadSiteDataFromStore(origin, std::move(init_callback));
    WaitForAsyncOperationsToComplete();
    return success;
  }

  // Add some entries to the data store and returns a vector with their origins.
  std::vector<url::Origin> AddDummyEntriesToDB(size_t num_entries) {
    std::vector<url::Origin> site_origins;
    for (size_t i = 0; i < num_entries; ++i) {
      SiteDataProto proto_temp;
      std::string origin_str = base::StringPrintf("http://%zu.com", i);
      InitSiteDataProto(&proto_temp, static_cast<::google::protobuf::int64>(i));
      EXPECT_TRUE(proto_temp.IsInitialized());
      url::Origin origin = url::Origin::Create(GURL(origin_str));
      db_->WriteSiteDataIntoStore(origin, proto_temp);
      site_origins.emplace_back(origin);
    }
    WaitForAsyncOperationsToComplete();
    return site_origins;
  }

  void WaitForAsyncOperationsToComplete() { task_env_.RunUntilIdle(); }

  const url::Origin kDummyOrigin = url::Origin::Create(GURL("http://foo.com"));

  base::FilePath db_path_;
  content::BrowserTaskEnvironment task_env_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LevelDBSiteDataStore> db_;
};

TEST_F(LevelDBSiteDataStoreTest, InitAndStoreSiteData) {
  // Initializing an entry that doesn't exist in the data store should fail.
  SiteDataProto early_read_proto;
  EXPECT_FALSE(ReadFromDB(kDummyOrigin, &early_read_proto));

  // Add an entry to the data store and make sure that we can read it back.
  ::google::protobuf::int64 test_value = 42;
  SiteDataProto stored_proto;
  InitSiteDataProto(&stored_proto, test_value);
  db_->WriteSiteDataIntoStore(kDummyOrigin, stored_proto);
  SiteDataProto read_proto;
  EXPECT_TRUE(ReadFromDB(kDummyOrigin, &read_proto));
  EXPECT_TRUE(read_proto.IsInitialized());
  EXPECT_EQ(stored_proto.SerializeAsString(), read_proto.SerializeAsString());
}

TEST_F(LevelDBSiteDataStoreTest, RemoveEntries) {
  std::vector<url::Origin> site_origins = AddDummyEntriesToDB(10);

  // Remove half the origins from the data store.
  std::vector<url::Origin> site_origins_to_remove(
      site_origins.begin(), site_origins.begin() + site_origins.size() / 2);
  db_->RemoveSiteDataFromStore(site_origins_to_remove);

  WaitForAsyncOperationsToComplete();

  // Verify that the origins were removed correctly.
  SiteDataProto proto_temp;
  for (const auto& iter : site_origins_to_remove)
    EXPECT_FALSE(ReadFromDB(iter, &proto_temp));

  for (auto iter = site_origins.begin() + site_origins.size() / 2;
       iter != site_origins.end(); ++iter) {
    EXPECT_TRUE(ReadFromDB(*iter, &proto_temp));
  }

  // Clear the data store.
  db_->ClearStore();

  WaitForAsyncOperationsToComplete();

  // Verify that no origin remains.
  for (auto iter : site_origins)
    EXPECT_FALSE(ReadFromDB(iter, &proto_temp));
}

TEST_F(LevelDBSiteDataStoreTest, GetDatabaseSize) {
  std::vector<url::Origin> site_origins = AddDummyEntriesToDB(200);

  auto size_callback =
      base::BindLambdaForTesting([&](std::optional<int64_t> num_rows,
                                     std::optional<int64_t> on_disk_size_kb) {
        EXPECT_TRUE(num_rows);
        // The DB contains an extra row for metadata.
        int64_t expected_rows = site_origins.size() + 1;
        EXPECT_EQ(expected_rows, num_rows.value());

        EXPECT_TRUE(on_disk_size_kb);
        EXPECT_LT(0, on_disk_size_kb.value());
      });

  db_->GetStoreSize(std::move(size_callback));

  WaitForAsyncOperationsToComplete();

  // Verify that the DB is still operational (see implementation detail
  // for Windows).
  SiteDataProto read_proto;
  EXPECT_TRUE(ReadFromDB(site_origins[0], &read_proto));
}

TEST_F(LevelDBSiteDataStoreTest, DatabaseRecoveryTest) {
  std::vector<url::Origin> site_origins = AddDummyEntriesToDB(10);

  db_.reset();

  EXPECT_TRUE(leveldb_chrome::CorruptClosedDBForTesting(GetDBPath()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("PerformanceManager.SiteDB.DatabaseInit",
                                    0);
  // Open the corrupt DB and ensure that the appropriate histograms gets
  // updated.
  OpenDB();
  EXPECT_TRUE(DbIsInitialized());
  histogram_tester.ExpectUniqueSample("PerformanceManager.SiteDB.DatabaseInit",
                                      1 /* kInitStatusCorruption */, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceManager.SiteDB.DatabaseInitAfterRepair",
      0 /* kInitStatusOk */, 1);

  // TODO(sebmarchand): try to induce an I/O error by deleting one of the
  // manifest files.
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221281): Re-enable when DatabaseOpeningFailure works on
// Fuchsia.
#define MAYBE_DatabaseOpeningFailure DISABLED_DatabaseOpeningFailure
#else
#define MAYBE_DatabaseOpeningFailure DatabaseOpeningFailure
#endif
// Ensure that there's no fatal failures if we try using the data store after
// failing to open it (all the events will be ignored).
TEST_F(LevelDBSiteDataStoreTest, MAYBE_DatabaseOpeningFailure) {
  db_.reset();
  ScopedReadOnlyDirectory read_only_dir(GetTempPath());

  OpenDB(read_only_dir.GetReadOnlyPath());
  EXPECT_FALSE(DbIsInitialized());

  SiteDataProto proto_temp;
  EXPECT_FALSE(
      ReadFromDB(url::Origin::Create(GURL("https://foo.com")), &proto_temp));
  WaitForAsyncOperationsToComplete();
  db_->WriteSiteDataIntoStore(url::Origin::Create(GURL("https://foo.com")),
                              proto_temp);
  WaitForAsyncOperationsToComplete();
  db_->RemoveSiteDataFromStore({});
  WaitForAsyncOperationsToComplete();
  db_->ClearStore();
  WaitForAsyncOperationsToComplete();
}

TEST_F(LevelDBSiteDataStoreTest, DBGetsClearedOnVersionUpgrade) {
  // Remove the entry containing the DB version number, this will cause the DB
  // to be cleared the next time it gets opened.
  {
    base::RunLoop run_loop;
    db_->RunTaskWithRawDBForTesting(base::BindOnce([](leveldb::DB* raw_db) {
                                      leveldb::Status s = raw_db->Delete(
                                          leveldb::WriteOptions(),
                                          LevelDBSiteDataStore::kDbMetadataKey);
                                      EXPECT_TRUE(s.ok());
                                    }),
                                    run_loop.QuitClosure());
    run_loop.Run();
  }

  // Add some dummy data to the data store to ensure the data store gets cleared
  // when upgrading it to the new version.
  ::google::protobuf::int64 test_value = 42;
  SiteDataProto stored_proto;
  InitSiteDataProto(&stored_proto, test_value);
  db_->WriteSiteDataIntoStore(kDummyOrigin, stored_proto);
  WaitForAsyncOperationsToComplete();

  db_.reset();

  // Reopen the data store and ensure that it has been cleared.
  OpenDB();

  {
    base::RunLoop run_loop;
    db_->RunTaskWithRawDBForTesting(
        base::BindOnce([](leveldb::DB* raw_db) {
          std::string db_metadata;
          leveldb::Status s =
              raw_db->Get(leveldb::ReadOptions(),
                          LevelDBSiteDataStore::kDbMetadataKey, &db_metadata);
          EXPECT_TRUE(s.ok());
          size_t version = std::numeric_limits<size_t>::max();
          EXPECT_TRUE(base::StringToSizeT(db_metadata, &version));
          EXPECT_EQ(LevelDBSiteDataStore::kDbVersion, version);
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  SiteDataProto proto_temp;
  EXPECT_FALSE(ReadFromDB(kDummyOrigin, &proto_temp));
}

}  // namespace performance_manager
