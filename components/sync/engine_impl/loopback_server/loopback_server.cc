// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/loopback_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "components/sync/engine_impl/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;

using syncer::GetModelType;
using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace syncer {

class LoopbackServerEntity;

namespace {

static const int kCurrentLoopbackServerProtoVersion = 1;
static const int kKeystoreKeyLength = 16;

// Properties of the bookmark bar permanent folders.
static const char kBookmarkBarFolderServerTag[] = "bookmark_bar";
static const char kBookmarkBarFolderName[] = "Bookmark Bar";
static const char kOtherBookmarksFolderServerTag[] = "other_bookmarks";
static const char kOtherBookmarksFolderName[] = "Other Bookmarks";
static const char kSyncedBookmarksFolderServerTag[] = "synced_bookmarks";
static const char kSyncedBookmarksFolderName[] = "Synced Bookmarks";

// A filter used during GetUpdates calls to determine what information to
// send back to the client; filtering out old entities and tracking versions to
// use in response progress markers. Note that only the GetUpdatesMessage's
// from_progress_marker is used to determine this; legacy fields are ignored.
class UpdateSieve {
 public:
  explicit UpdateSieve(const sync_pb::GetUpdatesMessage& message)
      : UpdateSieve(MessageToVersionMap(message)) {}
  ~UpdateSieve() {}

  // Sets the progress markers in |get_updates_response| based on the highest
  // version between request progress markers and response entities.
  void SetProgressMarkers(
      sync_pb::GetUpdatesResponse* get_updates_response) const {
    for (const auto& kv : response_version_map_) {
      sync_pb::DataTypeProgressMarker* new_marker =
          get_updates_response->add_new_progress_marker();
      new_marker->set_data_type_id(
          GetSpecificsFieldNumberFromModelType(kv.first));
      new_marker->set_token(base::Int64ToString(kv.second));
    }
  }

  // Determines whether the server should send an |entity| to the client as
  // part of a GetUpdatesResponse. Update internal tracking of max versions as a
  // side effect which will later be used to set response progress markers.
  bool ClientWantsItem(const LoopbackServerEntity& entity) {
    ModelType type = entity.GetModelType();
    // Return only requested datatypes, which makes sure we don't add new
    // entries to |response_version_map_|, which would otherwise send
    // unnecessary (and unrequested) progress markers in the response.
    auto it = request_version_map_.find(type);
    if (it == request_version_map_.end())
      return false;
    DCHECK_NE(0U, request_version_map_.count(type));
    int64_t version = entity.GetVersion();
    response_version_map_[type] =
        std::max(response_version_map_[type], version);
    return it->second < version;
  }

 private:
  using ModelTypeToVersionMap = std::map<ModelType, int64_t>;

  static UpdateSieve::ModelTypeToVersionMap MessageToVersionMap(
      const sync_pb::GetUpdatesMessage& get_updates_message) {
    DCHECK_GT(get_updates_message.from_progress_marker_size(), 0)
        << "A GetUpdates request must have at least one progress marker.";
    ModelTypeToVersionMap request_version_map;

    for (int i = 0; i < get_updates_message.from_progress_marker_size(); i++) {
      sync_pb::DataTypeProgressMarker marker =
          get_updates_message.from_progress_marker(i);

      int64_t version = 0;
      // Let the version remain zero if there is no token or an empty token (the
      // first request for this type).
      if (marker.has_token() && !marker.token().empty()) {
        bool parsed = base::StringToInt64(marker.token(), &version);
        DCHECK(parsed) << "Unable to parse progress marker token.";
      }

      ModelType model_type =
          syncer::GetModelTypeFromSpecificsFieldNumber(marker.data_type_id());
      DCHECK(request_version_map.find(model_type) == request_version_map.end());
      request_version_map[model_type] = version;
    }
    return request_version_map;
  }

  explicit UpdateSieve(const ModelTypeToVersionMap request_version_map)
      : request_version_map_(request_version_map),
        response_version_map_(request_version_map) {}

  // The largest versions the client has seen before this request, and is used
  // to filter entities to send back to clients. The values in this map are not
  // updated after being initially set. The presence of a type in this map is a
  // proxy for the desire to receive results about this type.
  const ModelTypeToVersionMap request_version_map_;

  // The largest versions seen between client and server, ultimately used to
  // send progress markers back to the client.
  ModelTypeToVersionMap response_version_map_;
};

}  // namespace

LoopbackServer::LoopbackServer(const base::FilePath& persistent_file)
    : strong_consistency_model_enabled_(false),
      version_(0),
      store_birthday_(0),
      persistent_file_(persistent_file),
      observer_for_tests_(nullptr) {
  DCHECK(!persistent_file_.empty());
  Init();
}

LoopbackServer::~LoopbackServer() {}

void LoopbackServer::Init() {
  if (LoadStateFromFile(persistent_file_))
    return;

  keystore_keys_.push_back(GenerateNewKeystoreKey());

  const bool create_result = CreateDefaultPermanentItems();
  DCHECK(create_result) << "Permanent items were not created successfully.";
}

std::string LoopbackServer::GenerateNewKeystoreKey() const {
  // TODO(pastarmovj): Check if true random bytes is ok or alpha-nums is needed?
  return base::RandBytesAsString(kKeystoreKeyLength);
}

bool LoopbackServer::CreatePermanentBookmarkFolder(
    const std::string& server_tag,
    const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<LoopbackServerEntity> entity =
      PersistentPermanentEntity::CreateNew(
          syncer::BOOKMARKS, server_tag, name,
          ModelTypeToRootTag(syncer::BOOKMARKS));
  if (!entity)
    return false;

  SaveEntity(std::move(entity));
  return true;
}

bool LoopbackServer::CreateDefaultPermanentItems() {
  // Permanent folders are always required for Bookmarks (hierarchical
  // structure) and Nigori (data stored in permanent root folder).
  ModelTypeSet permanent_folder_types =
      ModelTypeSet(syncer::BOOKMARKS, syncer::NIGORI);

  for (ModelType model_type : permanent_folder_types) {
    std::unique_ptr<LoopbackServerEntity> top_level_entity =
        PersistentPermanentEntity::CreateTopLevel(model_type);
    if (!top_level_entity) {
      return false;
    }
    top_level_permanent_item_ids_[model_type] = top_level_entity->GetId();
    SaveEntity(std::move(top_level_entity));

    if (model_type == syncer::BOOKMARKS) {
      if (!CreatePermanentBookmarkFolder(kBookmarkBarFolderServerTag,
                                         kBookmarkBarFolderName))
        return false;
      if (!CreatePermanentBookmarkFolder(kOtherBookmarksFolderServerTag,
                                         kOtherBookmarksFolderName))
        return false;
    }
  }

  return true;
}

std::string LoopbackServer::GetTopLevelPermanentItemId(
    syncer::ModelType model_type) {
  auto it = top_level_permanent_item_ids_.find(model_type);
  if (it == top_level_permanent_item_ids_.end()) {
    return std::string();
  }
  return it->second;
}

void LoopbackServer::UpdateEntityVersion(LoopbackServerEntity* entity) {
  entity->SetVersion(++version_);
}

void LoopbackServer::SaveEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  UpdateEntityVersion(entity.get());
  entities_[entity->GetId()] = std::move(entity);
}

