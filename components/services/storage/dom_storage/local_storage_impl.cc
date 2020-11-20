// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/local_storage_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <cctype>  // for std::isalnum
#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/legacy_dom_storage_database.h"
#include "components/services/storage/dom_storage/local_storage_database.pb.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/public/cpp/constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "sql/database.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace storage {

// LevelDB database schema
// =======================
//
// Version 1 (in sorted order):
//   key: "VERSION"
//   value: "1"
//
//   key: "META:" + <url::Origin 'origin'>
//   value: <LocalStorageOriginMetaData serialized as a string>
//
//   key: "_" + <url::Origin> 'origin'> + '\x00' + <script controlled key>
//   value: <script controlled value>

namespace {

// Temporary alias as this code moves incrementally into the storage namespace.
using StorageAreaImpl = StorageAreaImpl;

constexpr base::StringPiece kVersionKey = "VERSION";
const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};
const int64_t kMinSchemaVersion = 1;
const int64_t kCurrentLocalStorageSchemaVersion = 1;

// After this many consecutive commit errors we'll throw away the entire
// database.
const int kCommitErrorThreshold = 8;

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if defined(OS_ANDROID)
const unsigned kMaxLocalStorageAreaCount = 10;
const size_t kMaxLocalStorageCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxLocalStorageAreaCount = 50;
const size_t kMaxLocalStorageCacheSize = 20 * 1024 * 1024;
#endif

static const uint8_t kUTF16Format = 0;
static const uint8_t kLatin1Format = 1;

DomStorageDatabase::Key CreateMetaDataKey(const url::Origin& origin) {
  auto origin_str = origin.Serialize();
  std::vector<uint8_t> serialized_origin(origin_str.begin(), origin_str.end());
  DomStorageDatabase::Key key;
  key.reserve(base::size(kMetaPrefix) + serialized_origin.size());
  key.insert(key.end(), kMetaPrefix, kMetaPrefix + base::size(kMetaPrefix));
  key.insert(key.end(), serialized_origin.begin(), serialized_origin.end());
  return key;
}

base::Optional<url::Origin> ExtractOriginFromMetaDataKey(
    const DomStorageDatabase::Key& key) {
  DCHECK_GT(key.size(), base::size(kMetaPrefix));
  const base::StringPiece key_string(reinterpret_cast<const char*>(key.data()),
                                     key.size());
  const GURL url(key_string.substr(base::size(kMetaPrefix)));
  if (!url.is_valid())
    return base::nullopt;
  return url::Origin::Create(url);
}

void SuccessResponse(base::OnceClosure callback, bool success) {
  std::move(callback).Run();
}

void IgnoreStatus(base::OnceClosure callback, leveldb::Status status) {
  std::move(callback).Run();
}

void MigrateStorageHelper(
    base::FilePath db_path,
    const scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    base::OnceCallback<void(std::unique_ptr<StorageAreaImpl::ValueMap>)>
        callback) {
  LegacyDomStorageDatabase db(db_path, CreateFilesystemProxy());
  LegacyDomStorageValuesMap map;
  db.ReadAllValues(&map);
  auto values = std::make_unique<StorageAreaImpl::ValueMap>();
  for (const auto& it : map) {
    (*values)[LocalStorageImpl::MigrateString(it.first)] =
        LocalStorageImpl::MigrateString(it.second.string());
  }
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(values)));
}

// Helper to convert from OnceCallback to Callback.
void CallMigrationCalback(StorageAreaImpl::ValueMapCallback callback,
                          std::unique_ptr<StorageAreaImpl::ValueMap> data) {
  std::move(callback).Run(std::move(data));
}

DomStorageDatabase::Key MakeOriginPrefix(const url::Origin& origin) {
  const char kDataPrefix = '_';
  const std::string serialized_origin = origin.Serialize();
  const char kOriginSeparator = '\x00';

  DomStorageDatabase::Key prefix;
  prefix.reserve(serialized_origin.size() + 2);
  prefix.push_back(kDataPrefix);
  prefix.insert(prefix.end(), serialized_origin.begin(),
                serialized_origin.end());
  prefix.push_back(kOriginSeparator);
  return prefix;
}

