// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_store.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_util.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {
namespace {

using sync_pb::SessionSpecifics;
using syncer::MetadataChangeList;
using syncer::ModelTypeStore;

std::string TabNodeIdToClientTag(const std::string& session_tag,
                                 int tab_node_id) {
  CHECK_GT(tab_node_id, TabNodePool::kInvalidTabNodeID);
  return base::StringPrintf("%s %d", session_tag.c_str(), tab_node_id);
}

std::string EncodeStorageKey(const std::string& session_tag, int tab_node_id) {
  base::Pickle pickle;
  pickle.WriteString(session_tag);
  pickle.WriteInt(tab_node_id);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

bool DecodeStorageKey(const std::string& storage_key,
                      std::string* session_tag,
                      int* tab_node_id) {
  base::Pickle pickle(storage_key.c_str(), storage_key.size());
  base::PickleIterator iter(pickle);
  if (!iter.ReadString(session_tag)) {
    return false;
  }
  if (!iter.ReadInt(tab_node_id)) {
    return false;
  }
  return true;
}

std::unique_ptr<syncer::EntityData> MoveToEntityData(
    const std::string& client_name,
    SessionSpecifics* specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->non_unique_name = client_name;
  if (specifics->has_header()) {
    entity_data->non_unique_name += " (header)";
  } else if (specifics->has_tab()) {
    entity_data->non_unique_name +=
        base::StringPrintf(" (tab node %d)", specifics->tab_node_id());
  }
  entity_data->specifics.mutable_session()->Swap(specifics);
  return entity_data;
}

std::string GetSessionTagWithPrefs(const std::string& cache_guid,
                                   SessionSyncPrefs* sync_prefs) {
  DCHECK(sync_prefs);
  const std::string persisted_guid = sync_prefs->GetSyncSessionsGUID();
  if (!persisted_guid.empty()) {
    DVLOG(1) << "Restoring persisted session sync guid: " << persisted_guid;
    return persisted_guid;
  }

  const std::string new_guid =
      base::StringPrintf("session_sync%s", cache_guid.c_str());
  DVLOG(1) << "Creating session sync guid: " << new_guid;
  sync_prefs->SetSyncSessionsGUID(new_guid);
  return new_guid;
}

void ForwardError(syncer::OnceModelErrorHandler error_handler,
                  const base::Optional<syncer::ModelError>& error) {
  if (error) {
    std::move(error_handler).Run(*error);
  }
}

class FactoryImpl : public base::SupportsWeakPtr<FactoryImpl> {
 public:
  // Raw pointers must not be null and must outlive this object.
  FactoryImpl(SyncSessionsClient* sessions_client,
              const SessionStore::RestoredForeignTabCallback&
                  restored_foreign_tab_callback)
      : sessions_client_(sessions_client),
        restored_foreign_tab_callback_(restored_foreign_tab_callback) {
    DCHECK(sessions_client);
  }

  ~FactoryImpl() {}

  void Create(const syncer::DeviceInfo& device_info,
              SessionStore::FactoryCompletionCallback callback) {
    const std::string cache_guid = device_info.guid();
    DCHECK(!cache_guid.empty());

    SessionStore::SessionInfo session_info;
    session_info.client_name = device_info.client_name();
    session_info.device_type = device_info.device_type();
    session_info.session_tag = GetSessionTagWithPrefs(
        cache_guid, sessions_client_->GetSessionSyncPrefs());

    DVLOG(1) << "Initiating creation of session store";

    sessions_client_->GetStoreFactory().Run(
        syncer::SESSIONS,
        base::BindOnce(&FactoryImpl::OnStoreCreated, base::AsWeakPtr(this),
                       session_info, std::move(callback)));
  }

 private:
  void OnStoreCreated(const SessionStore::SessionInfo& session_info,
                      SessionStore::FactoryCompletionCallback callback,
                      const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store) {
    if (error) {
      std::move(callback).Run(error, /*store=*/nullptr,
                              /*metadata_batch=*/nullptr);
      return;
    }

    DCHECK(store);
    ModelTypeStore* store_copy = store.get();
    store_copy->ReadAllData(
        base::BindOnce(&FactoryImpl::OnReadAllData, base::AsWeakPtr(this),
                       session_info, std::move(callback), std::move(store)));
  }

  void OnReadAllData(const SessionStore::SessionInfo& session_info,
                     SessionStore::FactoryCompletionCallback callback,
                     std::unique_ptr<ModelTypeStore> store,
                     const base::Optional<syncer::ModelError>& error,
                     std::unique_ptr<ModelTypeStore::RecordList> record_list) {
    if (error) {
      std::move(callback).Run(error, /*store=*/nullptr,
                              /*metadata_batch=*/nullptr);
      return;
    }

    store->ReadAllMetadata(base::BindOnce(
        &FactoryImpl::OnReadAllMetadata, base::AsWeakPtr(this), session_info,
        std::move(callback), std::move(store), std::move(record_list)));
  }

  void OnReadAllMetadata(
      const SessionStore::SessionInfo& session_info,
      SessionStore::FactoryCompletionCallback callback,
      std::unique_ptr<ModelTypeStore> store,
      std::unique_ptr<ModelTypeStore::RecordList> record_list,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
    if (error) {
      std::move(callback).Run(error, /*store=*/nullptr,
                              /*metadata_batch=*/nullptr);
      return;
    }

    std::map<std::string, sync_pb::SessionSpecifics> initial_data;
    for (ModelTypeStore::Record& record : *record_list) {
      const std::string& storage_key = record.id;
      SessionSpecifics specifics;
      if (storage_key.empty() ||
          !specifics.ParseFromString(std::move(record.value))) {
        DVLOG(1) << "Ignoring corrupt database entry with key: " << storage_key;
        continue;
      }
      initial_data[storage_key].Swap(&specifics);
    }

    auto session_store = std::make_unique<SessionStore>(
        sessions_client_, session_info, std::move(store),
        std::move(initial_data), metadata_batch->GetAllMetadata(),
        restored_foreign_tab_callback_);

    std::move(callback).Run(/*error=*/base::nullopt, std::move(session_store),
                            std::move(metadata_batch));
  }

  SyncSessionsClient* const sessions_client_;
  const SessionStore::RestoredForeignTabCallback restored_foreign_tab_callback_;
};

}  // namespace

// static
SessionStore::Factory SessionStore::CreateFactory(
    SyncSessionsClient* sessions_client,
    const RestoredForeignTabCallback& restored_foreign_tab_callback) {
  auto factory = std::make_unique<FactoryImpl>(sessions_client,
                                               restored_foreign_tab_callback);
  return base::BindRepeating(&FactoryImpl::Create, std::move(factory));
}

SessionStore::WriteBatch::WriteBatch(
    std::unique_ptr<ModelTypeStore::WriteBatch> batch,
    CommitCallback commit_cb,
    syncer::OnceModelErrorHandler error_handler,
    SyncedSessionTracker* session_tracker)
    : batch_(std::move(batch)),
      commit_cb_(std::move(commit_cb)),
      error_handler_(std::move(error_handler)),
      session_tracker_(session_tracker) {
  DCHECK(batch_);
  DCHECK(commit_cb_);
  DCHECK(error_handler_);
  DCHECK(session_tracker_);
}

SessionStore::WriteBatch::~WriteBatch() {
  DCHECK(!batch_) << "Destructed without prior commit";
}

std::string SessionStore::WriteBatch::PutAndUpdateTracker(
    const sync_pb::SessionSpecifics& specifics,
    base::Time modification_time) {
  UpdateTrackerWithSpecifics(specifics, modification_time, session_tracker_);
  return PutWithoutUpdatingTracker(specifics);
}

void SessionStore::WriteBatch::DeleteForeignEntityAndUpdateTracker(
    const std::string& storage_key) {
  std::string session_tag;
  int tab_node_id;
  bool success = DecodeStorageKey(storage_key, &session_tag, &tab_node_id);
  DCHECK(success);
  DCHECK_NE(session_tag, session_tracker_->GetLocalSessionTag());

  if (tab_node_id == TabNodePool::kInvalidTabNodeID) {
    // Removal of a foreign header entity.
    // TODO(mastiz): This cascades with the removal of tabs too. Should we
    // reflect this as batch_->DeleteData()? The old code didn't, presumably
    // because we expect the rest of the removals to follow.
    session_tracker_->DeleteForeignSession(session_tag);
  } else {
    // Removal of a foreign tab entity.
    session_tracker_->DeleteForeignTab(session_tag, tab_node_id);
  }

  batch_->DeleteData(storage_key);
}

std::string SessionStore::WriteBatch::PutWithoutUpdatingTracker(
    const sync_pb::SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));

  const std::string storage_key = GetStorageKey(specifics);
  batch_->WriteData(storage_key, specifics.SerializeAsString());
  return storage_key;
}

