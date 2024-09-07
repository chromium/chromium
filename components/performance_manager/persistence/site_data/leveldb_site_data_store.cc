// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"

#include <atomic>
#include <limits>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace performance_manager {

namespace {

std::atomic<bool> g_use_in_memory_db_for_testing = false;

// The name of the following histograms is the same as the one used in the
// //c/b/resource_coordinator version of this file. It's fine to keep the same
// name as these 2 codepath will never be enabled at the same time. These
// histograms should be removed once it has been confirmed that the data is
// similar to the one from the other implementation.
//
// TODO(crbug.com/40902006): Remove these histograms when SiteDB is confirmed to
// be working for BackgroundTabLoadingPolicy.
const char kInitStatusHistogramLabel[] =
    "PerformanceManager.SiteDB.DatabaseInit";
const char kInitStatusAfterRepairHistogramLabel[] =
    "PerformanceManager.SiteDB.DatabaseInitAfterRepair";
const char kInitStatusAfterDeleteHistogramLabel[] =
    "PerformanceManager.SiteDB.DatabaseInitAfterDelete";

enum class InitStatus {
  kInitStatusOk,
  kInitStatusCorruption,
  kInitStatusIOError,
  kInitStatusUnknownError,
  kInitStatusMax
};

// Report the database's initialization status metrics.
void ReportInitStatus(const char* histogram_name,
                      const leveldb::Status& status) {
  if (status.ok()) {
    base::UmaHistogramEnumeration(histogram_name, InitStatus::kInitStatusOk,
                                  InitStatus::kInitStatusMax);
  } else if (status.IsCorruption()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusCorruption,
                                  InitStatus::kInitStatusMax);
  } else if (status.IsIOError()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusIOError,
                                  InitStatus::kInitStatusMax);
  } else {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusUnknownError,
                                  InitStatus::kInitStatusMax);
  }
}

// Attempt to repair the database stored in |db_path|.
bool RepairDatabase(const std::string& db_path) {
  leveldb_env::Options options;
  options.reuse_logs = false;
  options.max_open_files = 0;
  bool repair_succeeded = leveldb::RepairDB(db_path, options).ok();
  base::UmaHistogramBoolean("PerformanceManager.SiteDB.DatabaseRepair",
                            repair_succeeded);
  return repair_succeeded;
}

bool ShouldAttemptDbRepair(const leveldb::Status& status) {
  // A corrupt database might be repaired (some data might be loss but it's
  // better than losing everything).
  if (status.IsCorruption())
    return true;
  // An I/O error might be caused by a missing manifest, it's sometime possible
  // to repair this (some data might be loss).
  if (status.IsIOError())
    return true;

  return false;
}

struct DatabaseSizeResult {
  std::optional<int64_t> num_rows;
  std::optional<int64_t> on_disk_size_kb;
};

std::string SerializeOriginIntoDatabaseKey(const url::Origin& origin) {
  return base::MD5String(origin.host());
}

}  // namespace

// Version history:
//
// - {no version}:
//     - Initial launch of the Database.
// - 1:
//     - Ignore the title/favicon events happening during the first few seconds
//       after a tab being loaded.
//     - Ignore the audio events happening during the first few seconds after a
//       tab being backgrounded.
//
// Transform logic:
//     - From {no version} to v1: The database is erased entirely.
const size_t LevelDBSiteDataStore::kDbVersion = 1U;

const char LevelDBSiteDataStore::kDbMetadataKey[] = "database_metadata";

