// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/loopback_server.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/clamped_math.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/engine/loopback_server/persistent_permanent_entity.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;

using syncer::DataType;
using syncer::DataTypeSet;
using syncer::GetDataTypeFromSpecifics;

namespace syncer {

class LoopbackServerEntity;

namespace {

static const char kHistogramSuffix[] = "LoopBackServer";

static const int kCurrentLoopbackServerProtoVersion = 1;
static const int kKeystoreKeyLength = 16;

// Properties of the bookmark bar permanent folders.
static const char kBookmarkBarFolderServerTag[] = "bookmark_bar";
static const char kBookmarkBarFolderName[] = "Bookmark Bar";
static const char kOtherBookmarksFolderServerTag[] = "other_bookmarks";
static const char kOtherBookmarksFolderName[] = "Other Bookmarks";
static const char kSyncedBookmarksFolderServerTag[] = "synced_bookmarks";
static const char kSyncedBookmarksFolderName[] = "Synced Bookmarks";

int GetServerMigrationVersion(
    const std::map<DataType, int>& server_migration_versions,
    DataType type) {
  auto server_it = server_migration_versions.find(type);
  return server_it == server_migration_versions.end() ? 0 : server_it->second;
}

class ProgressMarkerToken {
 public:
  static ProgressMarkerToken FromEmpty(int migration_version) {
    ProgressMarkerToken token;
    token.migration_version_ = migration_version;
    return token;
  }

  static ProgressMarkerToken FromString(const std::string& s) {
    DCHECK(!s.empty());
    const vector<std::string_view> splits = base::SplitStringPiece(
        s, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (splits.size() != 2) {
      ProgressMarkerToken token;
      base::StringToInt64(s, &token.entity_version_);
      return token;
    }
    ProgressMarkerToken token;
    if (!base::StringToInt(splits[0], &token.migration_version_) ||
        !base::StringToInt64(splits[1], &token.entity_version_)) {
      return ProgressMarkerToken();
    }
    return token;
  }

  std::string ToString() const {
    if (migration_version_ == 0) {
      return base::NumberToString(entity_version_);
    } else {
      return base::StringPrintf("%d/%" PRId64, migration_version_,
                                entity_version_);
    }
  }

  int migration_version() const { return migration_version_; }
  int64_t entity_version() const { return entity_version_; }

  void UpdateWithEntity(int64_t other_entity_version) {
    entity_version_ = std::max(entity_version_, other_entity_version);
  }

 private:
  int migration_version_ = 0;
  int64_t entity_version_ = 0;
};

// A filter used during GetUpdates calls to determine what information to
// send back to the client; filtering out old entities and tracking versions to
// use in response progress markers. Note that only the GetUpdatesMessage's
// from_progress_marker is used to determine this; legacy fields are ignored.
class UpdateSieve {
 public:
  UpdateSieve(const sync_pb::GetUpdatesMessage& message,
              const std::map<DataType, int>& server_migration_versions)
      : UpdateSieve(MessageToVersionMap(message, server_migration_versions)) {}
  ~UpdateSieve() = default;

  // Verifies if MIGRATION_DONE should be exercised. It intentionally returns
  // migrations in the order that they were triggered.  Doing it this way
  // allows the client to queue up two migrations in a row, so the second one
  // is received while responding to the first.
  bool ShouldTriggerMigration(
      const std::map<DataType, int>& server_migration_versions,
      std::vector<DataType>* datatypes_to_migrate) const {
    DCHECK(datatypes_to_migrate);
    datatypes_to_migrate->clear();

    for (const auto& [type, request_version] : request_version_map_) {
      const int client_migration_version = request_version.migration_version();

      const int server_migration_version =
          GetServerMigrationVersion(server_migration_versions, type);

      if (client_migration_version < server_migration_version) {
        datatypes_to_migrate->push_back(type);
      }
    }

    return !datatypes_to_migrate->empty();
  }

