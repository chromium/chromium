// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_database.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/backup_util.h"
#endif

using base::TimeTicks;

namespace safe_browsing {

namespace {

const char kV4DatabaseSizeMetric[] = "SafeBrowsing.V4Database.Size";
const char kV4DatabaseSizeLinearMetric[] = "SafeBrowsing.V4Database.SizeLinear";
const char kV4DatabaseUpdateLatency[] = "SafeBrowsing.V4Database.UpdateLatency";
constexpr base::TimeDelta kUmaMinTime = base::Milliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::Hours(5);
constexpr int kUmaNumBuckets = 50;

// The factory that controls the creation of the V4Database object.
base::LazyInstance<std::unique_ptr<V4DatabaseFactory>>::Leaky g_db_factory =
    LAZY_INSTANCE_INITIALIZER;

// The factory that controls the creation of V4Store objects.
base::LazyInstance<std::unique_ptr<V4StoreFactory>>::Leaky g_store_factory =
    LAZY_INSTANCE_INITIALIZER;

// Verifies the checksums on a collection of stores.
// Returns the IDs of stores whose checksums failed to verify.
std::vector<ListIdentifier> VerifyChecksums(
    std::vector<std::pair<ListIdentifier, V4Store*>> stores) {
  std::vector<ListIdentifier> stores_to_reset;
  for (const auto& store_map_iter : stores) {
    if (!store_map_iter.second->VerifyChecksum()) {
      stores_to_reset.push_back(store_map_iter.first);
    }
  }
  return stores_to_reset;
}

}  // namespace

std::unique_ptr<V4Database, base::OnTaskRunnerDeleter>
V4DatabaseFactory::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map) {
  // Not using MakeUnique since the constructor of V4Database is protected.
  return std::unique_ptr<V4Database, base::OnTaskRunnerDeleter>(
      new V4Database(db_task_runner, std::move(store_map)),
      base::OnTaskRunnerDeleter(db_task_runner));
}

// static
void V4Database::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfos& list_infos,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(base_path.IsAbsolute());
  DCHECK(!list_infos.empty());

  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&V4Database::CreateOnTaskRunner, db_task_runner,
                                base_path, list_infos, callback_task_runner,
                                std::move(new_db_callback)));
}

// static
void V4Database::CreateOnTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfos& list_infos,
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  if (!g_store_factory.Get())
    g_store_factory.Get() = std::make_unique<V4StoreFactory>();

  if (!base::CreateDirectory(base_path))
    NOTREACHED();

#if BUILDFLAG(IS_APPLE)
  base::mac::SetBackupExclusion(base_path);
#endif

  std::unique_ptr<StoreMap> store_map = std::make_unique<StoreMap>();
  for (const auto& it : list_infos) {
    if (!it.fetch_updates()) {
      // This list doesn't need to be fetched or stored on disk.
      continue;
    }

    const base::FilePath store_path = base_path.AppendASCII(it.filename());
    (*store_map)[it.list_id()] =
        g_store_factory.Get()->CreateV4Store(db_task_runner, store_path);
  }

  if (!g_db_factory.Get())
    g_db_factory.Get() = std::make_unique<V4DatabaseFactory>();

  std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database =
      g_db_factory.Get()->Create(db_task_runner, std::move(store_map));

  // Database is done loading, pass it to the new_db_callback on the caller's
  // thread. This would unblock resource loads.
  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(new_db_callback), std::move(v4_database)));
}

// static
void V4Database::RegisterDatabaseFactoryForTest(
    std::unique_ptr<V4DatabaseFactory> factory) {
  g_db_factory.Get() = std::move(factory);
}

// static
void V4Database::RegisterStoreFactoryForTest(
    std::unique_ptr<V4StoreFactory> factory) {
  g_store_factory.Get() = std::move(factory);
}

V4Database::V4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : store_map_(std::move(store_map)),
      db_task_runner_(db_task_runner),
      pending_store_updates_(0) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());
  // This method executes on the DB sequence, whereas |io_sequence_checker_|
  // is meant to verify methods that should execute on the IO sequence. Detach
  // that sequence checker here; it will be bound to the IO sequence in
  // InitializeOnIOSequence().
  DETACH_FROM_SEQUENCE(io_sequence_checker_);
}