void LoopbackServer::HandleCommand(
    const string& request,
    HttpResponse::ServerConnectionCode* server_status,
    int64_t* response_code,
    std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  DCHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  sync_pb::ClientToServerResponse response_proto;

  if (message.has_store_birthday() &&
      message.store_birthday() != GetStoreBirthday()) {
    response_proto.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  } else {
    bool success = false;
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        success = HandleGetUpdatesRequest(message.get_updates(),
                                          response_proto.mutable_get_updates());
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        success = HandleCommitRequest(message.commit(),
                                      message.invalidator_client_id(),
                                      response_proto.mutable_commit());
        break;
      case sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA:
        ClearServerData();
        response_proto.mutable_clear_server_data();
        success = true;
        break;
      default:
        *server_status = HttpResponse::SYNC_SERVER_ERROR;
        *response_code = net::ERR_NOT_IMPLEMENTED;
        *response = string();
        return;
    }

    if (!success) {
      *server_status = HttpResponse::SYNC_SERVER_ERROR;
      *response_code = net::ERR_FAILED;
      *response = string();
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.Local.RequestTypeOnError", message.message_contents(),
          sync_pb::ClientToServerMessage_Contents_Contents_MAX);
      return;
    }

    response_proto.set_error_code(sync_pb::SyncEnums::SUCCESS);
  }

  response_proto.set_store_birthday(GetStoreBirthday());

  *server_status = HttpResponse::SERVER_CONNECTION_OK;
  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();

  // TODO(pastarmovj): This should be done asynchronously.
  SaveStateToFile(persistent_file_);
}