  // Sets the progress markers in |get_updates_response| based on the highest
  // version between request progress markers and response entities.
  void SetProgressMarkers(
      sync_pb::GetUpdatesResponse* get_updates_response) const {
    for (const auto& [type, response_version] : response_version_map_) {
      sync_pb::DataTypeProgressMarker* new_marker =
          get_updates_response->add_new_progress_marker();
      new_marker->set_data_type_id(GetSpecificsFieldNumberFromDataType(type));
      new_marker->set_token(response_version.ToString());
    }
  }

  // Determines whether the server should send an |entity| to the client as
  // part of a GetUpdatesResponse.
  bool ClientWantsItem(const LoopbackServerEntity& entity) const {
    DataType type = entity.GetDataType();
    auto it = request_version_map_.find(type);
    if (it == request_version_map_.end()) {
      return false;
    }
    DCHECK_NE(0U, response_version_map_.count(type));
    return it->second.entity_version() < entity.GetVersion();
  }

  // Updates internal tracking of max versions to later be used to set response
  // progress markers.
  void UpdateProgressMarker(const LoopbackServerEntity& entity) {
    DCHECK(ClientWantsItem(entity));
    DataType type = entity.GetDataType();
    response_version_map_[type].UpdateWithEntity(entity.GetVersion());
  }

 private:
  using DataTypeToVersionMap = std::map<DataType, ProgressMarkerToken>;

  static UpdateSieve::DataTypeToVersionMap MessageToVersionMap(
      const sync_pb::GetUpdatesMessage& get_updates_message,
      const std::map<DataType, int>& server_migration_versions) {
    DCHECK_GT(get_updates_message.from_progress_marker_size(), 0)
        << "A GetUpdates request must have at least one progress marker.";
    DataTypeToVersionMap request_version_map;

    for (int i = 0; i < get_updates_message.from_progress_marker_size(); i++) {
      const sync_pb::DataTypeProgressMarker& marker =
          get_updates_message.from_progress_marker(i);

      const DataType data_type =
          syncer::GetDataTypeFromSpecificsFieldNumber(marker.data_type_id());
      const int server_migration_version =
          GetServerMigrationVersion(server_migration_versions, data_type);
      const ProgressMarkerToken version =
          marker.token().empty()
              ? ProgressMarkerToken::FromEmpty(server_migration_version)
              : ProgressMarkerToken::FromString(marker.token());

      DCHECK(request_version_map.find(data_type) == request_version_map.end());
      request_version_map[data_type] = version;
    }
    return request_version_map;
  }

  explicit UpdateSieve(const DataTypeToVersionMap request_version_map)
      : request_version_map_(request_version_map),
        response_version_map_(request_version_map) {}

  // The largest versions the client has seen before this request, and is used
  // to filter entities to send back to clients. The values in this map are not
  // updated after being initially set. The presence of a type in this map is a
  // proxy for the desire to receive results about this type.
  const DataTypeToVersionMap request_version_map_;

