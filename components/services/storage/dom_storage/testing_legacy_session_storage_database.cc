// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/testing_legacy_session_storage_database.h"

#include <inttypes.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char session_storage_uma_name[] = "SessionStorageDatabase.Open";

enum SessionStorageUMA {
  SESSION_STORAGE_UMA_SUCCESS,
  SESSION_STORAGE_UMA_RECREATED,
  SESSION_STORAGE_UMA_RECREATE_FAIL,  // Deprecated in M56 (issue 183679)
  SESSION_STORAGE_UMA_RECREATE_NOT_FOUND,
  SESSION_STORAGE_UMA_RECREATE_NOT_SUPPORTED,
  SESSION_STORAGE_UMA_RECREATE_CORRUPTION,
  SESSION_STORAGE_UMA_RECREATE_INVALID_ARGUMENT,
  SESSION_STORAGE_UMA_RECREATE_IO_ERROR,
  SESSION_STORAGE_UMA_MAX
};

}  // namespace

// Layout of the database:
// | key                            | value                              |
// -----------------------------------------------------------------------
// | map-1-                         | 2 (refcount, start of map-1-* keys)|
// | map-1-a                        | b (a = b in map 1)                 |
// | ...                            |                                    |
// | namespace-                     | dummy (start of namespace-* keys)  |
// | namespace-1- (1 = namespace id)| dummy (start of namespace-1-* keys)|
// | namespace-1-origin1            | 1 (mapid)                          |
// | namespace-1-origin2            | 2                                  |
// | namespace-2-                   | dummy                              |
// | namespace-2-origin1            | 1 (shallow copy)                   |
// | namespace-2-origin2            | 2 (shallow copy)                   |
// | namespace-3-                   | dummy                              |
// | namespace-3-origin1            | 3 (deep copy)                      |
// | namespace-3-origin2            | 2 (shallow copy)                   |
// | next-map-id                    | 4                                  |

namespace storage {

// This class keeps track of ongoing operations across different threads. When
// DB inconsistency is detected, we need to 1) make sure no new operations start
// 2) wait until all current operations finish, and let the last one of them
// close the DB and delete the data. The DB will remain empty for the rest of
// the run, and will be recreated during the next run. We cannot hope to recover
// during this run, since the upper layer will have a different idea about what
// should be in the database.
class TestingLegacySessionStorageDatabase::DBOperation {
 public:
  explicit DBOperation(
      TestingLegacySessionStorageDatabase* session_storage_database)
      : session_storage_database_(session_storage_database) {
    base::AutoLock auto_lock(session_storage_database_->db_lock_);
    ++session_storage_database_->operation_count_;
  }

  ~DBOperation() {
    base::AutoLock auto_lock(session_storage_database_->db_lock_);
    --session_storage_database_->operation_count_;
    if ((session_storage_database_->is_inconsistent_ ||
         session_storage_database_->db_error_) &&
        session_storage_database_->operation_count_ == 0 &&
        !session_storage_database_->invalid_db_deleted_) {
      // No other operations are ongoing and the data is bad -> delete it now.
      session_storage_database_->db_.reset();
      leveldb::DestroyDB(session_storage_database_->file_path_.AsUTF8Unsafe(),
                         leveldb_env::Options());
      session_storage_database_->invalid_db_deleted_ = true;
    }
  }

 private:
  TestingLegacySessionStorageDatabase* session_storage_database_;
};

TestingLegacySessionStorageDatabase::TestingLegacySessionStorageDatabase(
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> commit_task_runner)
    : RefCountedDeleteOnSequence<TestingLegacySessionStorageDatabase>(
          std::move(commit_task_runner)),
      file_path_(file_path),
      db_error_(false),
      is_inconsistent_(false),
      invalid_db_deleted_(false),
      operation_count_(0) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TestingLegacySessionStorageDatabase::~TestingLegacySessionStorageDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  db_.reset();
}

