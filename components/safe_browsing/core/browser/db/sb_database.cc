// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_database.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/v4_store.h"
#include "components/safe_browsing/core/browser/db/v5_store.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/backup_util.h"
#endif

using base::TimeTicks;

// TODO(crbug.com/362791941): replace all |comments| with `comments` for v5.
// TODO(crbug.com/362791941): change all DCHECKs to CHECKs for v5 usages.
namespace safe_browsing {

namespace {

constexpr base::TimeDelta kUmaMinTime = base::Milliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::Hours(5);
constexpr int kUmaNumBuckets = 50;

// Returns the name of the metric by combining `prefix`, "V4" or "V5",
// and `suffix`.
std::string GetMetricName(std::string_view prefix,
                          std::string_view suffix,
                          bool allow_v5_logging = false) {
  // TODO(crbug.com/362791941): handle v5 and SB. Eventually `allow_v5_logging`
  // should be removed and always be true.
  return base::StrCat(
      {prefix,
       allow_v5_logging && base::FeatureList::IsEnabled(kLocalListsUseSBv5)
           ? "V5"
           : "V4",
       suffix});
}

// The factory that controls the creation of the SBDatabase object.
std::unique_ptr<SBDatabaseFactory>& GetDatabaseFactory() {
  static base::NoDestructor<std::unique_ptr<SBDatabaseFactory>> db_factory;
  return *db_factory;
}

// The factory that controls the creation of SBStore objects.
std::unique_ptr<SBStoreFactory>& GetStoreFactory() {
  static base::NoDestructor<std::unique_ptr<SBStoreFactory>> store_factory;
  return *store_factory;
}

// Verifies the checksums on a collection of stores.
// Returns the IDs of stores whose checksums failed to verify.
std::vector<ListIdentifier> VerifyChecksums(
    std::vector<std::pair<ListIdentifier, SBStore*>> stores) {
  std::vector<ListIdentifier> stores_to_reset;
  for (const auto& store_map_iter : stores) {
    if (!store_map_iter.second->VerifyChecksum()) {
      stores_to_reset.push_back(store_map_iter.first);
    }
  }
  return stores_to_reset;
}

void RecordCheckStoresTimeTaken(const std::string& metric_name,
                                base::TimeDelta delta) {
  base::UmaHistogramTimes(
      GetMetricName(
          "SafeBrowsing.",
          base::StrCat({"CheckUrl.TimeTaken.LocalLookup.", metric_name}),
          /*allow_v5_logging=*/true),
      delta);
  base::UmaHistogramTimes(
      base::StrCat(
          {"SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup.", metric_name}),
      delta);
}

// Returns hash prefixes matching the collection of stores.
DbLookupResult CheckStores(
    const std::vector<FullHashStr>& full_hashes,
    std::vector<std::pair<ListIdentifier, SBStore*>> stores,
    base::TimeTicks db_thread_post_time) {
  base::TimeTicks db_thread_start_time = base::TimeTicks::Now();
  RecordCheckStoresTimeTaken("DbThreadQueueDelay",
                             db_thread_start_time - db_thread_post_time);

  DbLookupResult lookup_result;
  lookup_result.db_thread_post_time = db_thread_post_time;
  lookup_result.db_thread_start_time = db_thread_start_time;

  for (const auto& store : stores) {
    for (const auto& full_hash : full_hashes) {
      HashPrefixStr hash_prefix =
          store.second->GetMatchingHashPrefix(full_hash);
      if (!hash_prefix.empty()) {
        lookup_result.results[full_hash].emplace_back(store.first, hash_prefix);
      }
    }
  }

  base::TimeTicks db_thread_end_time = base::TimeTicks::Now();
  RecordCheckStoresTimeTaken("StoreLookupDuration",
                             db_thread_end_time - db_thread_start_time);

  lookup_result.db_thread_end_time = db_thread_end_time;
  return lookup_result;
}

}  // namespace

std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter>
SBDatabaseFactory::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map) {
  // Not using MakeUnique since the constructor of SBDatabase is protected.
  return std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter>(
      new SBDatabase(db_task_runner, std::move(store_map)),
      base::OnTaskRunnerDeleter(db_task_runner));
}

// static
void SBDatabase::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfos& list_infos,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(base_path.IsAbsolute());
  DCHECK(!list_infos.empty());

  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SBDatabase::CreateOnTaskRunner, db_task_runner,
                                base_path, list_infos, callback_task_runner,
                                std::move(new_db_callback)));
}

// static
void SBDatabase::CreateOnTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfos& list_infos,
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  if (!base::CreateDirectory(base_path)) {
    return;
  }

#if BUILDFLAG(IS_APPLE)
  base::apple::SetBackupExclusion(base_path);