  // The largest versions seen between client and server, ultimately used to
  // send progress markers back to the client.
  DataTypeToVersionMap response_version_map_;
};

bool SortByVersion(const LoopbackServerEntity* lhs,
                   const LoopbackServerEntity* rhs) {
  return lhs->GetVersion() < rhs->GetVersion();
}

}  // namespace

LoopbackServer::LoopbackServer(const base::FilePath& persistent_file)
    : persistent_file_(persistent_file),
      writer_(
          persistent_file_,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          kHistogramSuffix) {
  DCHECK(!persistent_file_.empty());
  Init();
}

LoopbackServer::~LoopbackServer() {
  FlushToDisk();
}

void LoopbackServer::Init() {
  if (LoadStateFromFile()) {
    return;
  }

  store_birthday_ = base::Time::Now().InMillisecondsSinceUnixEpoch();
  keystore_keys_.push_back(GenerateNewKeystoreKey());

  const bool create_result = CreateDefaultPermanentItems();
  DCHECK(create_result) << "Permanent items were not created successfully.";
}

std::vector<uint8_t> LoopbackServer::GenerateNewKeystoreKey() const {
  std::vector<uint8_t> generated_key(kKeystoreKeyLength);
  base::RandBytes(generated_key);
  return generated_key;
}

bool LoopbackServer::CreatePermanentBookmarkFolder(
    const std::string& server_tag,
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<LoopbackServerEntity> entity =
      PersistentPermanentEntity::CreateNew(
          syncer::BOOKMARKS, server_tag, name,
          DataTypeToProtocolRootTag(syncer::BOOKMARKS));
  if (!entity) {
    return false;
  }

  SaveEntity(std::move(entity));
  return true;
}

bool LoopbackServer::CreateDefaultPermanentItems() {
  // Permanent folders are always required for Bookmarks (hierarchical
  // structure) and Nigori (data stored in permanent root folder).
  const DataTypeSet permanent_folder_types = {syncer::BOOKMARKS,
                                              syncer::NIGORI};

  for (DataType data_type : permanent_folder_types) {
    std::unique_ptr<LoopbackServerEntity> top_level_entity =
        PersistentPermanentEntity::CreateTopLevel(data_type);
    if (!top_level_entity) {
      return false;
    }
    SaveEntity(std::move(top_level_entity));
  }

  return true;
}

void LoopbackServer::UpdateEntityVersion(LoopbackServerEntity* entity) {
  entity->SetVersion(++version_);
}

void LoopbackServer::SaveEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  UpdateEntityVersion(entity.get());
  entities_[entity->GetId()] = std::move(entity);
}

net::HttpStatusCode LoopbackServer::HandleCommand(
    const sync_pb::ClientToServerMessage& message,
    sync_pb::ClientToServerResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response);

  response->Clear();

  if (bag_of_chips_.has_value()) {
    *response->mutable_new_bag_of_chips() = *bag_of_chips_;
  }

  if (message.has_store_birthday() &&
      message.store_birthday() != GetStoreBirthday()) {
    // The birthday provided by the client does not match the authoritative
    // value server-side, which in the absence of client-side bugs means that
    // the birthday was reset (e.g. via ClearServerDataMessage) since the last
    // time the client interacted with the server.
    response->set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  } else {
    bool success = false;
    std::vector<DataType> datatypes_to_migrate;
    DataTypeSet throttled_datatypes_in_request;
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        success = HandleGetUpdatesRequest(
            message.get_updates(), message.store_birthday(),
            message.invalidator_client_id(), response->mutable_get_updates(),
            &datatypes_to_migrate);
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        success = HandleCommitRequest(
            message.commit(), message.invalidator_client_id(),
            response->mutable_commit(), &throttled_datatypes_in_request);
        break;
      case sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA:
        ClearServerData();
        response->mutable_clear_server_data();
        success = true;
        break;
      case sync_pb::ClientToServerMessage::DEPRECATED_3:
      case sync_pb::ClientToServerMessage::DEPRECATED_4:
        NOTREACHED_IN_MIGRATION();
        return net::HTTP_BAD_REQUEST;
    }

    if (success) {
      response->set_error_code(sync_pb::SyncEnums::SUCCESS);
    } else if (!datatypes_to_migrate.empty()) {
      DLOG(WARNING) << "Migration required for " << datatypes_to_migrate.size()
                    << " datatypes";
      response->set_error_code(sync_pb::SyncEnums::MIGRATION_DONE);
      for (DataType type : datatypes_to_migrate) {
        response->add_migrated_data_type_id(
            GetSpecificsFieldNumberFromDataType(type));
      }
    } else if (!throttled_datatypes_in_request.empty()) {
      DLOG(WARNING) << "Throttled datatypes: "
                    << DataTypeSetToDebugString(throttled_datatypes_in_request);
      response->set_error_code(sync_pb::SyncEnums::THROTTLED);
      response->mutable_error()->set_error_type(sync_pb::SyncEnums::THROTTLED);
      for (DataType type : throttled_datatypes_in_request) {
        response->mutable_error()->add_error_data_type_ids(
            syncer::GetSpecificsFieldNumberFromDataType(type));
      }
      // Avoid tests waiting too long after throttling is disabled.
      response->mutable_client_command()->set_throttle_delay_seconds(1);
    } else {
      return net::HTTP_INTERNAL_SERVER_ERROR;
    }
  }

  response->set_store_birthday(GetStoreBirthday());

  ScheduleSaveStateToFile();
  return net::HTTP_OK;
}

