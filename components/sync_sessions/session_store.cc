// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_store.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/time.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {
namespace {

using sync_pb::SessionSpecifics;
using syncer::DataTypeStore;
using syncer::MetadataChangeList;

// NOTE: Values are persisted (as part of storage keys); do not renumber, and
// only add new entries at the end.
enum class EntityType {
  kHeader = 0,
  kTab = 1,
  kScreenshot = 2,
  // Add new entries here.
  kMaxValue = kScreenshot
};

// Returns the type (header, tab, or screenshot) contained in `specifics`, which
// must be valid according to SessionStore::AreValidSpecifics().
EntityType EntityTypeFromValidSpecifics(
    const sync_pb::SessionSpecifics& specifics) {
  DCHECK(SessionStore::AreValidSpecifics(specifics));

  if (specifics.has_header()) {
    return EntityType::kHeader;
  }
  if (specifics.has_tab()) {
    return EntityType::kTab;
  }
  DCHECK(specifics.has_tab_screenshot());
  return EntityType::kScreenshot;
}

std::string TabNodeIdToClientTag(const std::string& session_tag,
                                 int tab_node_id) {
  CHECK_GT(tab_node_id, TabNodePool::kInvalidTabNodeID);
  return base::StringPrintf("%s %d", session_tag.c_str(), tab_node_id);
}

std::string ScreenshotTabNodeIdToClientTag(const std::string& session_tag,
                                           int tab_node_id) {
  return TabNodeIdToClientTag(session_tag, tab_node_id) + " screenshot";
}

std::string EncodeStorageKey(const std::string& session_tag,
                             int tab_node_id,
                             EntityType type) {
  base::Pickle pickle;
  pickle.WriteString(session_tag);
  pickle.WriteInt(tab_node_id);
  switch (type) {
    case EntityType::kHeader:
    case EntityType::kTab:
      // For backwards compatibility reasons, header and tab entities do *not*
      // encode the type. They can instead be distinguished by the `tab_node_id`
      // (which is `kInvalidTabNodeID` for headers).
      break;
    case EntityType::kScreenshot:
      pickle.WriteInt(static_cast<int>(type));
      break;
  }
  return std::string(pickle.AsStringView());
}

bool DecodeStorageKey(const std::string& storage_key,
                      std::string* session_tag,
                      int* tab_node_id,
                      EntityType* type) {
  CHECK(session_tag);
  CHECK(tab_node_id);
  CHECK(type);

  base::PickleIterator iter =
      base::PickleIterator::WithData(base::as_byte_span(storage_key));
  if (!iter.ReadString(session_tag)) {
    return false;
  }
  if (!iter.ReadInt(tab_node_id)) {
    return false;
  }
  int type_int = -1;
  if (!iter.ReadInt(&type_int)) {
    // For backward compatibility, if the type is missing, it's either a header
    // or a tab.
    *type = (*tab_node_id == TabNodePool::kInvalidTabNodeID)
                ? EntityType::kHeader
                : EntityType::kTab;
    return true;
  }
  // Only screenshot entities should have the type set explicitly (see
  // EncodeStorageKey()).
  if (type_int != static_cast<int>(EntityType::kScreenshot)) {
    return false;
  }
  *type = EntityType::kScreenshot;
  return true;
}

std::unique_ptr<syncer::EntityData> MoveToEntityData(
    const std::string& client_name,
    SessionSpecifics* specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = client_name;
  if (specifics->has_header()) {
    entity_data->name += " (header)";
  } else if (specifics->has_tab()) {
    entity_data->name +=
        base::StringPrintf(" (tab node %d)", specifics->tab_node_id());
  } else if (specifics->has_tab_screenshot()) {
    entity_data->name += base::StringPrintf(" (screenshot for tab node %d)",
                                            specifics->tab_node_id());
  }
  entity_data->specifics.mutable_session()->Swap(specifics);
  return entity_data;
}

void ForwardError(syncer::OnceModelErrorHandler error_handler,
                  const std::optional<syncer::ModelError>& error) {
  if (error) {
    std::move(error_handler).Run(*error);
  }
}

// Parses the content of |record_list| into |*initial_data|. The output
// parameters are first for binding purposes.
std::optional<syncer::ModelError> ParseInitialDataOnBackendSequence(
    std::map<std::string, sync_pb::SessionSpecifics>* initial_data,
    std::string* session_name,
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  TRACE_EVENT0("sync", "sync_sessions::ParseInitialDataOnBackendSequence");
  DCHECK(initial_data);
  DCHECK(initial_data->empty());
  DCHECK(record_list);

  for (DataTypeStore::Record& record : *record_list) {
    const std::string& storage_key = record.id;
    SessionSpecifics specifics;
    if (storage_key.empty() ||
        !specifics.ParseFromString(std::move(record.value))) {
      DVLOG(1) << "Ignoring corrupt database entry with key: " << storage_key;
      continue;
    }
    (*initial_data)[storage_key] = std::move(specifics);
  }

  *session_name = syncer::GetPersonalizableDeviceNameBlocking();

  return std::nullopt;
}

}  // namespace