void TestingLegacySessionStorageDatabase::ReadAreaValues(
    const std::string& namespace_id,
    const std::vector<std::string>& original_permanent_namespace_ids,
    const url::Origin& origin,
    LegacyDomStorageValuesMap* result) {
  // We don't create a database if it doesn't exist. In that case, there is
  // nothing to be added to the result.
  if (!LazyOpen(false))
    return;
  DBOperation operation(this);

  // While ReadAreaValues is in progress, another thread can call
  // CommitAreaChanges. CommitAreaChanges might update map ref count key while
  // this thread is iterating over the map ref count key. To protect the reading
  // operation, create a snapshot and read from it.
  leveldb::ReadOptions options;
  options.snapshot = db_->GetSnapshot();

  std::string map_id;
  bool exists;
  if (GetMapForArea(namespace_id, origin.GetURL().spec(), options, &exists,
                    &map_id) &&
      exists)
    ReadMap(map_id, options, result, false);

  if (exists) {
    db_->ReleaseSnapshot(options.snapshot);
    return;
  }

  // If the area does not exist, |namespace_id| might refer to a clone that
  // is not yet created. Reading from the original database is expected to be
  // consistent because tasks posted on commit sequence after clone did not
  // run before capturing the snapshot.
  for (const auto& original_db_id : original_permanent_namespace_ids) {
    map_id.clear();
    if (GetMapForArea(original_db_id, origin.GetURL().spec(), options, &exists,
                      &map_id) &&
        exists) {
      ReadMap(map_id, options, result, false);
    }
    if (exists)
      break;
  }
  db_->ReleaseSnapshot(options.snapshot);
}