void LoopbackServer::EnableStrongConsistencyWithConflictDetectionModel() {
  strong_consistency_model_enabled_ = true;
}

void LoopbackServer::AddNewKeystoreKeyForTesting() {
  keystore_keys_.push_back(GenerateNewKeystoreKey());
}

void LoopbackServer::FlushToDisk() {
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

bool LoopbackServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    const std::string& store_birthday,
    const std::string& invalidator_client_id,
    sync_pb::GetUpdatesResponse* response,
    std::vector<DataType>* datatypes_to_migrate) {
  response->set_changes_remaining(0);

  bool is_initial_bookmark_sync = false;
  for (const sync_pb::DataTypeProgressMarker& marker :
       get_updates.from_progress_marker()) {
    if (GetDataTypeFromSpecificsFieldNumber(marker.data_type_id()) !=
        syncer::BOOKMARKS) {
      continue;
    }
    if (!marker.has_token() || marker.token().empty()) {
      is_initial_bookmark_sync = true;
      break;
    }
  }

  if (is_initial_bookmark_sync) {
    if (!CreatePermanentBookmarkFolder(kBookmarkBarFolderServerTag,
                                       kBookmarkBarFolderName)) {
      return false;
    }
    if (!CreatePermanentBookmarkFolder(kOtherBookmarksFolderServerTag,
                                       kOtherBookmarksFolderName)) {
      return false;
    }
    // This folder is called "Synced Bookmarks" by sync and is renamed
    // "Mobile Bookmarks" by the mobile client UIs.
    if (!CreatePermanentBookmarkFolder(kSyncedBookmarksFolderServerTag,
                                       kSyncedBookmarksFolderName)) {
      return false;
    }
  }

  // It's a protocol-level contract that the birthday should only be empty
  // during the initial sync cycle, which requires all progress markers to be
  // empty. This is also DCHECK-ed on the client, inside syncer_proto_util.cc,
  // but we guard against client-side code changes here.
  if (store_birthday.empty()) {
    for (const sync_pb::DataTypeProgressMarker& marker :
         get_updates.from_progress_marker()) {
      if (!marker.token().empty()) {
        DLOG(WARNING) << "Non-empty progress marker without birthday";
        return false;
      }
    }
  }

  auto sieve = std::make_unique<UpdateSieve>(get_updates, migration_versions_);

  if (sieve->ShouldTriggerMigration(migration_versions_,
                                    datatypes_to_migrate)) {
    DCHECK(!datatypes_to_migrate->empty());
    return false;
  }

  std::vector<const LoopbackServerEntity*> wanted_entities;
  for (const auto& [id, entity] : entities_) {
    if (sieve->ClientWantsItem(*entity)) {
      wanted_entities.push_back(entity.get());
    }
  }

  if (static_cast<int>(wanted_entities.size()) > max_get_updates_batch_size_) {
    response->set_changes_remaining(wanted_entities.size() -
                                    max_get_updates_batch_size_);
    std::partial_sort(wanted_entities.begin(),
                      wanted_entities.begin() + max_get_updates_batch_size_,
                      wanted_entities.end(), SortByVersion);
    wanted_entities.resize(max_get_updates_batch_size_);
  }

  bool send_encryption_keys_based_on_nigori = false;
  for (const LoopbackServerEntity* entity : wanted_entities) {
    sieve->UpdateProgressMarker(*entity);

    sync_pb::SyncEntity* response_entity = response->add_entries();
    entity->SerializeAsProto(response_entity);

    if (entity->GetDataType() == syncer::NIGORI) {
      send_encryption_keys_based_on_nigori =
          response_entity->specifics().nigori().passphrase_type() ==
          sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
    }
  }

  if (send_encryption_keys_based_on_nigori ||
      get_updates.need_encryption_key()) {
    for (const std::vector<uint8_t>& key : keystore_keys_) {
      response->add_encryption_keys(key.data(), key.size());
    }
  }

  sieve->SetProgressMarkers(response);
  // During initial bookmark sync, we create new entities for bookmark permanent
  // folders, and hence we should inform the observers.
  if (is_initial_bookmark_sync && observer_for_tests_) {
    observer_for_tests_->OnCommit({syncer::BOOKMARKS});
  }

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
  syncer::DataType type = GetDataTypeFromSpecifics(client_entity.specifics());
  if (client_entity.deleted()) {
    entity = PersistentTombstoneEntity::CreateFromEntity(client_entity);
    if (entity) {
      DeleteChildren(client_entity.id_string());
    }
  } else if (type == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    CHECK(iter != entities_.end(), base::NotFatalUntil::M130);
    entity = PersistentPermanentEntity::CreateUpdatedNigoriEntity(
        client_entity, *iter->second);
  } else if (type == syncer::BOOKMARKS) {
    // TODO(pvalenzuela): Validate entity's parent ID.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      entity = PersistentBookmarkEntity::CreateUpdatedVersion(
          client_entity, *iter->second, parent_id, client_guid);
    } else {
      entity = PersistentBookmarkEntity::CreateNew(client_entity, parent_id,
                                                   client_guid);
    }
  } else if (type == syncer::PASSWORDS) {
    entity = PersistentUniqueClientEntity::CreateFromEntity(client_entity);
    // If the commit is coming from a legacy client that doesn't support
    // password notes, carry over an existing note backup. The same logic is
    // implemented on the production sync server.
    if (!client_entity.specifics().password().has_encrypted_notes_backup()) {
      EntityMap::const_iterator iter =
          entities_.find(client_entity.id_string());
      if (iter != entities_.end()) {
        const LoopbackServerEntity* server_entity = iter->second.get();
        if (server_entity->GetSpecifics()
                .password()
                .has_encrypted_notes_backup()) {
          sync_pb::EntitySpecifics specifics = entity->GetSpecifics();
          *specifics.mutable_password()->mutable_encrypted_notes_backup() =
              server_entity->GetSpecifics().password().encrypted_notes_backup();
          entity->SetSpecifics(specifics);
        }
      }
    }
  } else {
    entity = PersistentUniqueClientEntity::CreateFromEntity(client_entity);
  }

  if (!entity) {
    return string();
  }

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
  CHECK(iter != entities_.end(), base::NotFatalUntil::M130);
  const LoopbackServerEntity& entity = *iter->second;
  entry_response->set_response_type(response_type_override_
                                        ? response_type_override_.Run(entity)
                                        : sync_pb::CommitResponse::SUCCESS);
  entry_response->set_id_string(entity.GetId());
  entry_response->set_version(entity.GetVersion());
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
  if (entity.GetParentId() == potential_parent_id) {
    return true;
  }

  // Recursively look up the tree.
  return IsChild(entity.GetParentId(), potential_parent_id);
}