// Helper class used to run all the blocking operations posted by
// LocalSiteCharacteristicDatabase on a ThreadPool sequence with the
// |MayBlock()| trait.
//
// Instances of this class should only be destructed once all the posted tasks
// have been run, in practice it means that they should ideally be stored in a
// std::unique_ptr<AsyncHelper, base::OnTaskRunnerDeleter>.
class LevelDBSiteDataStore::AsyncHelper {
 public:
  explicit AsyncHelper(const base::FilePath& db_path) : db_path_(db_path) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    // Setting |sync| to false might cause some data loss if the system crashes
    // but it'll make the write operations faster (no data will be lost if only
    // the process crashes).
    write_options_.sync = false;
  }

  AsyncHelper(const AsyncHelper&) = delete;
  AsyncHelper& operator=(const AsyncHelper&) = delete;

  ~AsyncHelper() = default;

  // Open the database from |db_path_| after creating it if it didn't exist,
  // this reset the database if it's not at the expected version.
  void OpenOrCreateDatabase();

  // Implementations of the DB manipulation functions of
  // LevelDBSiteDataStore that run on a blocking sequence.
  std::optional<SiteDataProto> ReadSiteDataFromDB(const url::Origin& origin);
  void WriteSiteDataIntoDB(const url::Origin& origin,
                           const SiteDataProto& site_characteristic_proto);
  void RemoveSiteDataFromDB(const std::vector<url::Origin>& site_origin);
  void ClearDatabase();
  // Returns a struct with unset fields on failure.
  DatabaseSizeResult GetDatabaseSize();

  bool DBIsInitialized() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_ != nullptr;
  }

  leveldb::DB* GetDBForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(DBIsInitialized());
    return db_.get();
  }

  void SetInitializationCallbackForTesting(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    init_callback_for_testing_ = std::move(callback);
    if (DBIsInitialized())
      std::move(init_callback_for_testing_).Run();
  }

 private:
  enum class OpeningType {
    // A new database has been created.
    kNewDb,
    // An existing database has been used.
    kExistingDb,
  };

  // Implementation for the OpenOrCreateDatabase function.
  OpeningType OpenOrCreateDatabaseImpl();

  // Implementation for the ClearDatabase function.
  void ClearDatabaseImpl();

  // A levelDB environment that gets used for testing. This allows using an
  // in-memory database when needed.
  std::unique_ptr<leveldb::Env> env_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The on disk location of the database.
  const base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The connection to the LevelDB database.
  std::unique_ptr<leveldb::DB> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The options to be used for all database read operations.
  leveldb::ReadOptions read_options_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The options to be used for all database write operations.
  leveldb::WriteOptions write_options_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure init_callback_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

void LevelDBSiteDataStore::AsyncHelper::OpenOrCreateDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OpeningType opening_type = OpenOrCreateDatabaseImpl();

  if (init_callback_for_testing_)
    std::move(init_callback_for_testing_).Run();

  if (!db_)
    return;
  std::string db_metadata;
  leveldb::Status s = db_->Get(
      read_options_, LevelDBSiteDataStore::kDbMetadataKey, &db_metadata);
  bool is_expected_version = false;
  if (s.ok()) {
    // The metadata only contains the version of the database as a size_t value
    // for now.
    size_t version = std::numeric_limits<size_t>::max();
    CHECK(base::StringToSizeT(db_metadata, &version));
    if (version == LevelDBSiteDataStore::kDbVersion)
      is_expected_version = true;
  }
  // TODO(sebmarchand): Add a migration engine rather than flushing the database
  // for every version change, https://crbug.com/866540.
  if ((opening_type == OpeningType::kExistingDb) && !is_expected_version) {
    DLOG(ERROR) << "Invalid DB version, recreating it.";
    ClearDatabaseImpl();
    // The database might fail to open.
    if (!db_)
      return;
    opening_type = OpeningType::kNewDb;
  }
  if (opening_type == OpeningType::kNewDb) {
    std::string metadata =
        base::NumberToString(LevelDBSiteDataStore::kDbVersion);
    s = db_->Put(write_options_, LevelDBSiteDataStore::kDbMetadataKey,
                 metadata);
    if (!s.ok()) {
      DLOG(ERROR) << "Error while inserting the metadata in the site "
                  << "characteristics database: " << s.ToString();
    }
  }
}

std::optional<SiteDataProto>
LevelDBSiteDataStore::AsyncHelper::ReadSiteDataFromDB(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return std::nullopt;

  leveldb::Status s;
  std::string protobuf_value;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    s = db_->Get(read_options_, SerializeOriginIntoDatabaseKey(origin),
                 &protobuf_value);
  }
  std::optional<SiteDataProto> site_characteristic_proto;
  if (s.ok()) {
    site_characteristic_proto = SiteDataProto();
    if (!site_characteristic_proto->ParseFromString(protobuf_value)) {
      site_characteristic_proto = std::nullopt;
      DLOG(ERROR) << "Error while trying to parse a SiteDataProto "
                  << "protobuf.";
    }
  }
  return site_characteristic_proto;
}