void LoopbackServer::EnableStrongConsistencyWithConflictDetectionModel() {
  strong_consistency_model_enabled_ = true;
}

bool LoopbackServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  auto sieve = std::make_unique<UpdateSieve>(get_updates);

  // This folder is called "Synced Bookmarks" by sync and is renamed
  // "Mobile Bookmarks" by the mobile client UIs.
  if (get_updates.create_mobile_bookmarks_folder() &&
      !CreatePermanentBookmarkFolder(kSyncedBookmarksFolderServerTag,
                                     kSyncedBookmarksFolderName)) {
    return false;
  }

  bool send_encryption_keys_based_on_nigori = false;
  for (const auto& kv : entities_) {
    const LoopbackServerEntity& entity = *kv.second;
    if (sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity = response->add_entries();
      entity.SerializeAsProto(response_entity);

      if (entity.GetModelType() == syncer::NIGORI) {
        send_encryption_keys_based_on_nigori =
            response_entity->specifics().nigori().passphrase_type() ==
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
      }
    }
  }

  if (send_encryption_keys_based_on_nigori ||
      get_updates.need_encryption_key()) {
    for (const string& key : keystore_keys_) {
      response->add_encryption_keys(key);
    }
  }

  sieve->SetProgressMarkers(response);
  return true;
}

string LoopbackServer::CommitEntity(
    const sync_pb::SyncEntity& client_entity,
    sync_pb::CommitResponse_EntryResponse* entry_response,
    const string& client_guid,
    const string& parent_id) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return string();
  }

  // If strong consistency model is enabled (usually on a per-datatype level,
  // but implemented here as a global state), the server detects version
  // mismatches and responds with CONFLICT.
  if (strong_consistency_model_enabled_) {
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      const LoopbackServerEntity* server_entity = iter->second.get();
      if (server_entity->GetVersion() != client_entity.version()) {
        entry_response->set_response_type(sync_pb::CommitResponse::CONFLICT);
        return client_entity.id_string();
      }
    }
  }

  std::unique_ptr<LoopbackServerEntity> entity;
  syncer::ModelType type = GetModelType(client_entity);
  if (client_entity.deleted()) {
    entity = PersistentTombstoneEntity::CreateFromEntity(client_entity);
    if (entity) {
      DeleteChildren(client_entity.id_string());
    }
  } else if (type == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    DCHECK(iter != entities_.end());
    entity = PersistentPermanentEntity::CreateUpdatedNigoriEntity(
        client_entity, *iter->second);
  } else if (type == syncer::BOOKMARKS) {
    // TODO(pvalenzuela): Validate entity's parent ID.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      entity = PersistentBookmarkEntity::CreateUpdatedVersion(
          client_entity, *iter->second, parent_id);
    } else {
      entity = PersistentBookmarkEntity::CreateNew(client_entity, parent_id,
                                                   client_guid);
    }
  } else {
    entity = PersistentUniqueClientEntity::CreateFromEntity(client_entity);
  }

  if (!entity)
    return string();

  const std::string id = entity->GetId();
  SaveEntity(std::move(entity));
  BuildEntryResponseForSuccessfulCommit(id, entry_response);
  return id;
}

void LoopbackServer::OverrideResponseType(
    ResponseTypeProvider response_type_override) {
  response_type_override_ = std::move(response_type_override);
}

void LoopbackServer::BuildEntryResponseForSuccessfulCommit(
    const std::string& entity_id,
    sync_pb::CommitResponse_EntryResponse* entry_response) {
  EntityMap::const_iterator iter = entities_.find(entity_id);
  DCHECK(iter != entities_.end());
  const LoopbackServerEntity& entity = *iter->second;
  entry_response->set_response_type(response_type_override_
                                        ? response_type_override_.Run(entity)
                                        : sync_pb::CommitResponse::SUCCESS);
  entry_response->set_id_string(entity.GetId());

  if (entity.IsDeleted()) {
    entry_response->set_version(entity.GetVersion() + 1);
  } else {
    entry_response->set_version(entity.GetVersion());
    entry_response->set_name(entity.GetName());
  }
}