bool TestingLegacySessionStorageDatabase::CommitAreaChanges(
    const std::string& namespace_id,
    const url::Origin& origin,
    bool clear_all_first,
    const LegacyDomStorageValuesMap& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Even if |changes| is empty, we need to write the appropriate placeholders
  // in the database, so that it can be later shallow-copied successfully.
  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  leveldb::WriteBatch batch;
  // Ensure that the keys "namespace-" "namespace-N" (see the schema above)
  // exist.
  const bool kOkIfExists = true;
  if (!CreateNamespace(namespace_id, kOkIfExists, &batch))
    return false;

  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin.GetURL().spec(),
                     leveldb::ReadOptions(), &exists, &map_id))
    return false;

  if (exists) {
    int64_t ref_count;
    if (!GetMapRefCount(map_id, &ref_count))
      return false;
    if (ref_count > 1) {
      if (!DeepCopyArea(namespace_id, origin, !clear_all_first, &map_id,
                        &batch))
        return false;
    } else if (clear_all_first) {
      if (!ClearMap(map_id, &batch))
        return false;
    }
  } else {
    // Map doesn't exist, create it now if needed.
    if (!changes.empty()) {
      if (!CreateMapForArea(namespace_id, origin, &map_id, &batch))
        return false;
    }
  }

  WriteValuesToMap(map_id, changes, &batch);

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool TestingLegacySessionStorageDatabase::CloneNamespace(
    const std::string& namespace_id,
    const std::string& new_namespace_id) {
  // Go through all origins in the namespace |namespace_id|, create placeholders
  // for them in |new_namespace_id|, and associate them with the existing maps.

  // Example, data before shallow copy:
  // | map-1-                         | 1 (refcount)        |
  // | map-1-a                        | b                   |
  // | namespace-1- (1 = namespace id)| dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |

  // Example, data after shallow copy:
  // | map-1-                         | 2 (inc. refcount)   |
  // | map-1-a                        | b                   |
  // | namespace-1-(1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  leveldb::WriteBatch batch;
  const bool kOkIfExists = false;
  if (!CreateNamespace(new_namespace_id, kOkIfExists, &batch))
    return false;

  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;

  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    const std::string& map_id = it->second;
    if (!IncreaseMapRefCount(map_id, &batch))
      return false;
    AddAreaToNamespace(new_namespace_id, origin, map_id, &batch);
  }
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool TestingLegacySessionStorageDatabase::DeleteArea(
    const std::string& namespace_id,
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false)) {
    // No need to create the database if it doesn't exist.
    return true;
  }
  DBOperation operation(this);
  leveldb::WriteBatch batch;
  if (!DeleteAreaHelper(namespace_id, origin.GetURL().spec(), &batch))
    return false;
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool TestingLegacySessionStorageDatabase::DeleteNamespace(
    const std::string& namespace_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    // The caller should have called other methods to open the DB before this
    // function. Otherwise, DB stores nothing interesting related to the
    // specified namespace.
    // Do nothing if the DB is not open (or we know it has failed already),
    base::AutoLock auto_lock(db_lock_);
    if (!IsOpen() || db_error_ || is_inconsistent_)
      return false;
  }
  DBOperation operation(this);
  // Itereate through the areas in the namespace.
  leveldb::WriteBatch batch;
  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;
  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    if (!DeleteAreaHelper(namespace_id, origin, &batch))
      return false;
  }
  batch.Delete(NamespaceStartKey(namespace_id));
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool TestingLegacySessionStorageDatabase::ReadNamespacesAndOrigins(
    std::map<std::string, std::vector<url::Origin>>* namespaces_and_origins) {
  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  // While ReadNamespacesAndOrigins is in progress, another thread can call
  // CommitAreaChanges. To protect the reading operation, create a snapshot and
  // read from it.
  leveldb::ReadOptions options;
  options.snapshot = db_->GetSnapshot();

  std::string namespace_prefix = NamespacePrefix();
  std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  it->Seek(namespace_prefix);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid.
  if (!it->Valid()) {
    db_->ReleaseSnapshot(options.snapshot);
    return true;
  }

  if (!DatabaseErrorCheck(it->status().ok())) {
    db_->ReleaseSnapshot(options.snapshot);
    return false;
  }

  // Skip the dummy entry "namespace-" and iterate the namespaces.
  std::string current_namespace_start_key;
  std::string current_namespace_id;
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (!base::StartsWith(key, namespace_prefix,
                          base::CompareCase::SENSITIVE)) {
      // Iterated past the "namespace-" keys.
      break;
    }
    // For each namespace, the first key is "namespace-<namespaceid>-", and the
    // subsequent keys are "namespace-<namespaceid>-<origin>". Read the unique
    // "<namespaceid>" parts from the keys.
    if (current_namespace_start_key.empty() ||
        key.substr(0, current_namespace_start_key.length()) !=
            current_namespace_start_key) {
      // The key is of the form "namespace-<namespaceid>-" for a new
      // <namespaceid>.
      current_namespace_start_key = key;
      current_namespace_id =
          key.substr(namespace_prefix.length(),
                     key.length() - namespace_prefix.length() - 1);
      // Ensure that we keep track of the namespace even if it doesn't contain
      // any origins.
      namespaces_and_origins->insert(
          std::make_pair(current_namespace_id, std::vector<url::Origin>()));
    } else {
      // The key is of the form "namespace-<namespaceid>-<origin>".
      std::string origin = key.substr(current_namespace_start_key.length());
      (*namespaces_and_origins)[current_namespace_id].push_back(
          url::Origin::Create(GURL(origin)));
    }
  }
  db_->ReleaseSnapshot(options.snapshot);
  return true;
}

void TestingLegacySessionStorageDatabase::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(db_lock_);
  if (!db_)
    return;
  // All leveldb databases are already dumped by leveldb_env::DBTracker. Add
  // an edge to the existing dump.
  auto* tracker_dump =
      leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!tracker_dump)
    return;

  auto* mad = pmd->CreateAllocatorDump(
      base::StringPrintf("site_storage/session_storage/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  pmd->AddOwnershipEdge(mad->guid(), tracker_dump->guid());
  mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                 base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                 tracker_dump->GetSizeInternal());
}

