// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_store.h"
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/optimization_guide/core/memory_hint.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

namespace {

// Enforce that StoreEntryType enum is synced with the StoreEntryType proto
// (components/previews/content/proto/hint_cache.proto)
static_assert(
    proto::StoreEntryType_MAX ==
        static_cast<int>(OptimizationGuideStore::StoreEntryType::kMaxValue),
    "mismatched StoreEntryType enums");

// The amount of data to build up in memory before converting to a sorted on-
// disk file.
constexpr size_t kDatabaseWriteBufferSizeBytes = 128 * 1024;

// Delimiter that appears between the sections of a store entry key.
//  Examples:
//    "[StoreEntryType::kMetadata]_[MetadataType]"
//    "[StoreEntryType::kComponentHint]_[component_version]_[host]"
constexpr char kKeySectionDelimiter = '_';

// Enumerates the possible outcomes of loading metadata. Used in UMA histograms,
// so the order of enumerators should not be changed.
//
// Keep in sync with OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult
// in tools/metrics/histograms/enums.xml.
enum class OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult {
  kSuccess = 0,
  kLoadMetadataFailed = 1,
  kSchemaMetadataMissing = 2,
  kSchemaMetadataWrongVersion = 3,
  kComponentMetadataMissing = 4,
  kFetchedMetadataMissing = 5,
  kComponentAndFetchedMetadataMissing = 6,
  kMaxValue = kComponentAndFetchedMetadataMissing,
};

// Util class for recording the result of loading the metadata. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedLoadMetadataResultRecorder {
 public:
  ScopedLoadMetadataResultRecorder() = default;
  ~ScopedLoadMetadataResultRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", result_);
  }

  void set_result(
      OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult result) {
    result_ = result;
  }

 private:
  OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult result_ =
      OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::kSuccess;
};

void RecordStatusChange(OptimizationGuideStore::Status status) {
  UMA_HISTOGRAM_ENUMERATION("OptimizationGuide.HintCacheLevelDBStore.Status",
                            status);
}

// Returns true if |key_prefix| is a prefix of |key|.
bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

// Returns true if |key| is in |key_set|.
bool KeySetFilter(const base::flat_set<std::string>& key_set,
                  const std::string& key) {
  return key_set.find(key) != key_set.end();
}

}  // namespace

OptimizationGuideStore::OptimizationGuideStore(
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> store_task_runner,
    PrefService* pref_service)
    : store_task_runner_(store_task_runner), pref_service_(pref_service) {
  database_ = database_provider->GetDB<proto::StoreEntry>(
      leveldb_proto::ProtoDbType::HINT_CACHE_STORE, database_dir,
      store_task_runner_);

  RecordStatusChange(status_);

  // Clean up any file paths that were slated for deletion in previous sessions.
  CleanUpFilePaths();
}

OptimizationGuideStore::OptimizationGuideStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::StoreEntry>> database,
    scoped_refptr<base::SequencedTaskRunner> store_task_runner,
    PrefService* pref_service)
    : database_(std::move(database)),
      store_task_runner_(store_task_runner),
      pref_service_(pref_service) {
  RecordStatusChange(status_);

  // Clean up any file paths that were slated for deletion in previous sessions.
  CleanUpFilePaths();
}

OptimizationGuideStore::~OptimizationGuideStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideStore::Initialize(bool purge_existing_data,
                                        base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ >= Status::kInitializing) {
    // Already initializing - just run callback. There is an edge case where it
    // is still initializing and new callbacks will be run prematurely, but any
    // operations that need to deal with store require the background thread and
    // are guaranteed to happen after the first initialization has completed.
    std::move(callback).Run();
    return;
  }

  UpdateStatus(Status::kInitializing);

  // Asynchronously initialize the store and run the callback once
  // initialization completes. Initialization consists of the following steps:
  //   1. Initialize the database.
  //   2. If |purge_existing_data| is set to true, unconditionally purge
  //      database and skip to step 6.
  //   3. Otherwise, retrieve the metadata entries (e.g. Schema and Component).
  //   4. If schema is the wrong version, purge database and skip to step 6.
  //   5. Otherwise, load all hint entry keys.
  //   6. Run callback after purging database or retrieving hint entry keys.

  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  database_->Init(options,
                  base::BindOnce(&OptimizationGuideStore::OnDatabaseInitialized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 purge_existing_data, std::move(callback)));
}