void LevelDBSiteDataStore::AsyncHelper::WriteSiteDataIntoDB(
    const url::Origin& origin,
    const SiteDataProto& site_characteristic_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return;

  leveldb::Status s;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    s = db_->Put(write_options_, SerializeOriginIntoDatabaseKey(origin),
                 site_characteristic_proto.SerializeAsString());
  }

  if (!s.ok()) {
    DLOG(ERROR)
        << "Error while inserting an element in the site characteristics "
        << "database: " << s.ToString();
  }
  base::UmaHistogramBoolean(
      "PerformanceManager.SiteDB.WriteCompleted.WriteSiteDataIntoStore", true);
}

void LevelDBSiteDataStore::AsyncHelper::RemoveSiteDataFromDB(
    const std::vector<url::Origin>& site_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  leveldb::WriteBatch batch;
  for (const auto& iter : site_origins)
    batch.Delete(SerializeOriginIntoDatabaseKey(iter));
  leveldb::Status status = db_->Write(write_options_, &batch);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to remove some entries from the site "
                 << "characteristics database: " << status.ToString();
  }
  base::UmaHistogramBoolean(
      "PerformanceManager.SiteDB.WriteCompleted.ClearSiteDataForOrigins", true);
}

void LevelDBSiteDataStore::AsyncHelper::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return;
  }

  ClearDatabaseImpl();
  base::UmaHistogramBoolean(
      "PerformanceManager.SiteDB.WriteCompleted.ClearAllSiteData", true);
}

DatabaseSizeResult LevelDBSiteDataStore::AsyncHelper::GetDatabaseSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DatabaseSizeResult();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DatabaseSizeResult ret;
#if BUILDFLAG(IS_WIN)
  // Windows has an annoying mis-feature that the size of an open file is not
  // written to the parent directory until the file is closed. Since this is a
  // diagnostic interface that should be rarely called, go to the trouble of
  // closing and re-opening the database in order to get an up-to date size to
  // report.
  db_.reset();
#endif
  ret.on_disk_size_kb = base::ComputeDirectorySize(db_path_) / 1024;
#if BUILDFLAG(IS_WIN)
  OpenOrCreateDatabase();
  if (!db_)
    return DatabaseSizeResult();
#endif

  // Default read options will fill the cache as we go.
  std::unique_ptr<leveldb::Iterator> iterator(
      db_->NewIterator(leveldb::ReadOptions()));
  int64_t num_rows = 0;
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next())
    ++num_rows;

  ret.num_rows = num_rows;
  return ret;
}

LevelDBSiteDataStore::AsyncHelper::OpeningType
LevelDBSiteDataStore::AsyncHelper::OpenOrCreateDatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_) << "Database already open";
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  OpeningType opening_type = OpeningType::kNewDb;

  // Report the on disk size of the database if it already exists.
  if (base::DirectoryExists(db_path_)) {
    opening_type = OpeningType::kExistingDb;
  }

  leveldb_env::Options options;
  options.create_if_missing = true;

  if (g_use_in_memory_db_for_testing.load(std::memory_order_relaxed)) {
    env_for_testing_ = leveldb_chrome::NewMemEnv("LevelDBSiteDataStore");
    options.env = env_for_testing_.get();
  }

  leveldb::Status status =
      leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);

  ReportInitStatus(kInitStatusHistogramLabel, status);

  if (status.ok())
    return opening_type;

  if (!ShouldAttemptDbRepair(status))
    return opening_type;

  if (RepairDatabase(db_path_.AsUTF8Unsafe())) {
    status = leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);
    ReportInitStatus(kInitStatusAfterRepairHistogramLabel, status);
    if (status.ok())
      return opening_type;
  }

  // Delete the database and try to open it one last time.
  if (leveldb_chrome::DeleteDB(db_path_, options).ok()) {
    status = leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);
    ReportInitStatus(kInitStatusAfterDeleteHistogramLabel, status);
    if (!status.ok())
      db_.reset();
  }

  return opening_type;
}