void TestingLegacySessionStorageDatabase::SetDatabaseForTesting(
    std::unique_ptr<leveldb::DB> db) {
  CHECK(!db_);
  db_ = std::move(db);
}

bool TestingLegacySessionStorageDatabase::LazyOpen(bool create_if_needed) {
  base::AutoLock auto_lock(db_lock_);
  if (db_error_ || is_inconsistent_) {
    // Don't try to open a database that we know has failed already.
    return false;
  }
  if (IsOpen())
    return true;

  if (!create_if_needed &&
      (!base::PathExists(file_path_) || base::IsDirectoryEmpty(file_path_))) {
    // If the directory doesn't exist already and we haven't been asked to
    // create a file on disk, then we don't bother opening the database. This
    // means we wait until we absolutely need to put something onto disk before
    // we do so.
    return false;
  }

  leveldb::Status s = TryToOpen(&db_);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                 << ", error: " << s.ToString();

    // Clear the directory and try again.
    s = leveldb_chrome::DeleteDB(file_path_, leveldb_env::Options());
    if (!s.ok()) {
      LOG(WARNING) << "Failed to delete leveldb in " << file_path_.value()
                   << ", error: " << s.ToString();
    }
    s = TryToOpen(&db_);
    if (!s.ok()) {
      LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                   << ", error: " << s.ToString();
      if (s.IsNotFound()) {
        UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                  SESSION_STORAGE_UMA_RECREATE_NOT_FOUND,
                                  SESSION_STORAGE_UMA_MAX);
      } else if (s.IsNotSupportedError()) {
        UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                  SESSION_STORAGE_UMA_RECREATE_NOT_SUPPORTED,
                                  SESSION_STORAGE_UMA_MAX);
      } else if (s.IsCorruption()) {
        UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                  SESSION_STORAGE_UMA_RECREATE_CORRUPTION,
                                  SESSION_STORAGE_UMA_MAX);
      } else if (s.IsInvalidArgument()) {
        UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                  SESSION_STORAGE_UMA_RECREATE_INVALID_ARGUMENT,
                                  SESSION_STORAGE_UMA_MAX);
      } else if (s.IsIOError()) {
        UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                  SESSION_STORAGE_UMA_RECREATE_IO_ERROR,
                                  SESSION_STORAGE_UMA_MAX);
      } else {
        NOTREACHED();
      }

      db_error_ = true;
      return false;
    }
    UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                              SESSION_STORAGE_UMA_RECREATED,
                              SESSION_STORAGE_UMA_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                              SESSION_STORAGE_UMA_SUCCESS,
                              SESSION_STORAGE_UMA_MAX);
  }
  return true;
}

leveldb::Status TestingLegacySessionStorageDatabase::TryToOpen(
    std::unique_ptr<leveldb::DB>* db) {
  leveldb_env::Options options;
  // The directory exists but a valid leveldb database might not exist inside it
  // (e.g., a subset of the needed files might be missing). Handle this
  // situation gracefully by creating the database now.
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();

  std::string db_name = file_path_.AsUTF8Unsafe();
#if defined(OS_ANDROID)
  // On Android there is no support for session storage restoring, and since
  // the restoring code is responsible for database cleanup, we must manually
  // delete the old database here before we open it.
  leveldb::DestroyDB(db_name, options);
#endif
  leveldb::Status s = leveldb_env::OpenDB(options, db_name, db);
  if (!s.ok())
    return s;

  // If there is a version entry from the new implementation, treat the database
  // as corrupt.
  leveldb::Slice version_key =
      leveldb::Slice(reinterpret_cast<const char*>(
                         SessionStorageMetadata::kDatabaseVersionBytes),
                     sizeof(SessionStorageMetadata::kDatabaseVersionBytes));
  std::string dummy;
  s = (*db)->Get(leveldb::ReadOptions(), version_key, &dummy);
  if (s.IsNotFound())
    return leveldb::Status::OK();

  db->reset();
  return leveldb::Status::Corruption(
      "Cannot read a database that is a higher schema version.", dummy);
}