std::unique_ptr<StoreUpdateData>
OptimizationGuideStore::MaybeCreateUpdateDataForComponentHints(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  if (!IsAvailable()) {
    return nullptr;
  }

  // Component updates are only permitted when the update version is newer than
  // the store's current one.
  if (component_version_.has_value() && version <= component_version_) {
    return nullptr;
  }

  // Create and return a StoreUpdateData object. This object has
  // hints from the component moved into it and organizes them in a format
  // usable by the store; the object will returned to the store in
  // StoreComponentHints().
  return StoreUpdateData::CreateComponentStoreUpdateData(version);
}

std::unique_ptr<StoreUpdateData>
OptimizationGuideStore::CreateUpdateDataForFetchedHints(
    base::Time update_time) const {
  // Create and returns a StoreUpdateData object. This object has has hints
  // from the GetHintsResponse moved into and organizes them in a format
  // usable by the store. The object will be store with UpdateFetchedData().
  return StoreUpdateData::CreateFetchedStoreUpdateData(update_time);
}

void OptimizationGuideStore::UpdateComponentHints(
    std::unique_ptr<StoreUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data);
  DCHECK(component_data->component_version());

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  // If this isn't a newer component version than the store's current one, then
  // the simply return. There's nothing to update.
  if (component_version_.has_value() &&
      component_data->component_version() <= component_version_) {
    std::move(callback).Run();
    return;
  }

  // Set the component version prior to requesting the update. This ensures that
  // a second update request for the same component version won't be allowed. In
  // the case where the update fails, the store will become unavailable, so it's
  // safe to treat component version in the update as the new one.
  SetComponentVersion(*component_data->component_version());

  // The current keys are about to be removed, so clear out the keys available
  // within the store. The keys will be populated after the component data
  // update completes.
  entry_keys_.reset();

  // Purge any component hints that are missing the new component version
  // prefix.
  EntryKeyPrefix retain_prefix =
      GetComponentHintEntryKeyPrefix(component_version_.value());
  EntryKeyPrefix filter_prefix = GetComponentHintEntryKeyPrefixWithoutVersion();

  // Add the new component data and purge any old component hints from the db.
  // After processing finishes, OnUpdateStore() is called, which loads
  // the updated hint entry keys from the database.
  database_->UpdateEntriesWithRemoveFilter(
      component_data->TakeUpdateEntries(),
      base::BindRepeating(
          [](const EntryKeyPrefix& retain_prefix,
             const EntryKeyPrefix& filter_prefix, const std::string& key) {
            return key.compare(0, retain_prefix.length(), retain_prefix) != 0 &&
                   key.compare(0, filter_prefix.length(), filter_prefix) == 0;
          },
          retain_prefix, filter_prefix),
      base::BindOnce(&OptimizationGuideStore::OnUpdateStore,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideStore::UpdateFetchedHints(
    std::unique_ptr<StoreUpdateData> fetched_hints_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(fetched_hints_data);
  DCHECK(fetched_hints_data->update_time());

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  fetched_update_time_ = *fetched_hints_data->update_time();

  entry_keys_.reset();

  // This will remove the fetched metadata entry and insert all the entries
  // currently in |leveldb_fetched_hints_data|.
  database_->UpdateEntriesWithRemoveFilter(
      fetched_hints_data->TakeUpdateEntries(),
      base::BindRepeating(&DatabasePrefixFilter,
                          GetMetadataTypeEntryKey(MetadataType::kFetched)),
      base::BindOnce(&OptimizationGuideStore::OnUpdateStore,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideStore::PurgeExpiredFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable())
    return;

  // Load all the fetched hints to check their expiry times.
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter,
                          GetFetchedHintEntryKeyPrefix()),
      base::BindOnce(&OptimizationGuideStore::OnLoadEntriesToPurgeExpired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OptimizationGuideStore::PurgeInactiveModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable())
    return;

  // Load all models to check their expiry times.
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter,
                          GetPredictionModelEntryKeyPrefix()),
      base::BindOnce(
          &OptimizationGuideStore::OnLoadModelsToBeUpdated,
          weak_ptr_factory_.GetWeakPtr(), std::make_unique<EntryVector>(),
          std::make_unique<leveldb_proto::KeyVector>(), base::DoNothing()));
}

void OptimizationGuideStore::OnLoadEntriesToPurgeExpired(
    bool success,
    std::unique_ptr<EntryMap> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success || !entries)
    return;

  EntryKeySet expired_keys_to_remove;
  int64_t now_since_epoch =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  for (const auto& entry : *entries) {
    if (entry.second.has_expiry_time_secs() &&
        entry.second.expiry_time_secs() <= now_since_epoch) {
      expired_keys_to_remove.insert(entry.first);
    }
  }

  entry_keys_.reset();

  database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<EntryVector>(),
      base::BindRepeating(&KeySetFilter, std::move(expired_keys_to_remove)),
      base::BindOnce(&OptimizationGuideStore::OnUpdateStore,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
}