void DeleteOrigins(AsyncDomStorageDatabase* database,
                   std::vector<url::Origin> origins,
                   base::OnceCallback<void(leveldb::Status)> callback) {
  database->RunDatabaseTask(
      base::BindOnce(
          [](std::vector<url::Origin> origins, const DomStorageDatabase& db) {
            leveldb::WriteBatch batch;
            for (const auto& origin : origins) {
              db.DeletePrefixed(MakeOriginPrefix(origin), &batch);
              batch.Delete(leveldb_env::MakeSlice(CreateMetaDataKey(origin)));
            }
            return db.Commit(&batch);
          },
          std::move(origins)),
      std::move(callback));
}

enum class CachePurgeReason {
  NotNeeded,
  SizeLimitExceeded,
  AreaCountLimitExceeded,
  InactiveOnLowEndDevice,
  AggressivePurgeTriggered
};

void RecordCachePurgedHistogram(CachePurgeReason reason,
                                size_t purged_size_kib) {
  UMA_HISTOGRAM_COUNTS_100000("LocalStorageContext.CachePurgedInKB",
                              purged_size_kib);
  switch (reason) {
    case CachePurgeReason::SizeLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.SizeLimitExceeded",
          purged_size_kib);
      break;
    case CachePurgeReason::AreaCountLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.AreaCountLimitExceeded",
          purged_size_kib);
      break;
    case CachePurgeReason::InactiveOnLowEndDevice:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.InactiveOnLowEndDevice",
          purged_size_kib);
      break;
    case CachePurgeReason::AggressivePurgeTriggered:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.AggressivePurgeTriggered",
          purged_size_kib);
      break;
    case CachePurgeReason::NotNeeded:
      NOTREACHED();
      break;
  }
}

const base::FilePath::CharType kLegacyDatabaseFileExtension[] =
    FILE_PATH_LITERAL(".localstorage");

std::vector<mojom::LocalStorageUsageInfoPtr> GetLegacyLocalStorageUsage(
    const base::FilePath& directory) {
  std::unique_ptr<FilesystemProxy> fs = CreateFilesystemProxy();
  FileErrorOr<std::vector<base::FilePath>> result = fs->GetDirectoryEntries(
      directory, FilesystemProxy::DirectoryEntryType::kFilesOnly);
  if (result.is_error())
    return {};

  std::vector<mojom::LocalStorageUsageInfoPtr> infos;
  for (const auto& path : result.value()) {
    if (!path.MatchesExtension(kLegacyDatabaseFileExtension))
      continue;
    base::Optional<base::File::Info> info = fs->GetFileInfo(path);
    if (!info)
      continue;
    infos.push_back(mojom::LocalStorageUsageInfo::New(
        LocalStorageImpl::OriginFromLegacyDatabaseFileName(path), info->size,
        info->last_modified));
  }
  return infos;
}

void InvokeLocalStorageUsageCallbackHelper(
    LocalStorageImpl::GetUsageCallback callback,
    std::unique_ptr<std::vector<mojom::LocalStorageUsageInfoPtr>> infos) {
  std::move(callback).Run(std::move(*infos));
}

void CollectLocalStorageUsage(
    std::vector<mojom::LocalStorageUsageInfoPtr>* out_info,
    base::OnceClosure done_callback,
    std::vector<mojom::LocalStorageUsageInfoPtr> in_info) {
  out_info->reserve(out_info->size() + in_info.size());
  for (auto& info : in_info)
    out_info->push_back(std::move(info));
  std::move(done_callback).Run();
}

}  // namespace