bool TestingLegacySessionStorageDatabase::IsOpen() const {
  return db_.get() != nullptr;
}

bool TestingLegacySessionStorageDatabase::CallerErrorCheck(bool ok) const {
  DCHECK(ok);
  return ok;
}

bool TestingLegacySessionStorageDatabase::ConsistencyCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  // We cannot recover the database during this run, e.g., the upper layer can
  // have a different understanding of the database state (shallow and deep
  // copies). Make further operations fail. The next operation that finishes
  // will delete the data, and next run will recerate the database.
  is_inconsistent_ = true;
  return false;
}

bool TestingLegacySessionStorageDatabase::DatabaseErrorCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  // Make further operations fail. The next operation that finishes
  // will delete the data, and next run will recerate the database.
  db_error_ = true;
  return false;
}

bool TestingLegacySessionStorageDatabase::CreateNamespace(
    const std::string& namespace_id,
    bool ok_if_exists,
    leveldb::WriteBatch* batch) {
  leveldb::Slice namespace_prefix = NamespacePrefix();
  std::string dummy;
  leveldb::Status s =
      db_->Get(leveldb::ReadOptions(), namespace_prefix, &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound())
    batch->Put(namespace_prefix, "");

  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  s = db_->Get(leveldb::ReadOptions(), namespace_start_key, &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound()) {
    batch->Put(namespace_start_key, "");
    return true;
  }
  return CallerErrorCheck(ok_if_exists);
}

bool TestingLegacySessionStorageDatabase::GetAreasInNamespace(
    const std::string& namespace_id,
    std::map<std::string, std::string>* areas) {
  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));
  it->Seek(namespace_start_key);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid.
  if (!it->Valid()) {
    // The namespace_start_key is not found when the namespace doesn't contain
    // any areas. We don't need to do anything.
    return true;
  }
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;

  // Skip the dummy entry "namespace-<namespaceid>-" and iterate the origins.
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (!base::StartsWith(key, namespace_start_key,
                          base::CompareCase::SENSITIVE)) {
      // Iterated past the origins for this namespace.
      break;
    }
    std::string origin = key.substr(namespace_start_key.length());
    std::string map_id = it->value().ToString();
    (*areas)[origin] = map_id;
  }
  return true;
}

void TestingLegacySessionStorageDatabase::AddAreaToNamespace(
    const std::string& namespace_id,
    const std::string& origin,
    const std::string& map_id,
    leveldb::WriteBatch* batch) {
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  batch->Put(namespace_key, map_id);
}

bool TestingLegacySessionStorageDatabase::DeleteAreaHelper(
    const std::string& namespace_id,
    const std::string& origin,
    leveldb::WriteBatch* batch) {
  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin, leveldb::ReadOptions(), &exists,
                     &map_id))
    return false;
  if (!exists)
    return true;  // Nothing to delete.
  if (!DecreaseMapRefCount(map_id, 1, batch))
    return false;
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  batch->Delete(namespace_key);

  // If this was the only area in the namespace, delete the namespace start key,
  // too.
  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));
  it->Seek(namespace_start_key);
  if (!ConsistencyCheck(it->Valid()))
    return false;
  // Advance the iterator 2 times (we still haven't really deleted
  // namespace_key).
  it->Next();
  if (!ConsistencyCheck(it->Valid()))
    return false;
  it->Next();
  if (!it->Valid())
    return true;
  std::string key = it->key().ToString();
  if (!base::StartsWith(key, namespace_start_key, base::CompareCase::SENSITIVE))
    batch->Delete(namespace_start_key);
  return true;
}