#endif

  std::unique_ptr<StoreMap> store_map = std::make_unique<StoreMap>();
  for (const auto& it : list_infos) {
    if (!it.fetch_updates()) {
      // This list doesn't need to be fetched or stored on disk.
      continue;
    }

    SBStorePtr store = CreateStore(db_task_runner, base_path, it);
    // Logs SafeBrowsing.V4Store.ReadyOnStartup
    base::UmaHistogramBoolean(
        GetMetricName("SafeBrowsing.", "Store.ReadyOnStartup",
                      /*allow_v5_logging=*/true),
        store->HasValidData());
    base::UmaHistogramBoolean("SafeBrowsing.SBStore.ReadyOnStartup",
                              store->HasValidData());
    store_map->insert({it.list_id(), std::move(store)});
  }

  if (!GetDatabaseFactory()) {
    GetDatabaseFactory() = std::make_unique<SBDatabaseFactory>();
  }

  std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter> sb_database =
      GetDatabaseFactory()->Create(db_task_runner, std::move(store_map));

  // Database is done loading, pass it to the new_db_callback on the caller's
  // thread. This would unblock resource loads.
  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(new_db_callback), std::move(sb_database)));
}

// static
void SBDatabase::RegisterDatabaseFactoryForTest(
    std::unique_ptr<SBDatabaseFactory> factory) {
  GetDatabaseFactory() = std::move(factory);
}

// static
SBStorePtr SBDatabase::CreateStore(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfo& list_info) {
  if (GetStoreFactory()) {
    // Used for tests.
    return GetStoreFactory()->CreateStore(db_task_runner, base_path, list_info);
  }
  return base::FeatureList::IsEnabled(kLocalListsUseSBv5)
             ? V5StoreFactory().CreateStore(db_task_runner, base_path,
                                            list_info)
             : V4StoreFactory().CreateStore(db_task_runner, base_path,
                                            list_info);
}

// static
void SBDatabase::RegisterStoreFactoryForTest(
    std::unique_ptr<SBStoreFactory> factory) {
  GetStoreFactory() = std::move(factory);
}

SBDatabase::SBDatabase(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : store_map_(std::move(store_map)),
      db_task_runner_(db_task_runner),
      pending_store_updates_(0) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());
  // This method executes on the DB sequence, whereas
  // |sequence_checker_| is meant to verify methods that should
  // execute on the UI sequence. Detach that sequence checker here; it
  // will be bound to the UI sequence in InitializeOnUIThread().
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void SBDatabase::InitializeOnUIThread() {
  // This invocation serves to bind |sequence_checker_| to the UI sequence
  // after its having been detached from the DB sequence in this object's
  // constructor.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SBDatabase::StopOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_on_io_.InvalidateWeakPtrs();
}

SBDatabase::~SBDatabase() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
}

void SBDatabase::ApplyUpdate(std::unique_ptr<SBUpdateResponseMap> update_map,
                             DatabaseUpdatedCallback db_updated_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_store_updates_);
  DCHECK(db_updated_callback_.is_null());

  db_updated_callback_ = db_updated_callback;

  // Post the SBStore update task on the DB sequence but get the callback on the
  // current sequence.
  const scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  for (auto& [list_id, response] : *update_map) {
    StoreMap::const_iterator iter = store_map_->find(list_id);
    CHECK(iter != store_map_->end())
        << "Got update for unexpected identifier: " << list_id;
    const SBStorePtr& old_store = iter->second;
    CHECK(response->v4_response || response->v5_response);
    std::string new_state = response->v4_response
                                ? response->v4_response->new_client_state()
                                : response->v5_response->version();
    if (old_store->GetStoreState() != new_state) {
      // A different state implies there are updates to process.
      pending_store_updates_++;
      UpdatedStoreReadyCallback store_ready_callback =
          base::BindOnce(&SBDatabase::UpdatedStoreReady,
                          weak_factory_on_io_.GetWeakPtr(), list_id);
      db_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&SBStore::ApplyUpdate,
                                    base::Unretained(old_store.get()),
                                    std::move(response), current_task_runner,
                                    std::move(store_ready_callback)));
    }
  }

  if (!pending_store_updates_) {
    current_task_runner->PostTask(FROM_HERE, db_updated_callback_);
    db_updated_callback_.Reset();
    RecordDatabaseUpdateLatency();
    last_update_ = base::Time::Now();
  }
}

void SBDatabase::UpdatedStoreReady(ListIdentifier identifier,
                                   SBStorePtr new_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_store_updates_);
  if (new_store) {
    if (auto it = store_map_->find(identifier); it != store_map_->end()) {
      it->second.swap(new_store);
    }
  }

  pending_store_updates_--;
  if (!pending_store_updates_) {
    db_updated_callback_.Run();
    RecordDatabaseUpdateLatency();
    last_update_ = base::Time::Now();
    db_updated_callback_.Reset();
  }
}

std::unique_ptr<StoreStateMap> SBDatabase::GetStoreStateMap() {
  std::unique_ptr<StoreStateMap> store_state_map =
      std::make_unique<StoreStateMap>();
  for (const auto& store_map_iter : *store_map_) {
    (*store_state_map)[store_map_iter.first] =
        store_map_iter.second->GetStoreState();
  }
  return store_state_map;
}

bool SBDatabase::AreAnyStoresAvailable(
    const StoresToCheck& stores_to_check) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_check) {
    if (IsStoreAvailable(identifier)) {
      return true;
    }
  }
  return false;
}