void LoopbackServer::DeleteChildren(const string& parent_id) {
  std::vector<sync_pb::SyncEntity> tombstones;
  // Find all the children of |parent_id|.
  for (auto& [id, entity] : entities_) {
    if (IsChild(id, parent_id)) {
      sync_pb::SyncEntity proto;
      entity->SerializeAsProto(&proto);
      tombstones.emplace_back(proto);
    }
  }

  for (sync_pb::SyncEntity& tombstone : tombstones) {
    SaveEntity(PersistentTombstoneEntity::CreateFromEntity(tombstone));
  }
}

bool LoopbackServer::HandleCommitRequest(
    const sync_pb::CommitMessage& commit,
    const std::string& invalidator_client_id,
    sync_pb::CommitResponse* response,
    DataTypeSet* throttled_datatypes_in_request) {
  std::map<string, string> client_to_server_ids;
  string guid = commit.cache_guid();
  DataTypeSet committed_data_types;

  // TODO(pvalenzuela): Add validation of CommitMessage.entries.
  for (const sync_pb::SyncEntity& client_entity : commit.entries()) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->add_entryresponse();

    string parent_id = client_entity.parent_id_string();
    if (client_to_server_ids.find(parent_id) != client_to_server_ids.end()) {
      parent_id = client_to_server_ids[parent_id];
    }

    const DataType entity_data_type =
        GetDataTypeFromSpecifics(client_entity.specifics());
    if (throttled_types_.Has(entity_data_type)) {
      entry_response->set_response_type(sync_pb::CommitResponse::OVER_QUOTA);
      throttled_datatypes_in_request->Put(entity_data_type);
      continue;
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
    CHECK(iter != entities_.end(), base::NotFatalUntil::M130);
    committed_data_types.Put(iter->second->GetDataType());

    if (observer_for_tests_) {
      if (client_entity.deleted() && client_entity.has_deletion_origin()) {
        observer_for_tests_->OnCommittedDeletionOrigin(
            iter->second->GetDataType(), client_entity.deletion_origin());
      }
    }
  }

  if (observer_for_tests_) {
    observer_for_tests_->OnCommit(committed_data_types);
  }

  return throttled_datatypes_in_request->empty();
}