std::string SessionStore::WriteBatch::DeleteLocalTabWithoutUpdatingTracker(
    int tab_node_id) {
  std::string storage_key =
      EncodeStorageKey(session_tracker_->GetLocalSessionTag(), tab_node_id);
  batch_->DeleteData(storage_key);
  return storage_key;
}

MetadataChangeList* SessionStore::WriteBatch::GetMetadataChangeList() {
  return batch_->GetMetadataChangeList();
}

// static
void SessionStore::WriteBatch::Commit(std::unique_ptr<WriteBatch> batch) {
  DCHECK(batch);
  std::move(batch->commit_cb_)
      .Run(std::move(batch->batch_),
           base::BindOnce(&ForwardError, std::move(batch->error_handler_)));
}

// static
bool SessionStore::AreValidSpecifics(const SessionSpecifics& specifics) {
  if (specifics.session_tag().empty()) {
    return false;
  }
  if (specifics.has_tab()) {
    return specifics.tab_node_id() >= 0 && specifics.tab().tab_id() > 0;
  }
  if (specifics.has_header()) {
    // Verify that tab IDs appear only once within a header. Intended to prevent
    // http://crbug.com/360822.
    std::set<int> session_tab_ids;
    for (const sync_pb::SessionWindow& window : specifics.header().window()) {
      for (int tab_id : window.tab()) {
        bool success = session_tab_ids.insert(tab_id).second;
        if (!success) {
          return false;
        }
      }
    }
    return !specifics.has_tab() &&
           specifics.tab_node_id() == TabNodePool::kInvalidTabNodeID;
  }
  return false;
}