void OptimizationGuideStore::RemoveFetchedHintsByKey(
    base::OnceClosure on_success,
    const base::flat_set<std::string>& hint_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EntryKeySet keys_to_remove;
  for (const std::string& key : hint_keys) {
    EntryKey store_key;
    if (FindEntryKeyForHostWithPrefix(key, &store_key,
                                      GetFetchedHintEntryKeyPrefix())) {
      keys_to_remove.insert(store_key);
    }
  }

  if (keys_to_remove.empty()) {
    std::move(on_success).Run();
    return;
  }

  for (const EntryKey& key : keys_to_remove) {
    entry_keys_->erase(key);
  }

  database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<EntryVector>(),
      base::BindRepeating(&KeySetFilter, keys_to_remove),
      base::BindOnce(&OptimizationGuideStore::OnFetchedEntriesRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_success),
                     keys_to_remove));
}

void OptimizationGuideStore::OnFetchedEntriesRemoved(
    base::OnceClosure on_success,
    const EntryKeySet& keys,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    // |on_success| is intentionally not run here since the operation did not
    // succeed.
    return;
  }

  std::move(on_success).Run();
}

bool OptimizationGuideStore::FindHintEntryKey(
    const std::string& host,
    EntryKey* out_hint_entry_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Search for kFetched hint entry keys first, fetched hints should be
  // fresher and preferred.
  if (FindEntryKeyForHostWithPrefix(host, out_hint_entry_key,
                                    GetFetchedHintEntryKeyPrefix())) {
    return true;
  }

  // Search for kComponent hint entry keys next.
  DCHECK(!component_version_.has_value() ||
         component_hint_entry_key_prefix_ ==
             GetComponentHintEntryKeyPrefix(component_version_.value()));
  if (FindEntryKeyForHostWithPrefix(host, out_hint_entry_key,
                                    component_hint_entry_key_prefix_)) {
    return true;
  }

  return false;
}

bool OptimizationGuideStore::FindEntryKeyForHostWithPrefix(
    const std::string& host,
    EntryKey* out_entry_key,
    const EntryKeyPrefix& entry_key_prefix) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(out_entry_key);

  // Look for entry key for host.
  *out_entry_key = entry_key_prefix + host;
  return entry_keys_ && entry_keys_->find(*out_entry_key) != entry_keys_->end();
}

void OptimizationGuideStore::LoadHint(const EntryKey& hint_entry_key,
                                      HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable()) {
    std::move(callback).Run(hint_entry_key, nullptr);
    return;
  }

  database_->GetEntry(hint_entry_key,
                      base::BindOnce(&OptimizationGuideStore::OnLoadHint,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     hint_entry_key, std::move(callback)));
}

base::Time OptimizationGuideStore::GetFetchedHintsUpdateTime() const {
  // If the store is not available, the metadata entries have not been loaded
  // so there are no fetched hints.
  if (!IsAvailable())
    return base::Time();
  return fetched_update_time_;
}

// static
const char OptimizationGuideStore::kStoreSchemaVersion[] = "1";

// static
OptimizationGuideStore::EntryKeyPrefix
OptimizationGuideStore::GetMetadataEntryKeyPrefix() {
  return base::NumberToString(static_cast<int>(
             OptimizationGuideStore::StoreEntryType::kMetadata)) +
         kKeySectionDelimiter;
}

// static
OptimizationGuideStore::EntryKey
OptimizationGuideStore::GetMetadataTypeEntryKey(MetadataType metadata_type) {
  return GetMetadataEntryKeyPrefix() +
         base::NumberToString(static_cast<int>(metadata_type));
}

// static
OptimizationGuideStore::EntryKeyPrefix
OptimizationGuideStore::GetComponentHintEntryKeyPrefixWithoutVersion() {
  return base::NumberToString(static_cast<int>(
             OptimizationGuideStore::StoreEntryType::kComponentHint)) +
         kKeySectionDelimiter;
}

// static
OptimizationGuideStore::EntryKeyPrefix
OptimizationGuideStore::GetComponentHintEntryKeyPrefix(
    const base::Version& component_version) {
  return GetComponentHintEntryKeyPrefixWithoutVersion() +
         component_version.GetString() + kKeySectionDelimiter;
}

// static
OptimizationGuideStore::EntryKeyPrefix
OptimizationGuideStore::GetFetchedHintEntryKeyPrefix() {
  return base::NumberToString(static_cast<int>(
             OptimizationGuideStore::StoreEntryType::kFetchedHint)) +
         kKeySectionDelimiter;
}