bool LoopbackServer::IsChild(const string& id,
                             const string& potential_parent_id) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    // We've hit an ID (probably the imaginary root entity) that isn't stored
    // by the server, so it can't be a child.
    return false;
  }

  const LoopbackServerEntity& entity = *iter->second;
  if (entity.GetParentId() == potential_parent_id)
    return true;

  // Recursively look up the tree.
  return IsChild(entity.GetParentId(), potential_parent_id);
}

void LoopbackServer::DeleteChildren(const string& parent_id) {
  std::vector<sync_pb::SyncEntity> tombstones;
  // Find all the children of |parent_id|.
  for (auto& entity : entities_) {
    if (IsChild(entity.first, parent_id)) {
      sync_pb::SyncEntity proto;
      entity.second->SerializeAsProto(&proto);
      tombstones.emplace_back(proto);
    }
  }

  for (auto& tombstone : tombstones) {
    SaveEntity(PersistentTombstoneEntity::CreateFromEntity(tombstone));
  }
}

bool LoopbackServer::HandleCommitRequest(
    const sync_pb::CommitMessage& commit,
    const std::string& invalidator_client_id,
    sync_pb::CommitResponse* response) {
  std::map<string, string> client_to_server_ids;
  string guid = commit.cache_guid();
  ModelTypeSet committed_model_types;

  // TODO(pvalenzuela): Add validation of CommitMessage.entries.
  for (const sync_pb::SyncEntity& client_entity : commit.entries()) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->add_entryresponse();

    string parent_id = client_entity.parent_id_string();
    if (client_to_server_ids.find(parent_id) != client_to_server_ids.end()) {
      parent_id = client_to_server_ids[parent_id];
    }

    const string entity_id =
        CommitEntity(client_entity, entry_response, guid, parent_id);
    if (entity_id.empty()) {
      return false;
    }

    // Record the ID if it was renamed.
    if (entity_id != client_entity.id_string()) {
      client_to_server_ids[client_entity.id_string()] = entity_id;
    }

    EntityMap::const_iterator iter = entities_.find(entity_id);
    DCHECK(iter != entities_.end());
    committed_model_types.Put(iter->second->GetModelType());
  }

  if (observer_for_tests_)
    observer_for_tests_->OnCommit(invalidator_client_id, committed_model_types);

  return true;
}

void LoopbackServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  entities_.clear();
  keystore_keys_.clear();
  ++store_birthday_;
  base::DeleteFile(persistent_file_, false);
  Init();
}

std::string LoopbackServer::GetStoreBirthday() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Int64ToString(store_birthday_);
}