struct SessionStore::Builder {
  base::WeakPtr<SyncSessionsClient> sessions_client;
  OpenCallback callback;
  SessionInfo local_session_info;
  std::unique_ptr<syncer::DataTypeStore> underlying_store;
  std::unique_ptr<syncer::MetadataBatch> metadata_batch;
  std::map<std::string, sync_pb::SessionSpecifics> initial_data;
};

// static
void SessionStore::Open(const std::string& cache_guid,
                        SyncSessionsClient* sessions_client,
                        OpenCallback callback) {
  DCHECK(sessions_client);

  DVLOG(1) << "Opening session store";

  auto builder = std::make_unique<Builder>();
  builder->sessions_client = sessions_client->AsWeakPtr();
  builder->callback = std::move(callback);

  builder->local_session_info.device_type = syncer::GetLocalDeviceType();
  builder->local_session_info.device_form_factor =
      syncer::GetLocalDeviceFormFactor();
  builder->local_session_info.session_tag = cache_guid;

  sessions_client->GetStoreFactory().Run(
      syncer::SESSIONS, base::BindOnce(&OnStoreCreated, std::move(builder)));
}

SessionStore::WriteBatch::WriteBatch(
    std::unique_ptr<DataTypeStore::WriteBatch> batch,
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

std::vector<std::string>
SessionStore::WriteBatch::DeleteForeignEntityAndUpdateTracker(
    const std::string& storage_key) {
  std::string session_tag;
  int tab_node_id;
  EntityType type;
  bool success =
      DecodeStorageKey(storage_key, &session_tag, &tab_node_id, &type);
  DCHECK(success);
  DCHECK_NE(session_tag, session_tracker_->GetLocalSessionTag());

  base::flat_set<std::string> deleted_storage_keys;
  deleted_storage_keys.insert(storage_key);

  switch (type) {
    case EntityType::kHeader:
      // Removal of a foreign header entity cascades the deletion of all tabs
      // and screenshots in the same session too.
      for (int cascading_tab_node_id :
           session_tracker_->LookupTabNodeIds(session_tag)) {
        deleted_storage_keys.insert(
            GetTabStorageKey(session_tag, cascading_tab_node_id));
      }
      for (int cascading_screenshot_tab_node_id :
           session_tracker_->LookupScreenshotTabNodeIds(session_tag)) {
        deleted_storage_keys.insert(GetTabScreenshotStorageKey(
            session_tag, cascading_screenshot_tab_node_id));
      }

      // Delete session itself.
      session_tracker_->DeleteForeignSession(session_tag);
      break;
    case EntityType::kTab:
      // Removal of a foreign tab entity cascades the deletion of the associated
      // screenshot entity.
      if (session_tracker_->TabNodeHasScreenshot(session_tag, tab_node_id)) {
        deleted_storage_keys.insert(
            GetTabScreenshotStorageKey(session_tag, tab_node_id));
      }
      session_tracker_->DeleteForeignTab(session_tag, tab_node_id);
      break;
    case EntityType::kScreenshot:
      // Removal of a screenshot entity does not cascade. If the tab node
      // doesn't exist, this does nothing.
      session_tracker_->SetTabNodeHasScreenshot(session_tag, tab_node_id,
                                                /*has_screenshot=*/false);
      break;
  }

  for (const std::string& key : deleted_storage_keys) {
    batch_->DeleteData(key);
  }

  return std::move(deleted_storage_keys).extract();
}

std::string SessionStore::WriteBatch::PutWithoutUpdatingTracker(
    const sync_pb::SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));

  const std::string storage_key = GetStorageKey(specifics);
  batch_->WriteData(storage_key, specifics.SerializeAsString());
  return storage_key;
}