class LocalStorageImpl::StorageAreaHolder final
    : public StorageAreaImpl::Delegate {
 public:
  StorageAreaHolder(LocalStorageImpl* context, const url::Origin& origin)
      : context_(context), origin_(origin) {
    // Delay for a moment after a value is set in anticipation
    // of other values being set, so changes are batched.
    static constexpr base::TimeDelta kCommitDefaultDelaySecs =
        base::TimeDelta::FromSeconds(5);

    // To avoid excessive IO we apply limits to the amount of data being written
    // and the frequency of writes.
    static const size_t kMaxBytesPerHour = kPerStorageAreaQuota;
    static constexpr int kMaxCommitsPerHour = 60;

    StorageAreaImpl::Options options;
    options.max_size = kPerStorageAreaQuota + kPerStorageAreaOverQuotaAllowance;
    options.default_commit_delay = kCommitDefaultDelaySecs;
    options.max_bytes_per_hour = kMaxBytesPerHour;
    options.max_commits_per_hour = kMaxCommitsPerHour;
#if defined(OS_ANDROID)
    options.cache_mode = StorageAreaImpl::CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
#else
    options.cache_mode = StorageAreaImpl::CacheMode::KEYS_AND_VALUES;
    if (base::SysInfo::IsLowEndDevice()) {
      options.cache_mode = StorageAreaImpl::CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
    }
#endif
    area_ = std::make_unique<StorageAreaImpl>(
        context_->database_.get(), MakeOriginPrefix(origin_), this, options);
    area_ptr_ = area_.get();
  }

  StorageAreaImpl* storage_area() { return area_ptr_; }

  void OnNoBindings() override {
    has_bindings_ = false;
    // Don't delete ourselves, but do schedule an immediate commit. Possible
    // deletion will happen under memory pressure or when another localstorage
    // area is opened.
    storage_area()->ScheduleImmediateCommit();
  }

  void PrepareToCommit(
      std::vector<DomStorageDatabase::KeyValuePair>* extra_entries_to_add,
      std::vector<DomStorageDatabase::Key>* extra_keys_to_delete) override {
    // Write schema version if not already done so before.
    if (!context_->database_initialized_) {
      const std::string version =
          base::NumberToString(kCurrentLocalStorageSchemaVersion);
      extra_entries_to_add->emplace_back(
          DomStorageDatabase::Key(kVersionKey.begin(), kVersionKey.end()),
          DomStorageDatabase::Value(version.begin(), version.end()));
      context_->database_initialized_ = true;
    }

    DomStorageDatabase::Key metadata_key = CreateMetaDataKey(origin_);
    if (storage_area()->empty()) {
      extra_keys_to_delete->push_back(std::move(metadata_key));
    } else {
      storage::LocalStorageOriginMetaData data;
      data.set_last_modified(base::Time::Now().ToInternalValue());
      data.set_size_bytes(storage_area()->storage_used());
      std::string serialized_data = data.SerializeAsString();
      extra_entries_to_add->emplace_back(
          std::move(metadata_key),
          DomStorageDatabase::Value(serialized_data.begin(),
                                    serialized_data.end()));
    }
  }

  void DidCommit(leveldb::Status status) override {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.CommitResult",
                              leveldb_env::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);

    // Delete any old database that might still exist if we successfully wrote
    // data to LevelDB, and our LevelDB is actually disk backed.
    if (status.ok() && !deleted_old_data_ && !context_->directory_.empty() &&
        context_->legacy_task_runner_) {
      deleted_old_data_ = true;
      context_->legacy_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(base::IgnoreResult(&sql::Database::Delete),
                                    sql_db_path()));
    }

    context_->OnCommitResult(status);
  }

  void MigrateData(StorageAreaImpl::ValueMapCallback callback) override {
    if (context_->legacy_task_runner_ && !context_->directory_.empty()) {
      context_->legacy_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&MigrateStorageHelper, sql_db_path(),
                                    base::ThreadTaskRunnerHandle::Get(),
                                    base::BindOnce(&CallMigrationCalback,
                                                   std::move(callback))));
      return;
    }
    std::move(callback).Run(nullptr);
  }

  std::vector<StorageAreaImpl::Change> FixUpData(
      const StorageAreaImpl::ValueMap& data) override {
    std::vector<StorageAreaImpl::Change> changes;
    // Chrome M61/M62 had a bug where keys that should have been encoded as
    // Latin1 were instead encoded as UTF16. Fix this by finding any 8-bit only
    // keys, and re-encode those. If two encodings of the key exist, the Latin1
    // encoded value should take precedence.
    size_t fix_count = 0;
    for (const auto& it : data) {
      // Skip over any Latin1 encoded keys, or unknown encodings/corrupted data.
      if (it.first.empty() || it.first[0] != kUTF16Format)
        continue;
      // Check if key is actually 8-bit safe.
      bool is_8bit = true;
      for (size_t i = 1; i < it.first.size(); i += sizeof(base::char16)) {
        // Don't just cast to char16* as that could be undefined behavior.
        // Instead use memcpy for the conversion, which compilers will generally
        // optimize away anyway.
        base::char16 char_val;
        memcpy(&char_val, it.first.data() + i, sizeof(base::char16));
        if (char_val & 0xff00) {
          is_8bit = false;
          break;
        }
      }
      if (!is_8bit)
        continue;
      // Found a key that should have been encoded differently. Decode and
      // re-encode.
      std::vector<uint8_t> key(1 + (it.first.size() - 1) / 2);
      key[0] = kLatin1Format;
      for (size_t in = 1, out = 1; in < it.first.size();
           in += sizeof(base::char16), out++) {
        base::char16 char_val;
        memcpy(&char_val, it.first.data() + in, sizeof(base::char16));
        key[out] = char_val;
      }
      // Delete incorrect key.
      changes.push_back(std::make_pair(it.first, base::nullopt));
      fix_count++;
      // Check if correct key already exists in data.
      auto new_it = data.find(key);
      if (new_it != data.end())
        continue;
      // Update value for correct key.
      changes.push_back(std::make_pair(key, it.second));
    }
    UMA_HISTOGRAM_BOOLEAN("LocalStorageContext.MigrationFixUpNeeded",
                          fix_count != 0);
    return changes;
  }

  void OnMapLoaded(leveldb::Status status) override {
    if (!status.ok()) {
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.MapLoadError",
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
  }

  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
    has_bindings_ = true;
    storage_area()->Bind(std::move(receiver));
  }

  bool has_bindings() const { return has_bindings_; }

 private:
  base::FilePath sql_db_path() const {
    if (context_->directory_.empty())
      return base::FilePath();
    return context_->directory_.Append(
        LocalStorageImpl::LegacyDatabaseFileNameFromOrigin(origin_));
  }

  LocalStorageImpl* context_;
  url::Origin origin_;
  std::unique_ptr<StorageAreaImpl> area_;
  // Holds the same value as |area_|. The reason for this is that
  // during destruction of the StorageAreaImpl instance we might still get
  // called and need access  to the StorageAreaImpl instance. The unique_ptr
  // could already be null, but this field should still be valid.
  StorageAreaImpl* area_ptr_;
  bool deleted_old_data_ = false;
  bool has_bindings_ = false;
};