bool TestingLegacySessionStorageDatabase::GetMapForArea(
    const std::string& namespace_id,
    const std::string& origin,
    const leveldb::ReadOptions& options,
    bool* exists,
    std::string* map_id) {
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  leveldb::Status s = db_->Get(options, namespace_key, map_id);
  if (s.IsNotFound()) {
    *exists = false;
    return true;
  }
  *exists = true;
  return DatabaseErrorCheck(s.ok());
}

bool TestingLegacySessionStorageDatabase::CreateMapForArea(
    const std::string& namespace_id,
    const url::Origin& origin,
    std::string* map_id,
    leveldb::WriteBatch* batch) {
  leveldb::Slice next_map_id_key = NextMapIdKey();
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), next_map_id_key, map_id);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  int64_t next_map_id = 0;
  if (s.IsNotFound()) {
    *map_id = "0";
  } else {
    bool conversion_ok = base::StringToInt64(*map_id, &next_map_id);
    if (!ConsistencyCheck(conversion_ok))
      return false;
  }
  batch->Put(next_map_id_key, base::NumberToString(++next_map_id));
  std::string namespace_key =
      NamespaceKey(namespace_id, origin.GetURL().spec());
  batch->Put(namespace_key, *map_id);
  batch->Put(MapRefCountKey(*map_id), "1");
  return true;
}

bool TestingLegacySessionStorageDatabase::ReadMap(
    const std::string& map_id,
    const leveldb::ReadOptions& options,
    LegacyDomStorageValuesMap* result,
    bool only_keys) {
  std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  std::string map_start_key = MapRefCountKey(map_id);
  it->Seek(map_start_key);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid. The map needs to exist, otherwise we have
  // a stale map_id in the database.
  if (!ConsistencyCheck(it->Valid()))
    return false;
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;
  // Skip the dummy entry "map-<mapid>-".
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (!base::StartsWith(key, map_start_key, base::CompareCase::SENSITIVE)) {
      // Iterated past the keys in this map.
      break;
    }
    // Key is of the form "map-<mapid>-<key>".
    base::string16 key16 =
        base::UTF8ToUTF16(key.substr(map_start_key.length()));
    if (only_keys) {
      (*result)[key16] = base::NullableString16();
    } else {
      // Convert the raw data stored in std::string (it->value()) to raw data
      // stored in base::string16.
      size_t len = it->value().size() / sizeof(base::char16);
      const base::char16* data_ptr =
          reinterpret_cast<const base::char16*>(it->value().data());
      (*result)[key16] =
          base::NullableString16(base::string16(data_ptr, len), false);
    }
  }
  return true;
}

void TestingLegacySessionStorageDatabase::WriteValuesToMap(
    const std::string& map_id,
    const LegacyDomStorageValuesMap& values,
    leveldb::WriteBatch* batch) {
  for (auto it = values.begin(); it != values.end(); ++it) {
    base::NullableString16 value = it->second;
    std::string key = MapKey(map_id, base::UTF16ToUTF8(it->first));
    if (value.is_null()) {
      batch->Delete(key);
    } else {
      // Convert the raw data stored in base::string16 to raw data stored in
      // std::string.
      const char* data = reinterpret_cast<const char*>(value.string().data());
      size_t size = value.string().size() * 2;
      batch->Put(key, leveldb::Slice(data, size));
    }
  }
}

bool TestingLegacySessionStorageDatabase::GetMapRefCount(
    const std::string& map_id,
    int64_t* ref_count) {
  std::string ref_count_string;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), MapRefCountKey(map_id),
                               &ref_count_string);
  if (!ConsistencyCheck(s.ok()))
    return false;
  bool conversion_ok = base::StringToInt64(ref_count_string, ref_count);
  return ConsistencyCheck(conversion_ok);
}