std::vector<std::string>
SessionStore::WriteBatch::DeleteLocalTabWithoutUpdatingTracker(
    int tab_node_id) {
  const std::string session_tag = session_tracker_->GetLocalSessionTag();
  const std::string tab_storage_key =
      GetTabStorageKey(session_tag, tab_node_id);
  const std::string tab_screenshot_storage_key =
      GetTabScreenshotStorageKey(session_tag, tab_node_id);

  batch_->DeleteData(tab_storage_key);
  batch_->DeleteData(tab_screenshot_storage_key);

  return {std::move(tab_storage_key), std::move(tab_screenshot_storage_key)};
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
  // A session tag is always required.
  if (specifics.session_tag().empty()) {
    return false;
  }

  // Only one of header, tab or tab_screenshot may be set.
  if (((specifics.has_header() ? 1 : 0) + (specifics.has_tab() ? 1 : 0) +
       (specifics.has_tab_screenshot() ? 1 : 0)) != 1) {
    return false;
  }

  // Tabs must have both a valid tab node ID and tab ID.
  if (specifics.has_tab()) {
    if (specifics.tab_node_id() < 0) {
      return false;
    }
    if (specifics.tab().tab_id() <= 0) {
      return false;
    }
    return true;
  }

  // Tab screenshots must have a valid tab node ID.
  if (specifics.has_tab_screenshot()) {
    if (specifics.tab_node_id() < 0) {
      return false;
    }
    return true;
  }

  if (specifics.has_header()) {
    // A header entity must not have a tab node ID.
    if (specifics.tab_node_id() != TabNodePool::kInvalidTabNodeID) {
      return false;
    }
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
    return true;
  }

  // Neither header nor tab is set.
  return false;
}

// static
std::string SessionStore::GetClientTag(const SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));

  if (specifics.has_header()) {
    return specifics.session_tag();
  }

  if (specifics.has_tab()) {
    return TabNodeIdToClientTag(specifics.session_tag(),
                                specifics.tab_node_id());
  }

  DCHECK(specifics.has_tab_screenshot());
  return ScreenshotTabNodeIdToClientTag(specifics.session_tag(),
                                        specifics.tab_node_id());
}

// static
std::string SessionStore::GetStorageKey(const SessionSpecifics& specifics) {
  DCHECK(AreValidSpecifics(specifics));
  return EncodeStorageKey(specifics.session_tag(), specifics.tab_node_id(),
                          EntityTypeFromValidSpecifics(specifics));
}

// static
std::string SessionStore::GetHeaderStorageKey(const std::string& session_tag) {
  return EncodeStorageKey(session_tag, TabNodePool::kInvalidTabNodeID,
                          EntityType::kHeader);
}

// static
std::string SessionStore::GetTabStorageKey(const std::string& session_tag,
                                           int tab_node_id) {
  DCHECK_GE(tab_node_id, 0);
  return EncodeStorageKey(session_tag, tab_node_id, EntityType::kTab);
}

// static
std::string SessionStore::GetTabScreenshotStorageKey(
    const std::string& session_tag,
    int tab_node_id) {
  DCHECK_GE(tab_node_id, 0);
  return EncodeStorageKey(session_tag, tab_node_id, EntityType::kScreenshot);
}

bool SessionStore::StorageKeyMatchesLocalSession(
    const std::string& storage_key) const {
  std::string session_tag;
  int tab_node_id;
  EntityType type;
  bool success =
      DecodeStorageKey(storage_key, &session_tag, &tab_node_id, &type);
  DCHECK(success);
  return session_tag == local_session_info_.session_tag;
}