void LoopbackServer::ClearServerData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entities_.clear();
  keystore_keys_.clear();
  store_birthday_ = base::Time::Now().InMillisecondsSinceUnixEpoch();
  base::DeleteFile(persistent_file_);
  Init();
}

void LoopbackServer::DeleteAllEntitiesForDataType(DataType data_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase_if(entities_, [data_type](const auto& id_and_entity) {
    return id_and_entity.second->GetDataType() == data_type;
  });
  ScheduleSaveStateToFile();
}

std::string LoopbackServer::GetStoreBirthday() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::NumberToString(store_birthday_);
}

std::vector<sync_pb::SyncEntity> LoopbackServer::GetSyncEntitiesByDataType(
    DataType data_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<sync_pb::SyncEntity> sync_entities;
  for (const auto& [id, entity] : entities_) {
    if (!(entity->IsDeleted() || entity->IsPermanent()) &&
        entity->GetDataType() == data_type) {
      sync_pb::SyncEntity sync_entity;
      entity->SerializeAsProto(&sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

std::vector<sync_pb::SyncEntity>
LoopbackServer::GetPermanentSyncEntitiesByDataType(DataType data_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<sync_pb::SyncEntity> sync_entities;
  for (const auto& [id, entity] : entities_) {
    if (!entity->IsDeleted() && entity->IsPermanent() &&
        entity->GetDataType() == data_type) {
      sync_pb::SyncEntity sync_entity;
      entity->SerializeAsProto(&sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

base::Value::Dict LoopbackServer::GetEntitiesAsDictForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dictionary;

  // Initialize an empty Value::List for all DataTypes.
  DataTypeSet all_types = DataTypeSet::All();
  for (DataType type : all_types) {
    dictionary.Set(DataTypeToDebugString(type), base::Value::List());
  }

  for (const auto& [id, entity] : entities_) {
    if (entity->IsDeleted() || entity->IsPermanent()) {
      // Tombstones are ignored as they don't represent current data. Folders
      // are also ignored as current verification infrastructure does not
      // consider them.
      continue;
    }

    base::Value::List* list_value =
        dictionary.FindList(DataTypeToDebugString(entity->GetDataType()));
    DCHECK(list_value);

    // TODO(pvalenzuela): Store more data for each entity so additional
    // verification can be performed. One example of additional verification
    // is checking the correctness of the bookmark hierarchy.
    list_value->Append(entity->GetName());
  }

  return dictionary;
}

bool LoopbackServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetDataType() !=
          GetDataTypeFromSpecifics(updated_specifics)) {
    return false;
  }

  LoopbackServerEntity* entity = iter->second.get();
  entity->SetSpecifics(updated_specifics);
  UpdateEntityVersion(entity);

  ScheduleSaveStateToFile();

  return true;
}

bool LoopbackServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetDataType() != syncer::BOOKMARKS ||
      GetDataTypeFromSpecifics(updated_specifics) != syncer::BOOKMARKS) {
    return false;
  }

  PersistentBookmarkEntity* entity =
      static_cast<PersistentBookmarkEntity*>(iter->second.get());

  entity->SetParentId(parent_id);
  entity->SetSpecifics(updated_specifics);
  if (updated_specifics.has_bookmark()) {
    entity->SetName(updated_specifics.bookmark().legacy_canonicalized_title());
  }
  UpdateEntityVersion(entity);

  ScheduleSaveStateToFile();

  return true;
}

void LoopbackServer::SerializeState(sync_pb::LoopbackServerProto* proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proto->set_version(kCurrentLoopbackServerProtoVersion);
  proto->set_store_birthday(store_birthday_);
  proto->set_last_version_assigned(version_);
  for (const std::vector<uint8_t>& key : keystore_keys_) {
    proto->add_keystore_keys(key.data(), key.size());
  }
  for (const auto& [id, entity] : entities_) {
    sync_pb::LoopbackServerEntity* new_entity =
        proto->mutable_entities()->Add();
    entity->SerializeAsLoopbackServerEntity(new_entity);
  }
}

bool LoopbackServer::DeSerializeState(
    const sync_pb::LoopbackServerProto& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(proto.version(), kCurrentLoopbackServerProtoVersion);

  store_birthday_ = proto.store_birthday();
  version_ = proto.last_version_assigned();
  for (int i = 0; i < proto.keystore_keys_size(); ++i) {
    const auto& key = proto.keystore_keys(i);
    keystore_keys_.emplace_back(key.begin(), key.end());
  }
  for (int i = 0; i < proto.entities_size(); ++i) {
    std::unique_ptr<LoopbackServerEntity> entity =
        LoopbackServerEntity::CreateEntityFromProto(proto.entities(i));
    // Silently drop entities that cannot be successfully deserialized.
    if (entity) {
      entities_[proto.entities(i).entity().id_string()] = std::move(entity);
    }
  }

  // Report success regardless of if some entities were dropped.
  return true;
}

std::optional<std::string> LoopbackServer::SerializeData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::LoopbackServerProto proto;
  SerializeState(&proto);
  std::string data;
  if (!proto.SerializeToString(&data)) {
    LOG(ERROR) << "Loopback sync proto could not be serialized";
    return std::nullopt;
  }
  UMA_HISTOGRAM_MEMORY_KB(
      "Sync.Local.FileSizeKB",
      base::saturated_cast<base::Histogram::Sample>(
          base::ClampDiv(base::ClampAdd(data.size(), 512), 1024)));
  return data;
}

bool LoopbackServer::ScheduleSaveStateToFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::CreateDirectory(persistent_file_.DirName())) {
    LOG(ERROR) << "Loopback sync could not create the storage directory.";
    return false;
  }

  writer_.ScheduleWrite(this);
  return true;
}

bool LoopbackServer::LoadStateFromFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensures local sync file can be opened, read, and is not being written to.
  // Also makes sure file will not be written to during serialization.
  base::File state_file(persistent_file_,
                        base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  base::File::Error state_file_error = state_file.error_details();

  if (state_file_error != base::File::FILE_OK) {
    UMA_HISTOGRAM_ENUMERATION("Sync.Local.ReadPlatformFileError",
                              -state_file_error, -base::File::FILE_ERROR_MAX);
    LOG(ERROR) << "Loopback sync cannot read the persistent state file ("
               << persistent_file_ << ") with error "
               << base::File::ErrorToString(state_file_error);
    return false;
  }

  std::string serialized;
  if (base::ReadFileToString(persistent_file_, &serialized)) {
    sync_pb::LoopbackServerProto proto;
    if (serialized.length() > 0 && proto.ParseFromString(serialized)) {
      return DeSerializeState(proto);
    }
    LOG(ERROR) << "Loopback sync cannot parse the persistent state file ("
               << persistent_file_ << ").";
    return false;
  }
  LOG(ERROR) << "Loopback sync cannot read the persistent state file ("
             << persistent_file_ << ").";
  return false;
}

}  // namespace syncer