// static
base::FilePath LocalStorageImpl::LegacyDatabaseFileNameFromOrigin(
    const url::Origin& origin) {
  std::string filename = GetIdentifierFromOrigin(origin);
  // There is no base::FilePath.AppendExtension() method, so start with just the
  // extension as the filename, and then InsertBeforeExtension the desired
  // name.
  return base::FilePath()
      .Append(kLegacyDatabaseFileExtension)
      .InsertBeforeExtensionASCII(filename);
}

// static
url::Origin LocalStorageImpl::OriginFromLegacyDatabaseFileName(
    const base::FilePath& name) {
  DCHECK(name.MatchesExtension(kLegacyDatabaseFileExtension));
  std::string origin_id = name.BaseName().RemoveExtension().MaybeAsASCII();
  return GetOriginFromIdentifier(origin_id);
}

LocalStorageImpl::LocalStorageImpl(
    const base::FilePath& storage_root,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> legacy_task_runner,
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver)
    : directory_(storage_root.empty() ? storage_root
                                      : storage_root.Append(kLocalStoragePath)),
      leveldb_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      memory_dump_id_(base::StringPrintf("LocalStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      legacy_task_runner_(std::move(legacy_task_runner)),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "LocalStorage", task_runner, MemoryDumpProvider::Options());

  if (receiver) {
    control_receiver_.Bind(std::move(receiver));
    control_receiver_.set_disconnect_handler(base::BindOnce(
        &LocalStorageImpl::ShutdownAndDelete, base::Unretained(this)));
  }
}

void LocalStorageImpl::BindStorageArea(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::BindStorageArea,
                                    weak_ptr_factory_.GetWeakPtr(), origin,
                                    std::move(receiver)));
    return;
  }

  GetOrCreateStorageArea(origin)->Bind(std::move(receiver));
}

void LocalStorageImpl::GetUsage(GetUsageCallback callback) {
  RunWhenConnected(base::BindOnce(&LocalStorageImpl::RetrieveStorageUsage,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void LocalStorageImpl::DeleteStorage(const url::Origin& origin,
                                     DeleteStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), origin,
                                    std::move(callback)));
    return;
  }

  auto found = areas_.find(origin);
  if (found != areas_.end()) {
    // Renderer process expects |source| to always be two newline separated
    // strings. We don't bother passing an observer because this is a one-shot
    // event and we only care about observing its completion, for which the
    // reply alone is sufficient.
    found->second->storage_area()->DeleteAll(
        "\n", /*new_observer=*/mojo::NullRemote(),
        base::BindOnce(&SuccessResponse, std::move(callback)));
    found->second->storage_area()->ScheduleImmediateCommit();
  } else if (database_) {
    DeleteOrigins(
        database_.get(), {origin},
        base::BindOnce([](base::OnceClosure callback,
                          leveldb::Status) { std::move(callback).Run(); },
                       std::move(callback)));
  } else {
    std::move(callback).Run();
  }

  if (!directory_.empty()) {
    legacy_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&sql::Database::Delete),
            directory_.Append(LegacyDatabaseFileNameFromOrigin(origin))));
  }
}