// static
OptimizationGuideStore::EntryKeyPrefix
OptimizationGuideStore::GetPredictionModelEntryKeyPrefix() {
  return base::NumberToString(static_cast<int>(
             OptimizationGuideStore::StoreEntryType::kPredictionModel)) +
         kKeySectionDelimiter;
}

// static
proto::OptimizationTarget
OptimizationGuideStore::GetOptimizationTargetFromPredictionModelEntryKey(
    const EntryKey& prediction_model_entry_key) {
  base::StringPiece optimization_target_number_string =
      base::TrimString(base::StringPiece(prediction_model_entry_key),
                       base::StringPiece(GetPredictionModelEntryKeyPrefix()),
                       base::TRIM_LEADING);
  int optimization_target_number;
  if (!base::StringToInt(optimization_target_number_string,
                         &optimization_target_number)) {
    return proto::OPTIMIZATION_TARGET_UNKNOWN;
  }
  if (!proto::OptimizationTarget_IsValid(optimization_target_number)) {
    return proto::OPTIMIZATION_TARGET_UNKNOWN;
  }
  return static_cast<proto::OptimizationTarget>(optimization_target_number);
}

void OptimizationGuideStore::UpdateStatus(Status new_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Status::kUninitialized can only transition to Status::kInitializing.
  DCHECK(status_ != Status::kUninitialized ||
         new_status == Status::kInitializing);
  // Status::kInitializing can only transition to Status::kAvailable or
  // Status::kFailed.
  DCHECK(status_ != Status::kInitializing || new_status == Status::kAvailable ||
         new_status == Status::kFailed);
  // Status::kAvailable can only transition to kStatus::Failed.
  DCHECK(status_ != Status::kAvailable || new_status == Status::kFailed);
  // The status can never transition from Status::kFailed.
  DCHECK(status_ != Status::kFailed || new_status == Status::kFailed);

  // If the status is not changing, simply return; the remaining logic handles
  // status changes.
  if (status_ == new_status) {
    return;
  }

  status_ = new_status;
  RecordStatusChange(status_);

  if (status_ == Status::kFailed) {
    database_->Destroy(
        base::BindOnce(&OptimizationGuideStore::OnDatabaseDestroyed,
                       weak_ptr_factory_.GetWeakPtr()));
    ClearComponentVersion();
    entry_keys_.reset();
  }
}

bool OptimizationGuideStore::IsAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return status_ == Status::kAvailable;
}

void OptimizationGuideStore::PurgeDatabase(base::OnceClosure callback) {
  // When purging the database, update the schema version to the current one.
  EntryKey schema_entry_key = GetMetadataTypeEntryKey(MetadataType::kSchema);
  proto::StoreEntry schema_entry;
  schema_entry.set_version(kStoreSchemaVersion);

  auto entries_to_save = std::make_unique<EntryVector>();
  entries_to_save->emplace_back(schema_entry_key, schema_entry);

  database_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save),
      base::BindRepeating(
          [](const std::string& schema_entry_key, const std::string& key) {
            return key.compare(0, schema_entry_key.length(),
                               schema_entry_key) != 0;
          },
          schema_entry_key),
      base::BindOnce(&OptimizationGuideStore::OnPurgeDatabase,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideStore::SetComponentVersion(
    const base::Version& component_version) {
  DCHECK(component_version.IsValid());
  component_version_ = component_version;
  component_hint_entry_key_prefix_ =
      GetComponentHintEntryKeyPrefix(component_version_.value());
}

void OptimizationGuideStore::ClearComponentVersion() {
  component_version_.reset();
  component_hint_entry_key_prefix_.clear();
}

void OptimizationGuideStore::ClearFetchedHintsFromDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(
      "OptimizationGuide.ClearFetchedHints.StoreAvailable", IsAvailable());
  if (!IsAvailable())
    return;

  auto entries_to_save = std::make_unique<EntryVector>();

  // TODO(mcrouse): Add histogram to record the number of hints being removed.
  entry_keys_.reset();

  // Removes all |kFetchedHint| store entries. OnUpdateStore will handle
  // updating status and re-filling entry_keys with the entries still in the
  // store.
  database_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save),  // this should be empty.
      base::BindRepeating(&DatabasePrefixFilter,
                          GetFetchedHintEntryKeyPrefix()),
      base::BindOnce(&OptimizationGuideStore::OnUpdateStore,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
}