bool TestingLegacySessionStorageDatabase::IncreaseMapRefCount(
    const std::string& map_id,
    leveldb::WriteBatch* batch) {
  // Increase the ref count for the map.
  int64_t old_ref_count;
  if (!GetMapRefCount(map_id, &old_ref_count))
    return false;
  batch->Put(MapRefCountKey(map_id), base::NumberToString(++old_ref_count));
  return true;
}

bool TestingLegacySessionStorageDatabase::DecreaseMapRefCount(
    const std::string& map_id,
    int decrease,
    leveldb::WriteBatch* batch) {
  // Decrease the ref count for the map.
  int64_t ref_count;
  if (!GetMapRefCount(map_id, &ref_count))
    return false;
  if (!ConsistencyCheck(decrease <= ref_count))
    return false;
  ref_count -= decrease;
  if (ref_count > 0) {
    batch->Put(MapRefCountKey(map_id), base::NumberToString(ref_count));
  } else {
    // Clear all keys in the map.
    if (!ClearMap(map_id, batch))
      return false;
    batch->Delete(MapRefCountKey(map_id));
  }
  return true;
}

bool TestingLegacySessionStorageDatabase::ClearMap(const std::string& map_id,
                                                   leveldb::WriteBatch* batch) {
  LegacyDomStorageValuesMap values;
  if (!ReadMap(map_id, leveldb::ReadOptions(), &values, true))
    return false;
  for (LegacyDomStorageValuesMap::const_iterator it = values.begin();
       it != values.end(); ++it)
    batch->Delete(MapKey(map_id, base::UTF16ToUTF8(it->first)));
  return true;
}

bool TestingLegacySessionStorageDatabase::DeepCopyArea(
    const std::string& namespace_id,
    const url::Origin& origin,
    bool copy_data,
    std::string* map_id,
    leveldb::WriteBatch* batch) {
  // Example, data before deep copy:
  // | namespace-1- (1 = namespace id)| dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map
  // | map-1-                         | 2 (refcount)        |
  // | map-1-a                        | b                   |

  // Example, data after deep copy copy:
  // | namespace-1-(1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 2 (mapid) << references the new map
  // | map-1-                         | 1 (dec. refcount)   |
  // | map-1-a                        | b                   |
  // | map-2-                         | 1 (refcount)        |
  // | map-2-a                        | b                   |

  // Read the values from the old map here. If we don't need to copy the data,
  // this can stay empty.
  LegacyDomStorageValuesMap values;
  if (copy_data && !ReadMap(*map_id, leveldb::ReadOptions(), &values, false))
    return false;
  if (!DecreaseMapRefCount(*map_id, 1, batch))
    return false;
  // Create a new map (this will also break the association to the old map) and
  // write the old data into it. This will write the id of the created map into
  // |map_id|.
  if (!CreateMapForArea(namespace_id, origin, map_id, batch))
    return false;
  WriteValuesToMap(*map_id, values, batch);
  return true;
}

std::string TestingLegacySessionStorageDatabase::NamespaceStartKey(
    const std::string& namespace_id) {
  return base::StringPrintf("namespace-%s-", namespace_id.c_str());
}

std::string TestingLegacySessionStorageDatabase::NamespaceKey(
    const std::string& namespace_id,
    const std::string& origin) {
  return base::StringPrintf("namespace-%s-%s", namespace_id.c_str(),
                            origin.c_str());
}

const char* TestingLegacySessionStorageDatabase::NamespacePrefix() {
  return "namespace-";
}

std::string TestingLegacySessionStorageDatabase::MapRefCountKey(
    const std::string& map_id) {
  return base::StringPrintf("map-%s-", map_id.c_str());
}

std::string TestingLegacySessionStorageDatabase::MapKey(
    const std::string& map_id,
    const std::string& key) {
  return base::StringPrintf("map-%s-%s", map_id.c_str(), key.c_str());
}

const char* TestingLegacySessionStorageDatabase::NextMapIdKey() {
  return "next-map-id";
}

}  // namespace storage