void LocalStorageImpl::CleanUpStorage(CleanUpStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::CleanUpStorage,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  if (database_) {
    // Try to commit all changes before rewriting the database. If
    // an area is not ready to commit its changes, nothing breaks but the
    // rewrite doesn't remove all traces of old data.
    Flush(base::DoNothing());
    database_->RewriteDB(base::BindOnce(&IgnoreStatus, std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void LocalStorageImpl::Flush(FlushCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::Flush,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  base::RepeatingClosure commit_callback = base::BarrierClosure(
      base::saturated_cast<int>(areas_.size()), std::move(callback));
  for (const auto& it : areas_)
    it.second->storage_area()->ScheduleImmediateCommit(commit_callback);
}

void LocalStorageImpl::FlushOriginForTesting(const url::Origin& origin) {
  if (connection_state_ != CONNECTION_FINISHED)
    return;
  const auto& it = areas_.find(origin);
  if (it == areas_.end())
    return;
  it->second->storage_area()->ScheduleImmediateCommit();
}

void LocalStorageImpl::ShutdownAndDelete() {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete(leveldb::Status::OK());
    return;
  }

  connection_state_ = CONNECTION_SHUTDOWN;

  // Flush any uncommitted data.
  for (const auto& it : areas_) {
    auto* area = it.second->storage_area();
    LOCAL_HISTOGRAM_BOOLEAN(
        "LocalStorageContext.ShutdownAndDelete.MaybeDroppedChanges",
        area->has_pending_load_tasks());
    area->ScheduleImmediateCommit();
    // TODO(dmurph): Monitor the above histogram, and if dropping changes is
    // common then handle that here.
    area->CancelAllPendingRequests();
  }

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_) {
    OnShutdownComplete(leveldb::Status::OK());
    return;  // Keep everything.
  }

  if (!origins_to_purge_on_shutdown_.empty()) {
    RetrieveStorageUsage(
        base::BindOnce(&LocalStorageImpl::OnGotStorageUsageForShutdown,
                       base::Unretained(this)));
  } else {
    OnShutdownComplete(leveldb::Status::OK());
  }
}

void LocalStorageImpl::PurgeMemory() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  for (auto it = areas_.begin(); it != areas_.end();) {
    if (it->second->has_bindings()) {
      it->second->storage_area()->PurgeMemory();
      ++it;
    } else {
      it = areas_.erase(it);
    }
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordCachePurgedHistogram(CachePurgeReason::AggressivePurgeTriggered,
                             purged_size_kib);
}

void LocalStorageImpl::ApplyPolicyUpdates(
    std::vector<mojom::LocalStoragePolicyUpdatePtr> policy_updates) {
  for (const auto& update : policy_updates) {
    GURL url = update->origin.GetURL();
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(url);
    else
      origins_to_purge_on_shutdown_.insert(std::move(url));
  }
}

void LocalStorageImpl::PurgeUnusedAreasIfNeeded() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Nothing to purge.
  if (!unused_area_count)
    return;

  CachePurgeReason purge_reason = CachePurgeReason::NotNeeded;

  if (total_cache_size > kMaxLocalStorageCacheSize)
    purge_reason = CachePurgeReason::SizeLimitExceeded;
  else if (areas_.size() > kMaxLocalStorageAreaCount)
    purge_reason = CachePurgeReason::AreaCountLimitExceeded;
  else if (is_low_end_device_)
    purge_reason = CachePurgeReason::InactiveOnLowEndDevice;

  if (purge_reason == CachePurgeReason::NotNeeded)
    return;

  for (auto it = areas_.begin(); it != areas_.end();) {
    if (it->second->has_bindings())
      ++it;
    else
      it = areas_.erase(it);
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordCachePurgedHistogram(purge_reason, purged_size_kib);
}

void LocalStorageImpl::ForceKeepSessionState() {
  SetForceKeepSessionState();
}

bool LocalStorageImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (connection_state_ != CONNECTION_FINISHED)
    return true;

  std::string context_name =
      base::StringPrintf("site_storage/localstorage/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));

  // Account for leveldb memory usage, which actually lives in the file service.
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(memory_dump_id_);
  // The size of the leveldb dump will be added by the leveldb service.
  auto* leveldb_mad = pmd->CreateAllocatorDump(context_name + "/leveldb");
  // Specifies that the current context is responsible for keeping memory alive.
  int kImportance = 2;
  pmd->AddOwnershipEdge(leveldb_mad->guid(), global_dump->guid(), kImportance);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    size_t total_cache_size, unused_area_count;
    GetStatistics(&total_cache_size, &unused_area_count);
    auto* mad = pmd->CreateAllocatorDump(context_name + "/cache_size");
    mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                   base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                   total_cache_size);
    mad->AddScalar("total_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   areas_.size());
    return true;
  }
  for (const auto& it : areas_) {
    // Limit the url length to 50 and strip special characters.
    std::string url = it.first.Serialize().substr(0, 50);
    for (size_t index = 0; index < url.size(); ++index) {
      if (!std::isalnum(url[index]))
        url[index] = '_';
    }
    std::string area_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), url.c_str(),
        reinterpret_cast<uintptr_t>(it.second->storage_area()));
    it.second->storage_area()->OnMemoryDump(area_dump_name, pmd);
  }
  return true;
}