// static
std::string SessionStore::GetClientTag(const SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));

  if (specifics.has_header()) {
    return specifics.session_tag();
  }

  DCHECK(specifics.has_tab());
  return TabNodeIdToClientTag(specifics.session_tag(), specifics.tab_node_id());
}

// static
std::string SessionStore::GetStorageKey(const SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));
  return EncodeStorageKey(specifics.session_tag(), specifics.tab_node_id());
}

// static
std::string SessionStore::GetHeaderStorageKey(const std::string& session_tag) {
  return EncodeStorageKey(session_tag, TabNodePool::kInvalidTabNodeID);
}

// static
std::string SessionStore::GetTabStorageKey(const std::string& session_tag,
                                           int tab_node_id) {
  DCHECK_GE(tab_node_id, 0);
  return EncodeStorageKey(session_tag, tab_node_id);
}

bool SessionStore::StorageKeyMatchesLocalSession(
    const std::string& storage_key) const {
  std::string session_tag;
  int tab_node_id;
  bool success = DecodeStorageKey(storage_key, &session_tag, &tab_node_id);
  DCHECK(success);
  return session_tag == local_session_info_.session_tag;
}

// static
std::string SessionStore::GetTabClientTagForTest(const std::string& session_tag,
                                                 int tab_node_id) {
  return TabNodeIdToClientTag(session_tag, tab_node_id);
}