void V4Database::InitializeOnIOSequence() {
  // This invocation serves to bind |io_sequence_checker_| to the IO sequence
  // after its having been detached from the DB sequence in this object's
  // constructor.
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
}

void V4Database::StopOnIO() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  weak_factory_on_io_.InvalidateWeakPtrs();
}

V4Database::~V4Database() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
}

void V4Database::ApplyUpdate(
    std::unique_ptr<ParsedServerResponse> parsed_server_response,
    DatabaseUpdatedCallback db_updated_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  DCHECK(!pending_store_updates_);
  DCHECK(db_updated_callback_.is_null());

  db_updated_callback_ = db_updated_callback;

  // Post the V4Store update task on the DB sequence but get the callback on the
  // current sequence.
  const scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  for (std::unique_ptr<ListUpdateResponse>& response :
       *parsed_server_response) {
    ListIdentifier identifier(*response);
    StoreMap::const_iterator iter = store_map_->find(identifier);
    if (iter != store_map_->end()) {
      const std::unique_ptr<V4Store>& old_store = iter->second;
      if (old_store->state() != response->new_client_state()) {
        // A different state implies there are updates to process.
        pending_store_updates_++;
        UpdatedStoreReadyCallback store_ready_callback =
            base::BindOnce(&V4Database::UpdatedStoreReady,
                           weak_factory_on_io_.GetWeakPtr(), identifier);
        db_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&V4Store::ApplyUpdate,
                                      base::Unretained(old_store.get()),
                                      std::move(response), current_task_runner,
                                      std::move(store_ready_callback)));
      }
    } else {
      NOTREACHED() << "Got update for unexpected identifier: " << identifier;
    }
  }

  if (!pending_store_updates_) {
    current_task_runner->PostTask(FROM_HERE, db_updated_callback_);
    db_updated_callback_.Reset();
    RecordDatabaseUpdateLatency();
    last_update_ = base::Time::Now();
  }
}

void V4Database::UpdatedStoreReady(ListIdentifier identifier,
                                   std::unique_ptr<V4Store> new_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  DCHECK(pending_store_updates_);
  if (new_store) {
    (*store_map_)[identifier].swap(new_store);
    // |new_store| now is the store that needs to be destroyed on task runner.
    V4Store::Destroy(std::move(new_store));
  }

  pending_store_updates_--;
  if (!pending_store_updates_) {
    db_updated_callback_.Run();
    RecordDatabaseUpdateLatency();
    last_update_ = base::Time::Now();
    db_updated_callback_.Reset();
  }
}

std::unique_ptr<StoreStateMap> V4Database::GetStoreStateMap() {
  std::unique_ptr<StoreStateMap> store_state_map =
      std::make_unique<StoreStateMap>();
  for (const auto& store_map_iter : *store_map_) {
    (*store_state_map)[store_map_iter.first] = store_map_iter.second->state();
  }
  return store_state_map;
}

bool V4Database::AreAnyStoresAvailable(
    const StoresToCheck& stores_to_check) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_check) {
    if (IsStoreAvailable(identifier))
      return true;
  }
  return false;
}

bool V4Database::AreAllStoresAvailable(
    const StoresToCheck& stores_to_check) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_check) {
    if (!IsStoreAvailable(identifier))
      return false;
  }
  return true;
}

void V4Database::GetStoresMatchingFullHash(
    const FullHashStr& full_hash,
    const StoresToCheck& stores_to_check,
    StoreAndHashPrefixes* matched_store_and_hash_prefixes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  matched_store_and_hash_prefixes->clear();
  for (const ListIdentifier& identifier : stores_to_check) {
    if (!IsStoreAvailable(identifier))
      continue;
    const auto& store_pair = store_map_->find(identifier);
    DCHECK(store_pair != store_map_->end());
    const std::unique_ptr<V4Store>& store = store_pair->second;
    HashPrefixStr hash_prefix = store->GetMatchingHashPrefix(full_hash);
    if (!hash_prefix.empty()) {
      matched_store_and_hash_prefixes->emplace_back(identifier, hash_prefix);
    }
  }
}