void OptimizationGuideStore::MaybeLoadEntryKeys(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the database is unavailable  don't load the hint keys. Simply run the
  // callback.
  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  // Create a new KeySet object. This is populated by the store's keys as the
  // filter is run with each key on the DB's background thread. The filter
  // itself always returns false, ensuring that no entries are ever actually
  // loaded by the DB. Ownership of the KeySet is passed into the
  // LoadKeysAndEntriesCallback callback, guaranteeing that the KeySet has a
  // lifespan longer than the filter calls.
  std::unique_ptr<EntryKeySet> entry_keys(std::make_unique<EntryKeySet>());
  EntryKeySet* raw_entry_keys_pointer = entry_keys.get();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(
          [](EntryKeySet* entry_keys, const std::string& filter_prefix,
             const std::string& entry_key) {
            if (entry_key.compare(0, filter_prefix.length(), filter_prefix) !=
                0) {
              entry_keys->insert(entry_key);
            }
            return false;
          },
          raw_entry_keys_pointer, GetMetadataEntryKeyPrefix()),
      base::BindOnce(&OptimizationGuideStore::OnLoadEntryKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry_keys),
                     std::move(callback)));
}

size_t OptimizationGuideStore::GetEntryKeyCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entry_keys_ ? entry_keys_->size() : 0;
}