// static
std::string SessionStore::GetTabClientTagForTest(const std::string& session_tag,
                                                 int tab_node_id) {
  return TabNodeIdToClientTag(session_tag, tab_node_id);
}

// static
void SessionStore::OnStoreCreated(
    std::unique_ptr<Builder> builder,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<DataTypeStore> underlying_store) {
  DCHECK(builder);

  if (error) {
    std::move(builder->callback)
        .Run(error, /*store=*/nullptr,
             /*metadata_batch=*/nullptr);
    return;
  }

  DCHECK(underlying_store);
  builder->underlying_store = std::move(underlying_store);

  Builder* builder_copy = builder.get();
  builder_copy->underlying_store->ReadAllMetadata(
      base::BindOnce(&OnReadAllMetadata, std::move(builder)));
}

// static
void SessionStore::OnReadAllMetadata(
    std::unique_ptr<Builder> builder,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "sync_sessions::SessionStore::OnReadAllMetadata");
  DCHECK(builder);

  if (error) {
    std::move(builder->callback)
        .Run(error, /*store=*/nullptr,
             /*metadata_batch=*/nullptr);
    return;
  }

  DCHECK(metadata_batch);
  builder->metadata_batch = std::move(metadata_batch);

  Builder* builder_copy = builder.get();
  builder_copy->underlying_store->ReadAllDataAndPreprocess(
      base::BindOnce(
          &ParseInitialDataOnBackendSequence,
          base::Unretained(&builder_copy->initial_data),
          base::Unretained(&builder_copy->local_session_info.client_name)),
      base::BindOnce(&OnReadAllData, std::move(builder)));
}

// static
void SessionStore::OnReadAllData(
    std::unique_ptr<Builder> builder,
    const std::optional<syncer::ModelError>& error) {
  TRACE_EVENT0("sync", "sync_sessions::SessionStore::OnReadAllData");
  DCHECK(builder);

  if (error) {
    std::move(builder->callback)
        .Run(error, /*store=*/nullptr,
             /*metadata_batch=*/nullptr);
    return;
  }

  // We avoid initialization of the store if the callback was cancelled, in
  // case dependencies (SessionSyncClient) are already destroyed, even though
  // the current implementation doesn't seem to crash otherwise.
  if (builder->callback.IsCancelled()) {
    return;
  }

  // WrapUnique() used because constructor is private.
  auto session_store = base::WrapUnique(new SessionStore(
      builder->local_session_info, std::move(builder->underlying_store),
      std::move(builder->initial_data),
      builder->metadata_batch->GetAllMetadata(),
      builder->sessions_client.get()));

  std::move(builder->callback)
      .Run(/*error=*/std::nullopt, std::move(session_store),
           std::move(builder->metadata_batch));
}

// static
std::unique_ptr<SessionStore> SessionStore::RecreateEmptyStore(
    SessionStore::SessionInfo local_session_info_without_session_tag,
    std::unique_ptr<syncer::DataTypeStore> underlying_store,
    const std::string& cache_guid,
    SyncSessionsClient* sessions_client) {
  local_session_info_without_session_tag.session_tag = cache_guid;
  // WrapUnique() used because constructor is private.
  return base::WrapUnique(new SessionStore(
      local_session_info_without_session_tag, std::move(underlying_store),
      std::map<std::string, sync_pb::SessionSpecifics>(),
      syncer::EntityMetadataMap(), sessions_client));
}