SessionStore::SessionStore(
    SyncSessionsClient* sessions_client,
    const SessionInfo& local_session_info,
    std::unique_ptr<ModelTypeStore> store,
    std::map<std::string, sync_pb::SessionSpecifics> initial_data,
    const syncer::EntityMetadataMap& initial_metadata,
    const RestoredForeignTabCallback& restored_foreign_tab_callback)
    : store_(std::move(store)),
      local_session_info_(local_session_info),
      session_tracker_(sessions_client),
      weak_ptr_factory_(this) {
  DCHECK(store_);

  DVLOG(1) << "Constructed session store with " << initial_data.size()
           << " restored entities and " << initial_metadata.size()
           << " metadata entries.";

  session_tracker_.InitLocalSession(local_session_info.session_tag,
                                    local_session_info.client_name,
                                    local_session_info.device_type);

  bool found_local_header = false;

  for (auto& storage_key_and_specifics : initial_data) {
    const std::string& storage_key = storage_key_and_specifics.first;
    SessionSpecifics& specifics = storage_key_and_specifics.second;

    // The store should not contain invalid data, but as a precaution we filter
    // out anyway in case the persisted data is corrupted.
    if (!AreValidSpecifics(specifics)) {
      continue;
    }

    // Metadata should be available if data is available. If not, it means
    // the local store is corrupt, because we delete all data and metadata
    // at the same time (e.g. sync is disabled).
    auto metadata_it = initial_metadata.find(storage_key);
    if (metadata_it == initial_metadata.end()) {
      continue;
    }

    const base::Time mtime =
        syncer::ProtoTimeToTime(metadata_it->second.modification_time());

    if (specifics.session_tag() != local_session_info.session_tag) {
      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);

      // Notify listeners. In practice, this has the goal to load the URLs and
      // visit times into the in-memory favicon cache.
      if (specifics.has_tab()) {
        restored_foreign_tab_callback.Run(specifics.tab(), mtime);
      }
    } else if (specifics.has_header()) {
      // This is previously stored local header information. Restoring the local
      // is actually needed on Android only where we might not have a complete
      // view of local window/tabs.

      // Two local headers cannot coexist because they would use the very same
      // storage key in ModelTypeStore/LevelDB.
      DCHECK(!found_local_header);
      found_local_header = true;

      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
      DVLOG(1) << "Loaded local header.";
    } else {
      DCHECK(specifics.has_tab());

      // This is a valid old tab node, add it to the tracker and associate
      // it (using the new tab id).
      DVLOG(1) << "Associating local tab " << specifics.tab().tab_id()
               << " with node " << specifics.tab_node_id();

      // TODO(mastiz): Move call to ReassociateLocalTab() into
      // UpdateTrackerWithSpecifics(), possibly merge with OnTabNodeSeen(). Also
      // consider merging this branch with processing of foreign tabs above.
      session_tracker_.ReassociateLocalTab(
          specifics.tab_node_id(),
          SessionID::FromSerializedValue(specifics.tab().tab_id()));
      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
    }
  }

  // Cleanup all foreign sessions, since orphaned tabs may have been added after
  // the header.
  for (const SyncedSession* session :
       session_tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW)) {
    session_tracker_.CleanupSession(session->session_tag);
  }
}

SessionStore::~SessionStore() {}

std::unique_ptr<syncer::DataBatch> SessionStore::GetSessionDataForKeys(
    const std::vector<std::string>& storage_keys) const {
  // Decode |storage_keys| into a map that can be fed to
  // SerializePartialTrackerToSpecifics().
  std::map<std::string, std::set<int>> session_tag_to_node_ids;
  for (const std::string& storage_key : storage_keys) {
    std::string session_tag;
    int tab_node_id;
    bool success = DecodeStorageKey(storage_key, &session_tag, &tab_node_id);
    DCHECK(success);
    session_tag_to_node_ids[session_tag].insert(tab_node_id);
  }
  // Run the actual serialization into a data batch.
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  SerializePartialTrackerToSpecifics(
      session_tracker_, session_tag_to_node_ids,
      base::BindRepeating(
          [](syncer::MutableDataBatch* batch, const std::string& session_name,
             sync_pb::SessionSpecifics* specifics) {
            DCHECK(AreValidSpecifics(*specifics));
            // Local variable used to avoid assuming argument evaluation order.
            const std::string storage_key = GetStorageKey(*specifics);
            batch->Put(storage_key, MoveToEntityData(session_name, specifics));
          },
          batch.get()));
  return batch;
}

std::unique_ptr<syncer::DataBatch> SessionStore::GetAllSessionData() const {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  SerializeTrackerToSpecifics(
      session_tracker_,
      base::BindRepeating(
          [](syncer::MutableDataBatch* batch, const std::string& session_name,
             sync_pb::SessionSpecifics* specifics) {
            DCHECK(AreValidSpecifics(*specifics));
            // Local variable used to avoid assuming argument evaluation order.
            const std::string storage_key = GetStorageKey(*specifics);
            batch->Put(storage_key, MoveToEntityData(session_name, specifics));
          },
          batch.get()));
  return batch;
}

std::unique_ptr<SessionStore::WriteBatch> SessionStore::CreateWriteBatch(
    syncer::OnceModelErrorHandler error_handler) {
  // The store is guaranteed to outlive WriteBatch instances (as per API
  // requirement).
  return std::make_unique<WriteBatch>(
      store_->CreateWriteBatch(),
      base::BindOnce(&ModelTypeStore::CommitWriteBatch,
                     base::Unretained(store_.get())),
      std::move(error_handler), &session_tracker_);
}

void SessionStore::DeleteAllDataAndMetadata() {
  session_tracker_.Clear();
  return store_->DeleteAllDataAndMetadata(base::DoNothing());
}

}  // namespace sync_sessions