std::vector<sync_pb::SyncEntity> LoopbackServer::GetSyncEntitiesByModelType(
    ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<sync_pb::SyncEntity> sync_entities;
  for (const auto& kv : entities_) {
    const LoopbackServerEntity& entity = *kv.second;
    if (!(entity.IsDeleted() || entity.IsPermanent()) &&
        entity.GetModelType() == model_type) {
      sync_pb::SyncEntity sync_entity;
      entity.SerializeAsProto(&sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

std::vector<sync_pb::SyncEntity>
LoopbackServer::GetPermanentSyncEntitiesByModelType(ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<sync_pb::SyncEntity> sync_entities;
  for (const auto& kv : entities_) {
    const LoopbackServerEntity& entity = *kv.second;
    if (!entity.IsDeleted() && entity.IsPermanent() &&
        entity.GetModelType() == model_type) {
      sync_pb::SyncEntity sync_entity;
      entity.SerializeAsProto(&sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

std::unique_ptr<base::DictionaryValue>
LoopbackServer::GetEntitiesAsDictionaryValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());

  // Initialize an empty ListValue for all ModelTypes.
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType type : all_types) {
    dictionary->Set(ModelTypeToString(type),
                    std::make_unique<base::ListValue>());
  }

  for (const auto& kv : entities_) {
    const LoopbackServerEntity& entity = *kv.second;
    if (entity.IsDeleted() || entity.IsPermanent()) {
      // Tombstones are ignored as they don't represent current data. Folders
      // are also ignored as current verification infrastructure does not
      // consider them.
      continue;
    }
    base::ListValue* list_value;
    if (!dictionary->GetList(ModelTypeToString(entity.GetModelType()),
                             &list_value)) {
      return std::unique_ptr<base::DictionaryValue>();
    }
    // TODO(pvalenzuela): Store more data for each entity so additional
    // verification can be performed. One example of additional verification
    // is checking the correctness of the bookmark hierarchy.
    list_value->AppendString(entity.GetName());
  }

  return dictionary;
}

bool LoopbackServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetModelType() !=
          GetModelTypeFromSpecifics(updated_specifics)) {
    return false;
  }

  LoopbackServerEntity* entity = iter->second.get();
  entity->SetSpecifics(updated_specifics);
  UpdateEntityVersion(entity);
  return true;
}

bool LoopbackServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetModelType() != syncer::BOOKMARKS ||
      GetModelTypeFromSpecifics(updated_specifics) != syncer::BOOKMARKS) {
    return false;
  }

  PersistentBookmarkEntity* entity =
      static_cast<PersistentBookmarkEntity*>(iter->second.get());

  entity->SetParentId(parent_id);
  entity->SetSpecifics(updated_specifics);
  if (updated_specifics.has_bookmark()) {
    entity->SetName(updated_specifics.bookmark().title());
  }
  UpdateEntityVersion(entity);
  return true;
}

void LoopbackServer::SerializeState(sync_pb::LoopbackServerProto* proto) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  proto->set_version(kCurrentLoopbackServerProtoVersion);
  proto->set_store_birthday(store_birthday_);
  proto->set_last_version_assigned(version_);
  for (const auto& key : keystore_keys_)
    proto->add_keystore_keys(key);
  for (const auto& entity : entities_) {
    auto* new_entity = proto->mutable_entities()->Add();
    entity.second->SerializeAsLoopbackServerEntity(new_entity);
  }
}

bool LoopbackServer::DeSerializeState(
    const sync_pb::LoopbackServerProto& proto) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(proto.version(), kCurrentLoopbackServerProtoVersion);

  store_birthday_ = proto.store_birthday();
  version_ = proto.last_version_assigned();
  for (int i = 0; i < proto.keystore_keys_size(); ++i)
    keystore_keys_.push_back(proto.keystore_keys(i));
  for (int i = 0; i < proto.entities_size(); ++i) {
    std::unique_ptr<LoopbackServerEntity> entity =
        LoopbackServerEntity::CreateEntityFromProto(proto.entities(i));
    // Silently drop entities that cannot be successfully deserialized.
    if (entity)
      entities_[proto.entities(i).entity().id_string()] = std::move(entity);
  }

  // Report success regardless of if some entities were dropped.
  return true;
}

// Saves all entities and server state to a protobuf file in |filename|.
bool LoopbackServer::SaveStateToFile(const base::FilePath& filename) const {
  sync_pb::LoopbackServerProto proto;
  SerializeState(&proto);

  std::string serialized = proto.SerializeAsString();
  if (!base::CreateDirectory(filename.DirName())) {
    LOG(ERROR) << "Loopback sync could not create the storage directory.";
    return false;
  }
  int result = base::WriteFile(filename, serialized.data(), serialized.size());
  UMA_HISTOGRAM_MEMORY_KB("Sync.Local.FileSize", result);
  return result == static_cast<int>(serialized.size());
}

// Loads all entities and server state from a protobuf file in |filename|.
bool LoopbackServer::LoadStateFromFile(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    std::string serialized;
    if (base::ReadFileToString(filename, &serialized)) {
      sync_pb::LoopbackServerProto proto;
      if (serialized.length() > 0 && proto.ParseFromString(serialized)) {
        return DeSerializeState(proto);
      } else {
        LOG(ERROR) << "Loopback sync can not parse the persistent state file.";
        return false;
      }
    } else {
      // TODO(pastarmovj): Try to understand what is the issue e.g. file already
      // open, no access rights etc. and decide if better course of action is
      // available instead of giving up and wiping the global state on the next
      // write.
      LOG(ERROR) << "Loopback sync can not read the persistent state file.";
      return false;
    }
  }
  LOG(WARNING) << "Loopback sync persistent state file does not exist.";
  return false;
}

}  // namespace syncer