SessionStore::SessionStore(
    const SessionInfo& local_session_info,
    std::unique_ptr<syncer::DataTypeStore> underlying_store,
    std::map<std::string, sync_pb::SessionSpecifics> initial_data,
    const syncer::EntityMetadataMap& initial_metadata,
    SyncSessionsClient* sessions_client)
    : local_session_info_(local_session_info),
      store_(std::move(underlying_store)),
      sessions_client_(sessions_client),
      session_tracker_(sessions_client) {
  session_tracker_.InitLocalSession(
      local_session_info_.session_tag, local_session_info_.client_name,
      local_session_info_.device_type, local_session_info_.device_form_factor);

  DCHECK(store_);
  DCHECK(sessions_client_);

  DVLOG(1) << "Initializing session store with " << initial_data.size()
           << " restored entities and " << initial_metadata.size()
           << " metadata entries.";

  bool found_local_header = false;

  for (auto& [storage_key, specifics] : initial_data) {
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
        syncer::ProtoTimeToTime(metadata_it->second->modification_time());

    if (specifics.session_tag() != local_session_info_.session_tag) {
      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
    } else if (specifics.has_header()) {
      // This is previously stored local header information. Restoring the local
      // is actually needed on Android only where we might not have a complete
      // view of local window/tabs.

      // Two local headers cannot coexist because they would use the very same
      // storage key in DataTypeStore/LevelDB.
      DCHECK(!found_local_header);
      found_local_header = true;

      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
      DVLOG(1) << "Loaded local header.";
    } else if (specifics.has_tab()) {
      // This is a valid old tab node, add it to the tracker and associate
      // it (using the new tab id).
      DVLOG(1) << "Associating local tab " << specifics.tab().tab_id()
               << " with node " << specifics.tab_node_id();

      session_tracker_.ReassociateLocalTab(
          specifics.tab_node_id(),
          SessionID::FromSerializedValue(specifics.tab().tab_id()));
      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
    } else if (specifics.has_tab_screenshot()) {
      UpdateTrackerWithSpecifics(specifics, mtime, &session_tracker_);
    } else {
      // Unreachable because `AreValidSpecifics()` was checked above.
      NOTREACHED();
    }
  }

  // Cleanup all foreign sessions, since orphaned tabs may have been added after
  // the header.
  for (const SyncedSession* session :
       session_tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW)) {
    session_tracker_.CleanupSession(session->GetSessionTag());
  }
}

SessionStore::~SessionStore() = default;

std::unique_ptr<syncer::DataBatch> SessionStore::GetSessionDataForKeys(
    const std::vector<std::string>& storage_keys) const {
  // Decode |storage_keys| into two maps that can be fed to
  // SerializePartialTrackerToSpecifics().
  std::map<std::string, std::set<int>> session_tag_to_node_ids;
  std::map<std::string, std::set<int>> session_tag_to_screenshot_node_ids;

  for (const std::string& storage_key : storage_keys) {
    std::string session_tag;
    int tab_node_id;
    EntityType type;
    bool success =
        DecodeStorageKey(storage_key, &session_tag, &tab_node_id, &type);
    DCHECK(success);
    switch (type) {
      case EntityType::kHeader:
      case EntityType::kTab:
        session_tag_to_node_ids[session_tag].insert(tab_node_id);
        break;
      case EntityType::kScreenshot:
        session_tag_to_screenshot_node_ids[session_tag].insert(tab_node_id);
        break;
    }
  }
  // Run the actual serialization into a data batch.
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  SerializePartialTrackerToSpecifics(
      session_tracker_, session_tag_to_node_ids,
      session_tag_to_screenshot_node_ids,
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
      base::BindOnce(&DataTypeStore::CommitWriteBatch,
                     base::Unretained(store_.get())),
      std::move(error_handler), &session_tracker_);
}

// static
SessionStore::RecreateEmptyStoreCallback SessionStore::DeleteAllDataAndMetadata(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    std::unique_ptr<SessionStore> session_store) {
  CHECK(session_store);

  // Clear the store and related info.
  session_store->session_tracker_.Clear();
  session_store->store_->DeleteAllDataAndMetadata(
      std::move(metadata_change_list), base::DoNothing());

  // Grab the necessary stuff for (synchronously) recreating a store later.
  SessionInfo local_session_info = session_store->local_session_info_;
  // After clearing data and metadata, the session tag may not be valid anymore.
  // Clear it to prevent accidental reuse.
  local_session_info.session_tag.clear();

  std::unique_ptr<syncer::DataTypeStore> underlying_store =
      std::move(session_store->store_);

  session_store.reset();

  return base::BindOnce(&SessionStore::RecreateEmptyStore,
                        std::move(local_session_info),
                        std::move(underlying_store));
}

}  // namespace sync_sessions