// static
std::vector<uint8_t> LocalStorageImpl::MigrateString(
    const base::string16& input) {
  // TODO(mek): Deduplicate this somehow with the code in
  // LocalStorageCachedArea::String16ToUint8Vector.
  bool is_8bit = true;
  for (const auto& c : input) {
    if (c & 0xff00) {
      is_8bit = false;
      break;
    }
  }
  if (is_8bit) {
    std::vector<uint8_t> result(input.size() + 1);
    result[0] = kLatin1Format;
    std::copy(input.begin(), input.end(), result.begin() + 1);
    return result;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
  std::vector<uint8_t> result;
  result.reserve(input.size() * sizeof(base::char16) + 1);
  result.push_back(kUTF16Format);
  result.insert(result.end(), data, data + input.size() * sizeof(base::char16));
  return result;
}

void LocalStorageImpl::SetDatabaseOpenCallbackForTesting(
    base::OnceClosure callback) {
  RunWhenConnected(std::move(callback));
}

LocalStorageImpl::~LocalStorageImpl() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void LocalStorageImpl::RunWhenConnected(base::OnceClosure callback) {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // If we don't have a database connection, we'll need to establish one.
  if (connection_state_ == NO_CONNECTION) {
    connection_state_ = CONNECTION_IN_PROGRESS;
    InitiateConnection();
  }

  if (connection_state_ == CONNECTION_IN_PROGRESS) {
    // Queue this OpenLocalStorage call for when we have a level db pointer.
    on_database_opened_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}

void LocalStorageImpl::InitiateConnection(bool in_memory_only) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (!directory_.empty() && directory_.IsAbsolute() && !in_memory_only) {
    // We were given a subdirectory to write to, so use a disk-backed database.
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // use minimum
    // Default write_buffer_size is 4 MB but that might leave a 3.999
    // memory allocation in RAM from a log file recovery.
    options.write_buffer_size = 64 * 1024;
    options.block_cache = leveldb_chrome::GetSharedWebBlockCache();

    in_memory_ = false;
    database_ = AsyncDomStorageDatabase::OpenDirectory(
        std::move(options), directory_, kLocalStorageLeveldbName,
        memory_dump_id_, leveldb_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = AsyncDomStorageDatabase::OpenInMemory(
      memory_dump_id_, "local-storage", leveldb_task_runner_,
      base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalStorageImpl::OnDatabaseOpened(leveldb::Status status) {
  if (!status.ok()) {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError",
                              leveldb_env::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    if (in_memory_) {
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError.Memory",
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError.Disk",
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
    LogDatabaseOpenResult(OpenResult::DATABASE_OPEN_FAILED);
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase("LocalStorageContext.OpenResultAfterOpenFailed");
    return;
  }

  // Verify DB schema version.
  if (database_) {
    database_->Get(std::vector<uint8_t>(kVersionKey.begin(), kVersionKey.end()),
                   base::BindOnce(&LocalStorageImpl::OnGotDatabaseVersion,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnConnectionFinished();
}

void LocalStorageImpl::OnGotDatabaseVersion(leveldb::Status status,
                                            const std::vector<uint8_t>& value) {
  if (status.IsNotFound()) {
    // New database, nothing more to do. Current version will get written
    // when first data is committed.
  } else if (status.ok()) {
    // Existing database, check if version number matches current schema
    // version.
    int64_t db_version;
    if (!base::StringToInt64(std::string(value.begin(), value.end()),
                             &db_version) ||
        db_version < kMinSchemaVersion ||
        db_version > kCurrentLocalStorageSchemaVersion) {
      LogDatabaseOpenResult(OpenResult::INVALID_VERSION);
      DeleteAndRecreateDatabase(
          "LocalStorageContext.OpenResultAfterInvalidVersion");
      return;
    }

    database_initialized_ = true;
  } else {
    // Other read error. Possibly database corruption.
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.ReadVersionError",
                              leveldb_env::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    LogDatabaseOpenResult(OpenResult::VERSION_READ_ERROR);
    DeleteAndRecreateDatabase(
        "LocalStorageContext.OpenResultAfterReadVersionError");
    return;
  }

  OnConnectionFinished();
}

void LocalStorageImpl::OnConnectionFinished() {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

  LogDatabaseOpenResult(OpenResult::SUCCESS);
  open_result_histogram_ = nullptr;

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  for (size_t i = 0; i < on_database_opened_callbacks_.size(); ++i)
    std::move(on_database_opened_callbacks_[i]).Run();
  on_database_opened_callbacks_.clear();
}

void LocalStorageImpl::DeleteAndRecreateDatabase(const char* histogram_name) {
  // We're about to set database_ to null, so delete the StorageAreaImpls
  // that might still be using the old database.
  for (const auto& it : areas_)
    it.second->storage_area()->CancelAllPendingRequests();
  areas_.clear();

  // Reset state to be in process of connecting. This will cause requests for
  // StorageAreas to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  commit_error_count_ = 0;
  database_.reset();
  open_result_histogram_ = histogram_name;

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_ && !in_memory_) {
    recreate_in_memory = true;
  } else if (tried_to_recreate_during_open_) {
    // Give up completely, run without any database.
    OnConnectionFinished();
    return;
  }

  tried_to_recreate_during_open_ = true;

  // Destroy database, and try again.
  if (!in_memory_) {
    DomStorageDatabase::Destroy(
        directory_, kLocalStorageLeveldbName, leveldb_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void LocalStorageImpl::OnDBDestroyed(bool recreate_in_memory,
                                     leveldb::Status status) {
  UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DestroyDBResult",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

LocalStorageImpl::StorageAreaHolder* LocalStorageImpl::GetOrCreateStorageArea(
    const url::Origin& origin) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto found = areas_.find(origin);
  if (found != areas_.end()) {
    return found->second.get();
  }

  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Track the total localStorage cache size.
  UMA_HISTOGRAM_COUNTS_100000("LocalStorageContext.CacheSizeInKB",
                              total_cache_size / 1024);

  PurgeUnusedAreasIfNeeded();

  auto holder = std::make_unique<StorageAreaHolder>(this, origin);
  StorageAreaHolder* holder_ptr = holder.get();
  areas_[origin] = std::move(holder);
  return holder_ptr;
}

void LocalStorageImpl::RetrieveStorageUsage(GetUsageCallback callback) {
  auto infos = std::make_unique<std::vector<mojom::LocalStorageUsageInfoPtr>>();
  auto* infos_ptr = infos.get();
  base::RepeatingClosure got_local_storage_usage = base::BarrierClosure(
      2, base::BindOnce(&InvokeLocalStorageUsageCallbackHelper,
                        std::move(callback), std::move(infos)));
  auto collect_callback = base::BindRepeating(
      CollectLocalStorageUsage, infos_ptr, std::move(got_local_storage_usage));

  // Grab metadata about pre-migration Local Storage data in the background
  // while we query |database_| below.
  if (directory_.empty()) {
    collect_callback.Run({});
  } else {
    base::PostTaskAndReplyWithResult(
        legacy_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetLegacyLocalStorageUsage, directory_),
        base::BindOnce(collect_callback));
  }

  if (!database_) {
    // If for whatever reason no leveldb database is available, no storage is
    // used, so return an array only containing the current areas.
    std::vector<mojom::LocalStorageUsageInfoPtr> result;
    base::Time now = base::Time::Now();
    for (const auto& it : areas_) {
      result.push_back(mojom::LocalStorageUsageInfo::New(it.first, 0, now));
    }
    collect_callback.Run(std::move(result));
  } else {
    database_->RunDatabaseTask(
        base::BindOnce([](const DomStorageDatabase& db) {
          std::vector<DomStorageDatabase::KeyValuePair> data;
          db.GetPrefixed(base::make_span(kMetaPrefix), &data);
          return data;
        }),
        base::BindOnce(&LocalStorageImpl::OnGotMetaData,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::BindOnce(collect_callback)));
  }
}

void LocalStorageImpl::OnGotMetaData(
    GetUsageCallback callback,
    std::vector<DomStorageDatabase::KeyValuePair> data) {
  std::vector<mojom::LocalStorageUsageInfoPtr> result;
  std::set<url::Origin> origins;
  for (const auto& row : data) {
    base::Optional<url::Origin> origin = ExtractOriginFromMetaDataKey(row.key);
    origins.insert(origin.value_or(url::Origin()));
    if (!origin) {
      // TODO(mek): Deal with database corruption.
      continue;
    }

    storage::LocalStorageOriginMetaData row_data;
    if (!row_data.ParseFromArray(row.value.data(), row.value.size())) {
      // TODO(mek): Deal with database corruption.
      continue;
    }

    result.push_back(mojom::LocalStorageUsageInfo::New(
        *origin, row_data.size_bytes(),
        base::Time::FromInternalValue(row_data.last_modified())));
  }
  // Add any origins for which StorageAreas exist, but which haven't
  // committed any data to disk yet.
  base::Time now = base::Time::Now();
  for (const auto& it : areas_) {
    if (origins.find(it.first) != origins.end())
      continue;
    // Skip any origins that definitely don't have any data.
    if (!it.second->storage_area()->has_pending_load_tasks() &&
        it.second->storage_area()->empty()) {
      continue;
    }
    result.push_back(mojom::LocalStorageUsageInfo::New(it.first, 0, now));
  }
  std::move(callback).Run(std::move(result));
}

void LocalStorageImpl::OnGotStorageUsageForShutdown(
    std::vector<mojom::LocalStorageUsageInfoPtr> usage) {
  std::vector<url::Origin> origins_to_delete;
  for (const auto& info : usage) {
    if (base::Contains(origins_to_purge_on_shutdown_, info->origin.GetURL()))
      origins_to_delete.push_back(info->origin);
  }

  if (!origins_to_delete.empty()) {
    DeleteOrigins(database_.get(), std::move(origins_to_delete),
                  base::BindOnce(&LocalStorageImpl::OnShutdownComplete,
                                 base::Unretained(this)));
  } else {
    OnShutdownComplete(leveldb::Status::OK());
  }
}

void LocalStorageImpl::OnShutdownComplete(leveldb::Status status) {
  delete this;
}

void LocalStorageImpl::GetStatistics(size_t* total_cache_size,
                                     size_t* unused_area_count) {
  *total_cache_size = 0;
  *unused_area_count = 0;
  for (const auto& it : areas_) {
    *total_cache_size += it.second->storage_area()->memory_used();
    if (!it.second->has_bindings())
      (*unused_area_count)++;
  }
}

void LocalStorageImpl::OnCommitResult(leveldb::Status status) {
  DCHECK(connection_state_ == CONNECTION_FINISHED ||
         connection_state_ == CONNECTION_SHUTDOWN)
      << connection_state_;
  if (status.ok()) {
    commit_error_count_ = 0;
    return;
  }

  commit_error_count_++;
  if (commit_error_count_ > kCommitErrorThreshold &&
      connection_state_ != CONNECTION_SHUTDOWN) {
    if (tried_to_recover_from_commit_errors_) {
      // We already tried to recover from a high commit error rate before, but
      // are still having problems: there isn't really anything left to try, so
      // just ignore errors.
      return;
    }
    tried_to_recover_from_commit_errors_ = true;

    // Deleting StorageAreas in here could cause more commits (and commit
    // errors), but those commits won't reach OnCommitResult because the area
    // will have been deleted before the commit finishes.
    DeleteAndRecreateDatabase(
        "LocalStorageContext.OpenResultAfterCommitErrors");
  }
}

void LocalStorageImpl::LogDatabaseOpenResult(OpenResult result) {
  if (result != OpenResult::SUCCESS) {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.OpenError", result,
                              OpenResult::MAX);
  }
  if (open_result_histogram_) {
    base::UmaHistogramEnumeration(open_result_histogram_, result,
                                  OpenResult::MAX);
  }
}

}  // namespace storage