bool SBDatabase::AreAllStoresAvailable(
    const StoresToCheck& stores_to_check) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_check) {
    if (!IsStoreAvailable(identifier)) {
      return false;
    }
  }
  return true;
}

void SBDatabase::GetStoresMatchingFullHash(
    const std::vector<FullHashStr>& full_hashes,
    const StoresToCheck& stores_to_check,
    base::OnceCallback<void(DbLookupResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::pair<ListIdentifier, SBStore*>> stores;
  for (const ListIdentifier& identifier : stores_to_check) {
    if (!IsStoreAvailable(identifier)) {
      continue;
    }
    const auto& store_pair = store_map_->find(identifier);
    CHECK(store_pair != store_map_->end());
    stores.emplace_back(identifier, store_pair->second.get());
  }

  base::TimeTicks db_thread_post_time = base::TimeTicks::Now();
  auto check_stores = base::BindOnce(CheckStores, full_hashes,
                                     std::move(stores), db_thread_post_time);

  // The SBStores ptrs are guaranteed to be valid because their deletion would
  // be sequenced on the DB thread, after this posted task is serviced.
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(check_stores), std::move(callback));
}

void SBDatabase::ResetStores(
    const std::vector<ListIdentifier>& stores_to_reset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_reset) {
    store_map_->at(identifier)->Reset();
  }
}

void SBDatabase::VerifyChecksum(
    DatabaseReadyForUpdatesCallback db_ready_for_updates_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make a threadsafe copy of store_map_ w/raw pointers that we can hand to
  // the DB thread. The SBStores ptrs are guaranteed to be valid because their
  // deletion would be sequenced on the DB thread, after this posted task is
  // serviced.
  std::vector<std::pair<ListIdentifier, SBStore*>> stores;
  for (const auto& next_store : *store_map_) {
    stores.push_back(std::make_pair(next_store.first, next_store.second.get()));
  }

  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&VerifyChecksums, stores),
      base::BindOnce(&SBDatabase::OnChecksumVerified,
                     weak_factory_on_io_.GetWeakPtr(),
                     std::move(db_ready_for_updates_callback)));
}

void SBDatabase::OnChecksumVerified(
    DatabaseReadyForUpdatesCallback db_ready_for_updates_callback,
    const std::vector<ListIdentifier>& stores_to_reset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(db_ready_for_updates_callback).Run(stores_to_reset);
}

bool SBDatabase::IsStoreAvailable(const ListIdentifier& identifier) const {
  const auto& store_pair = store_map_->find(identifier);
  return (store_pair != store_map_->end()) &&
         store_pair->second->HasValidData();
}

int64_t SBDatabase::GetStoreSizeInBytes(
    const ListIdentifier& identifier) const {
  const auto& store_pair = store_map_->find(identifier);
  if (store_pair == store_map_->end()) {
    return 0;
  }
  return store_pair->second->file_size();
}

void SBDatabase::RecordFileSizeHistograms() {
  // Logs SafeBrowsing.V4Database.Size or SafeBrowsing.V5Database.Size
  std::string size_metric = GetMetricName("SafeBrowsing.", "Database.Size",
                                          /*allow_v5_logging=*/true);
  int64_t db_size = 0;
  for (const auto& store_map_iter : *store_map_) {
    const int64_t size =
        store_map_iter.second->RecordAndReturnFileSize(size_metric);
    db_size += size;
  }
  const int64_t db_size_kilobytes = static_cast<int64_t>(db_size / 1024);
  base::UmaHistogramCounts1M(size_metric, db_size_kilobytes);

  const int64_t db_size_megabytes =
      static_cast<int64_t>(db_size_kilobytes / 1024);
  // Logs SafeBrowsing.V4Database.SizeLinear or
  // SafeBrowsing.V5Database.SizeLinear
  base::UmaHistogramExactLinear(
      GetMetricName("SafeBrowsing.", "Database.SizeLinear",
                    /*allow_v5_logging=*/true),
      db_size_megabytes, /*value_max=*/50);
}

void SBDatabase::RecordDatabaseUpdateLatency() {
  if (!last_update_.is_null()) {
    // Logs SafeBrowsing.V4Database.UpdateLatency or
    // SafeBrowsing.V5Database.UpdateLatency
    base::UmaHistogramCustomTimes(
        GetMetricName("SafeBrowsing.", "Database.UpdateLatency",
                      /*allow_v5_logging=*/true),
        base::Time::Now() - last_update_, kUmaMinTime, kUmaMaxTime,
        kUmaNumBuckets);
  }
}

void SBDatabase::CollectDatabaseInfo(
    DatabaseManagerInfo::DatabaseInfo* database_info) {
  // Records the database size in bytes.
  int64_t db_size = 0;

  for (const auto& store_map_iter : *store_map_) {
    DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info =
        database_info->add_store_info();
    store_map_iter.second->CollectStoreInfo(store_info);
    db_size += store_info->file_size_bytes();
  }

  database_info->set_database_size_bytes(db_size);
}

}  // namespace safe_browsing