void LevelDBSiteDataStore::AsyncHelper::ClearDatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_) << "Database not open";

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  db_.reset();
  leveldb_env::Options options;
  leveldb::Status status = leveldb::DestroyDB(db_path_.AsUTF8Unsafe(), options);
  if (status.ok()) {
    OpenOrCreateDatabaseImpl();
  } else {
    LOG(WARNING) << "Failed to destroy the site characteristics database: "
                 << status.ToString();
  }
}

LevelDBSiteDataStore::LevelDBSiteDataStore(const base::FilePath& db_path)
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // The |BLOCK_SHUTDOWN| trait is required to ensure that a clearing of
          // the database won't be skipped.
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      async_helper_(new AsyncHelper(db_path),
                    base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::OpenOrCreateDatabase,
                     base::Unretained(async_helper_.get())));
}

LevelDBSiteDataStore::~LevelDBSiteDataStore() = default;

void LevelDBSiteDataStore::ReadSiteDataFromStore(
    const url::Origin& origin,
    SiteDataStore::ReadSiteDataFromStoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Trigger the asynchronous task and make it run the callback on this thread
  // once it returns.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::ReadSiteDataFromDB,
                     base::Unretained(async_helper_.get()), origin),
      std::move(callback));
}

void LevelDBSiteDataStore::WriteSiteDataIntoStore(
    const url::Origin& origin,
    const SiteDataProto& site_characteristic_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::WriteSiteDataIntoDB,
                     base::Unretained(async_helper_.get()), origin,
                     std::move(site_characteristic_proto)));
}

void LevelDBSiteDataStore::RemoveSiteDataFromStore(
    const std::vector<url::Origin>& site_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::RemoveSiteDataFromDB,
                     base::Unretained(async_helper_.get()),
                     std::move(site_origins)));
}

void LevelDBSiteDataStore::ClearStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::ClearDatabase,
                     base::Unretained(async_helper_.get())));
}

void LevelDBSiteDataStore::GetStoreSize(GetStoreSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Adapt the callback with a lambda to allow using PostTaskAndReplyWithResult.
  auto reply_callback = base::BindOnce(
      [](GetStoreSizeCallback callback, const DatabaseSizeResult& result) {
        std::move(callback).Run(result.num_rows, result.on_disk_size_kb);
      },
      std::move(callback));

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::GetDatabaseSize,
                     base::Unretained(async_helper_.get())),
      std::move(reply_callback));
}

void LevelDBSiteDataStore::SetInitializationCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::
                                    SetInitializationCallbackForTesting,
                                base::Unretained(async_helper_.get()),
                                std::move(callback)));
}

void LevelDBSiteDataStore::DatabaseIsInitializedForTesting(
    base::OnceCallback<void(bool)> reply_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteDataStore::AsyncHelper::DBIsInitialized,
                     base::Unretained(async_helper_.get())),
      std::move(reply_cb));
}

void LevelDBSiteDataStore::RunTaskWithRawDBForTesting(
    base::OnceCallback<void(leveldb::DB*)> task,
    base::OnceClosure after_task_run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task_on_blocking_task_runner = base::BindOnce(
      [](LevelDBSiteDataStore::AsyncHelper* helper,
         base::OnceCallback<void(leveldb::DB*)> task) {
        std::move(task).Run(helper->GetDBForTesting());  // IN-TEST
      },
      base::Unretained(async_helper_.get()), std::move(task));
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(task_on_blocking_task_runner),
      std::move(after_task_run_closure));
}

// static
base::ScopedClosureRunner LevelDBSiteDataStore::UseInMemoryDBForTesting() {
  g_use_in_memory_db_for_testing.store(true, std::memory_order_relaxed);
  return base::ScopedClosureRunner(base::BindOnce([] {
    g_use_in_memory_db_for_testing.store(false, std::memory_order_relaxed);
  }));
}

}  // namespace performance_manager