void OptimizationGuideStore::OnDatabaseInitialized(
    bool purge_existing_data,
    base::OnceClosure callback,
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If initialization is set to purge all existing data, then simply trigger
  // the purge and return. There's no need to load metadata entries that'll
  // immediately be purged.
  if (purge_existing_data) {
    PurgeDatabase(std::move(callback));
    return;
  }

  // Load all entries from the DB with the metadata key prefix.
  database_->LoadKeysAndEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(),
      GetMetadataEntryKeyPrefix(),
      base::BindOnce(&OptimizationGuideStore::OnLoadMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideStore::OnDatabaseDestroyed(bool /*success*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideStore::OnLoadMetadata(
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> metadata_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a scoped load metadata result recorder. It records the result when
  // its destructor is called.
  ScopedLoadMetadataResultRecorder result_recorder;

  if (!success || !metadata_entries) {
    result_recorder.set_result(
        OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
            kLoadMetadataFailed);

    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the schema version in the DB is not the current version, then purge
  // the database.
  auto schema_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kSchema));
  if (schema_entry == metadata_entries->end() ||
      !schema_entry->second.has_version() ||
      schema_entry->second.version() != kStoreSchemaVersion) {
    if (schema_entry == metadata_entries->end()) {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataMissing);
    } else {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataWrongVersion);
    }

    PurgeDatabase(std::move(callback));
    return;
  }

  // If the component metadata entry exists, then use it to set the component
  // version.
  bool component_metadata_missing = false;
  auto component_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kComponent));
  if (component_entry != metadata_entries->end()) {
    DCHECK(component_entry->second.has_version());
    SetComponentVersion(base::Version(component_entry->second.version()));
  } else {
    result_recorder.set_result(
        OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
            kComponentMetadataMissing);
    component_metadata_missing = true;
  }

  auto fetched_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kFetched));
  if (fetched_entry != metadata_entries->end()) {
    DCHECK(fetched_entry->second.has_update_time_secs());
    fetched_update_time_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(fetched_entry->second.update_time_secs()));
  } else {
    if (component_metadata_missing) {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kComponentAndFetchedMetadataMissing);
    } else {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kFetchedMetadataMissing);
    }
    fetched_update_time_ = base::Time();
  }

  UpdateStatus(Status::kAvailable);
  MaybeLoadEntryKeys(std::move(callback));
}

void OptimizationGuideStore::OnPurgeDatabase(base::OnceClosure callback,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The database can only be purged during initialization.
  DCHECK_EQ(status_, Status::kInitializing);

  UpdateStatus(success ? Status::kAvailable : Status::kFailed);
  std::move(callback).Run();
}

void OptimizationGuideStore::OnUpdateStore(base::OnceClosure callback,
                                           bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }
  MaybeLoadEntryKeys(std::move(callback));
}

void OptimizationGuideStore::OnLoadEntryKeys(
    std::unique_ptr<EntryKeySet> hint_entry_keys,
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> /*unused*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the store was set to unavailable after the request was started, then the
  // loaded keys should not be considered valid. Reset the keys so that they are
  // cleared.
  if (!IsAvailable())
    hint_entry_keys.reset();

  entry_keys_ = std::move(hint_entry_keys);

  std::move(callback).Run();
}

void OptimizationGuideStore::OnLoadHint(
    const std::string& entry_key,
    HintLoadedCallback callback,
    bool success,
    std::unique_ptr<proto::StoreEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If either the request failed, the store was set to unavailable after the
  // request was started, or there's an in-flight component data update, which
  // means the entry is about to be invalidated, then the loaded hint should
  // not be considered valid. Reset the entry so that no hint is returned to
  // the requester.
  if (!success || !IsAvailable()) {
    entry.reset();
  }

  if (!entry || !entry->has_hint()) {
    std::unique_ptr<MemoryHint> memory_hint;
    std::move(callback).Run(entry_key, std::move(memory_hint));
    return;
  }

  if (entry->has_expiry_time_secs() &&
      entry->expiry_time_secs() <=
          base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds()) {
    // An expired hint should be loaded rarely if the user is regularly fetching
    // and storing fresh hints. Expired fetched hints are removed each time
    // fresh hints are fetched and placed into the store. In the future, the
    // expired hints could be asynchronously removed if necessary.
    // An empty hint is returned instead of the expired one.
    LOCAL_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.HintCacheStore.OnLoadHint.FetchedHintExpired", true);
    std::unique_ptr<MemoryHint> memory_hint(nullptr);
    std::move(callback).Run(entry_key, std::move(memory_hint));
    return;
  }

  StoreEntryType store_entry_type =
      static_cast<StoreEntryType>(entry->entry_type());
  UMA_HISTOGRAM_ENUMERATION("OptimizationGuide.HintCache.HintType.Loaded",
                            store_entry_type);

  absl::optional<base::Time> expiry_time;
  if (entry->has_expiry_time_secs()) {
    expiry_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(entry->expiry_time_secs()));
    LOCAL_HISTOGRAM_CUSTOM_TIMES(
        "OptimizationGuide.HintCache.FetchedHint.TimeToExpiration",
        *expiry_time - base::Time::Now(), base::Hours(1), base::Days(15), 50);
  }
  std::move(callback).Run(
      entry_key,
      std::make_unique<MemoryHint>(
          expiry_time, std::unique_ptr<proto::Hint>(entry->release_hint())));
}

std::unique_ptr<StoreUpdateData>
OptimizationGuideStore::CreateUpdateDataForPredictionModels(
    base::Time expiry_time) const {
  // Create and returns a StoreUpdateData object. This object has prediction
  // models from the GetModelsResponse moved into and organizes them in a format
  // usable by the store. The object will be stored with
  // UpdatePredictionModels().
  return StoreUpdateData::CreatePredictionModelStoreUpdateData(expiry_time);
}

void OptimizationGuideStore::UpdatePredictionModels(
    std::unique_ptr<StoreUpdateData> prediction_models_update_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prediction_models_update_data);

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<EntryVector> entry_vector =
      prediction_models_update_data->TakeUpdateEntries();

  EntryKeySet keys_to_update;
  for (const auto& entry : *entry_vector)
    keys_to_update.insert(entry.first);

  // Load the models that are to be updated and delete the old model file, if
  // applicable.
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&KeySetFilter, std::move(keys_to_update)),
      base::BindOnce(&OptimizationGuideStore::OnLoadModelsToBeUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry_vector),
                     std::make_unique<leveldb_proto::KeyVector>(),
                     std::move(callback)));
}

// This method is called during browser startup when we need to check if any
// models have expired. When this occurs, |update_vector| and |remove_vector|
// will both be empty. Otherwise, this method may also be called whenever a
// model is updated or its deletion is requested in which case one of or both
// |update_vector| and |remove_vector| will have entries.
void OptimizationGuideStore::OnLoadModelsToBeUpdated(
    std::unique_ptr<EntryVector> update_vector,
    std::unique_ptr<leveldb_proto::KeyVector> remove_vector,
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> entries) {
  if (!success || !entries) {
    std::move(callback).Run();
    return;
  }

  int64_t now_since_epoch =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();
  bool had_entries_to_update_or_remove =
      !update_vector->empty() || !remove_vector->empty();
  for (const auto& entry : *entries) {
    absl::optional<std::string> delete_download_file;
    if (had_entries_to_update_or_remove &&
        entry.second.has_prediction_model() &&
        !entry.second.prediction_model().model().download_url().empty()) {
      delete_download_file =
          entry.second.prediction_model().model().download_url();
    }

    // Only check expiry if we weren't explicitly passed in entries to update or
    // remove.
    if (!had_entries_to_update_or_remove) {
      if (entry.second.keep_beyond_valid_duration()) {
        continue;
      }
      if (entry.second.has_expiry_time_secs()) {
        if (entry.second.expiry_time_secs() <= now_since_epoch) {
          // Update the entry to remove the model.
          if (entry.second.has_prediction_model() &&
              !entry.second.prediction_model().model().download_url().empty()) {
            delete_download_file =
                entry.second.prediction_model().model().download_url();
          }

          remove_vector->push_back(entry.first);
          proto::OptimizationTarget optimization_target =
              GetOptimizationTargetFromPredictionModelEntryKey(entry.first);
          base::UmaHistogramBoolean(
              "OptimizationGuide.PredictionModelExpired." +
                  GetStringNameForOptimizationTarget(optimization_target),
              true);
          base::UmaHistogramSparse(
              "OptimizationGuide.PredictionModelExpiredVersion." +
                  GetStringNameForOptimizationTarget(optimization_target),
              entry.second.prediction_model().model_info().version());
        }
      } else {
        // If we were checking expiry and the entry did not have an expiration
        // time associated with it, add one with a default TTL.
        update_vector->push_back(entry);
        update_vector->back().second.set_expiry_time_secs(
            now_since_epoch +
            features::StoredModelsValidDuration().InSeconds());
      }
    }

    // Delete files (the model itself and any additional files) that are
    // provided by the model in its directory.
    if (delete_download_file) {
      // |StringToFilePath| only returns nullopt when
      // |delete_download_file| is empty.
      base::FilePath model_file_path =
          StringToFilePath(*delete_download_file).value();
      base::FilePath path_to_delete;

      // Backwards compatibility: Once upon a time (<M93), model files were
      // stored as
      // `$CHROME_DATA/OptGuideModels/${MODELTARGET}_${MODELVERSION}.tfl` but
      // were later moved to
      // `$CHROME_DATA/OptGuideModels/${MODELTARGET}_${MODELVERSION}/model.tfl`
      // to support additional files to be packaged alongside the model. Since
      // the current code needs to recursively delete the whole directory, we'd
      // normally just take the directory name of the model file. However, doing
      // this on a freshly updated browser to newer code would cause the entire
      // OptGuide directory to be blown away, causing collateral damage to other
      // downloaded models. This is detected by checking whether the base name
      // of the model file is the old or new version, and acting accordingly.
      if (model_file_path.BaseName() == GetBaseFileNameForModels()) {
        path_to_delete = model_file_path.DirName();
      } else {
        path_to_delete = model_file_path;
      }

      if (pref_service_) {
        ScopedDictPrefUpdate pref_update(pref_service_,
                                         prefs::kStoreFilePathsToDelete);
        pref_update->Set(FilePathToString(path_to_delete), true);
      } else {
        // |pref_service_| should always be provided by owning classes; however,
        // if it is not, just default back to deleting it here. This has the
        // potential to be racy though.

        // Note that the delete function doesn't care whether the target is a
        // directory or file. But in the case of a directory, it is recursively
        // deleted.
        store_task_runner_->PostTask(
            FROM_HERE, base::GetDeletePathRecursivelyCallback(path_to_delete));
      }
    }
  }

  database_->UpdateEntries(
      std::move(update_vector), std::move(remove_vector),
      base::BindOnce(&OptimizationGuideStore::OnUpdateStore,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool OptimizationGuideStore::FindPredictionModelEntryKey(
    proto::OptimizationTarget optimization_target,
    OptimizationGuideStore::EntryKey* out_prediction_model_entry_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!entry_keys_)
    return false;
  *out_prediction_model_entry_key =
      GetPredictionModelEntryKeyPrefix() +
      base::NumberToString(static_cast<int>(optimization_target));
  return entry_keys_->find(*out_prediction_model_entry_key) !=
         entry_keys_->end();
}

bool OptimizationGuideStore::RemovePredictionModelFromEntryKey(
    const EntryKey& entry_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable() || !entry_keys_ ||
      entry_keys_->find(entry_key) == entry_keys_->end()) {
    return false;
  }

  auto key_to_remove = std::make_unique<leveldb_proto::KeyVector>();
  key_to_remove->push_back(entry_key);
  EntryKeySet key_set;
  key_set.insert(entry_key);
  // Load the model that is to be removed and delete the old model file, if
  // applicable.
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&KeySetFilter, std::move(key_set)),
      base::BindOnce(&OptimizationGuideStore::OnLoadModelsToBeUpdated,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<EntryVector>(), std::move(key_to_remove),
                     base::DoNothing()));

  return true;
}

void OptimizationGuideStore::LoadPredictionModel(
    const EntryKey& prediction_model_entry_key,
    PredictionModelLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable()) {
    std::move(callback).Run(nullptr);
    return;
  }

  database_->GetEntry(
      prediction_model_entry_key,
      base::BindOnce(&OptimizationGuideStore::OnLoadPredictionModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideStore::OnLoadPredictionModel(
    PredictionModelLoadedCallback callback,
    bool success,
    std::unique_ptr<proto::StoreEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If either the request failed or the store was set to unavailable after the
  // request was started, then the loaded model should not be considered valid.
  // Reset the entry so that nothing is returned to
  // the requester.
  if (!success || !IsAvailable())
    entry.reset();

  if (!entry || !entry->has_prediction_model()) {
    std::unique_ptr<proto::PredictionModel> loaded_prediction_model(nullptr);
    std::move(callback).Run(std::move(loaded_prediction_model));
    return;
  }
  // Also ensure that nothing is returned if the model is expired.
  int64_t now_since_epoch =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();
  if (!entry->keep_beyond_valid_duration() &&
      entry->expiry_time_secs() <= now_since_epoch) {
    // Leave the actual model deletions to |PurgeInactiveModels| and return
    // early.
    std::unique_ptr<proto::PredictionModel> loaded_prediction_model(nullptr);
    std::move(callback).Run(std::move(loaded_prediction_model));
    return;
  }

  std::unique_ptr<proto::PredictionModel> loaded_prediction_model(
      entry->release_prediction_model());
  if (loaded_prediction_model->model().download_url().empty()) {
    std::move(callback).Run(std::move(loaded_prediction_model));
    return;
  }

  // Make sure the model file path and all additional files still exist before
  // we send it back to the load initiator.
  std::vector<base::FilePath> file_paths_to_check;
  absl::optional<base::FilePath> model_file_path =
      StringToFilePath(loaded_prediction_model->model().download_url());
  if (model_file_path) {
    file_paths_to_check.emplace_back(*model_file_path);
  }
  for (const proto::AdditionalModelFile& additional_file :
       loaded_prediction_model->model_info().additional_files()) {
    absl::optional<base::FilePath> additional_file_path =
        StringToFilePath(additional_file.file_path());
    if (additional_file_path) {
      file_paths_to_check.emplace_back(*additional_file_path);
    }
  }

  store_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CheckAllPathsExist, file_paths_to_check),
      base::BindOnce(&OptimizationGuideStore::OnModelFilePathVerified,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(loaded_prediction_model), std::move(callback)));
}

void OptimizationGuideStore::OnModelFilePathVerified(
    std::unique_ptr<proto::PredictionModel> loaded_model,
    PredictionModelLoadedCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelFilesVerified." +
          GetStringNameForOptimizationTarget(
              loaded_model->model_info().optimization_target()),
      success);

  if (success) {
    std::move(callback).Run(std::move(loaded_model));
    return;
  }

  // If the model no longer exists, remove the prediction model from the store.
  DCHECK(loaded_model);
  OptimizationGuideStore::EntryKey model_entry_key;
  if (FindPredictionModelEntryKey(
          loaded_model->model_info().optimization_target(), &model_entry_key)) {
    RemovePredictionModelFromEntryKey(model_entry_key);
  }
  std::move(callback).Run(nullptr);
}

void OptimizationGuideStore::CleanUpFilePaths() {
  if (!pref_service_) {
    return;
  }

  ScopedDictPrefUpdate file_paths_to_delete_pref(
      pref_service_, prefs::kStoreFilePathsToDelete);
  for (const auto entry : *file_paths_to_delete_pref) {
    absl::optional<base::FilePath> path_to_delete =
        StringToFilePath(entry.first);
    if (!path_to_delete) {
      // This is probably not a real file path so delete it from the pref, so we
      // don't go through this sequence again.
      OnFilePathDeleted(entry.first, /*success=*/true);
      continue;
    }
    // Note that the delete function doesn't care whether the target is a
    // directory or file. But in the case of a directory, it is recursively
    // deleted.
    //
    // We post it to the generic thread pool since we don't really care what
    // thread deletes it at this point as long as it gets deleted.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&base::DeletePathRecursively, *path_to_delete),
        base::BindOnce(&OptimizationGuideStore::OnFilePathDeleted,
                       weak_ptr_factory_.GetWeakPtr(), entry.first));
  }
}

void OptimizationGuideStore::OnFilePathDeleted(
    const std::string& file_path_to_clean_up,
    bool success) {
  if (!success) {
    // Try to delete again later.
    return;
  }

  // If we get here, we should have a pref service.
  DCHECK(pref_service_);
  ScopedDictPrefUpdate update(pref_service_, prefs::kStoreFilePathsToDelete);
  update->Remove(file_path_to_clean_up);
}

}  // namespace optimization_guide