void V4Database::ResetStores(
    const std::vector<ListIdentifier>& stores_to_reset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  for (const ListIdentifier& identifier : stores_to_reset) {
    store_map_->at(identifier)->Reset();
  }
}

void V4Database::VerifyChecksum(
    DatabaseReadyForUpdatesCallback db_ready_for_updates_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);

  // Make a threadsafe copy of store_map_ w/raw pointers that we can hand to
  // the DB thread. The V4Stores ptrs are guaranteed to be valid because their
  // deletion would be sequenced on the DB thread, after this posted task is
  // serviced.
  std::vector<std::pair<ListIdentifier, V4Store*>> stores;
  for (const auto& next_store : *store_map_) {
    stores.push_back(std::make_pair(next_store.first, next_store.second.get()));
  }

  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&VerifyChecksums, stores),
      base::BindOnce(&V4Database::OnChecksumVerified,
                     weak_factory_on_io_.GetWeakPtr(),
                     std::move(db_ready_for_updates_callback)));
}

void V4Database::OnChecksumVerified(
    DatabaseReadyForUpdatesCallback db_ready_for_updates_callback,
    const std::vector<ListIdentifier>& stores_to_reset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  std::move(db_ready_for_updates_callback).Run(stores_to_reset);
}

bool V4Database::IsStoreAvailable(const ListIdentifier& identifier) const {
  const auto& store_pair = store_map_->find(identifier);
  return (store_pair != store_map_->end()) &&
         store_pair->second->HasValidData();
}

int64_t V4Database::GetStoreSizeInBytes(
    const ListIdentifier& identifier) const {
  const auto& store_pair = store_map_->find(identifier);
  if (store_pair == store_map_->end()) {
    return 0;
  }
  return store_pair->second->file_size();
}

void V4Database::RecordFileSizeHistograms() {
  int64_t db_size = 0;
  for (const auto& store_map_iter : *store_map_) {
    const int64_t size =
        store_map_iter.second->RecordAndReturnFileSize(kV4DatabaseSizeMetric);
    db_size += size;
  }
  const int64_t db_size_kilobytes = static_cast<int64_t>(db_size / 1024);
  UMA_HISTOGRAM_COUNTS_1M(kV4DatabaseSizeMetric, db_size_kilobytes);

  const int64_t db_size_megabytes =
      static_cast<int64_t>(db_size_kilobytes / 1024);
  UMA_HISTOGRAM_EXACT_LINEAR(kV4DatabaseSizeLinearMetric, db_size_megabytes,
                             50);
}

void V4Database::RecordDatabaseUpdateLatency() {
  if (!last_update_.is_null())
    UmaHistogramCustomTimes(kV4DatabaseUpdateLatency,
                            base::Time::Now() - last_update_, kUmaMinTime,
                            kUmaMaxTime, kUmaNumBuckets);
}

void V4Database::CollectDatabaseInfo(
    DatabaseManagerInfo::DatabaseInfo* database_info) {
  // Records the database size in bytes.
  int64_t db_size = 0;

  for (const auto& store_map_iter : *store_map_) {
    DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info =
        database_info->add_store_info();
    store_map_iter.second->CollectStoreInfo(store_info, kV4DatabaseSizeMetric);
    db_size += store_info->file_size_bytes();
  }

  database_info->set_database_size_bytes(db_size);
}

ListInfo::ListInfo(const bool fetch_updates,
                   const std::string& filename,
                   const ListIdentifier& list_id,
                   const SBThreatType sb_threat_type)
    : fetch_updates_(fetch_updates),
      filename_(filename),
      list_id_(list_id),
      sb_threat_type_(sb_threat_type) {
  DCHECK(!fetch_updates_ || !filename_.empty());
  DCHECK_NE(SB_THREAT_TYPE_SAFE, sb_threat_type_);
}

ListInfo::~ListInfo() {}

}  // namespace safe_browsing
