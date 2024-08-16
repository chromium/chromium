// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/single_type_mock_server.h"

#include "components/sync/base/time.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

using google::protobuf::RepeatedPtrField;

namespace syncer {

SingleTypeMockServer::SingleTypeMockServer(DataType type)
    : type_(type),
      type_root_id_(DataTypeToProtocolRootTag(type)),
      progress_marker_token_("non_null_progress_token") {}

SingleTypeMockServer::~SingleTypeMockServer() = default;

sync_pb::SyncEntity SingleTypeMockServer::TypeRootUpdate() {
  sync_pb::SyncEntity entity;

  entity.set_id_string(type_root_id_);
  entity.set_parent_id_string("r");
  entity.set_version(1000);
  entity.set_ctime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_mtime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_server_defined_unique_tag(DataTypeToProtocolRootTag(type_));
  entity.set_deleted(false);
  AddDefaultFieldValue(type_, entity.mutable_specifics());

  return entity;
}

sync_pb::SyncEntity SingleTypeMockServer::UpdateFromServer(
    int64_t version_offset,
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  sync_pb::SyncEntity entity;

  entity.set_id_string(GenerateId(tag_hash));
  entity.set_parent_id_string(type_root_id_);
  entity.set_version(version);
  entity.set_client_tag_hash(tag_hash.value());
  entity.set_deleted(false);
  entity.mutable_specifics()->CopyFrom(specifics);

  // Unimportant fields, set for completeness only.
  base::Time ctime = base::Time::UnixEpoch() + base::Days(1);
  base::Time mtime = ctime + base::Seconds(version);
  entity.set_ctime(TimeToProtoTime(ctime));
  entity.set_mtime(TimeToProtoTime(mtime));
  entity.set_name("Name: " + tag_hash.value());

  return entity;
}

sync_pb::SyncEntity SingleTypeMockServer::UpdateFromServer(
    int64_t version_offset,
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    const std::string& collaboration_id) {
  sync_pb::SyncEntity entity =
      UpdateFromServer(version_offset, tag_hash, specifics);
  entity.mutable_collaboration()->set_collaboration_id(collaboration_id);
  return entity;
}

sync_pb::SyncEntity SingleTypeMockServer::TombstoneFromServer(
    int64_t version_offset,
    const ClientTagHash& tag_hash) {
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  sync_pb::SyncEntity entity;

  entity.set_id_string(GenerateId(tag_hash));
  entity.set_parent_id_string(type_root_id_);
  entity.set_version(version);
  entity.set_client_tag_hash(tag_hash.value());
  entity.set_deleted(true);
  AddDefaultFieldValue(type_, entity.mutable_specifics());

  // Unimportant fields, set for completeness only.
  base::Time ctime = base::Time::UnixEpoch() + base::Days(1);
  base::Time mtime = ctime + base::Seconds(version);
  entity.set_ctime(TimeToProtoTime(ctime));
  entity.set_mtime(TimeToProtoTime(mtime));
  entity.set_name("Tombstone");

  return entity;
}

sync_pb::ClientToServerResponse SingleTypeMockServer::DoSuccessfulCommit(
    const sync_pb::ClientToServerMessage& message) {
  commit_messages_.push_back(message);

  sync_pb::ClientToServerResponse response;
  sync_pb::CommitResponse* commit_response = response.mutable_commit();

  const RepeatedPtrField<sync_pb::SyncEntity>& entries =
      message.commit().entries();
  for (const sync_pb::SyncEntity& entry : entries) {
    const ClientTagHash tag_hash =
        ClientTagHash::FromHashed(entry.client_tag_hash());

    committed_items_[tag_hash] = entry;

    // Every commit increments the version number.
    int64_t version = GetServerVersion(tag_hash);
    version++;
    SetServerVersion(tag_hash, version);

    sync_pb::CommitResponse_EntryResponse* entryresponse =
        commit_response->add_entryresponse();
    entryresponse->set_response_type(sync_pb::CommitResponse::SUCCESS);
    entryresponse->set_id_string(GenerateId(tag_hash));
    entryresponse->set_version(version);
    entryresponse->set_mtime(entry.mtime());
  }

  return response;
}

size_t SingleTypeMockServer::GetNumCommitMessages() const {
  return commit_messages_.size();
}

const sync_pb::ClientToServerMessage& SingleTypeMockServer::GetNthCommitMessage(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitMessages());
  return commit_messages_[n];
}

bool SingleTypeMockServer::HasCommitEntity(
    const ClientTagHash& tag_hash) const {
  return committed_items_.find(tag_hash) != committed_items_.end();
}

sync_pb::SyncEntity SingleTypeMockServer::GetLastCommittedEntity(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasCommitEntity(tag_hash));
  return committed_items_.find(tag_hash)->second;
}

sync_pb::DataTypeProgressMarker SingleTypeMockServer::GetProgress() const {
  sync_pb::DataTypeProgressMarker progress;
  progress.set_data_type_id(type_);
  progress.set_token(progress_marker_token_);
  if (return_gc_directive_version_watermark_) {
    progress.mutable_gc_directive()->set_type(
        sync_pb::GarbageCollectionDirective::VERSION_WATERMARK);
    // The value of version watermark is not used, so just provide some number.
    progress.mutable_gc_directive()->set_version_watermark(123);
  }
  if (SharedTypes().Has(type_)) {
    // Always provide gc_directive for active collaborations for the shared
    // types.
    sync_pb::GarbageCollectionDirective::CollaborationGarbageCollection*
        collaboration_gc =
            progress.mutable_gc_directive()->mutable_collaboration_gc();
    for (const std::string& collaboration_id : active_collaborations_) {
      collaboration_gc->add_active_collaboration_ids(collaboration_id);
    }
  }
  return progress;
}

sync_pb::DataTypeContext SingleTypeMockServer::GetContext() const {
  return sync_pb::DataTypeContext();
}

void SingleTypeMockServer::SetProgressMarkerToken(const std::string& token) {
  progress_marker_token_ = token;
}

std::string SingleTypeMockServer::GenerateId(const ClientTagHash& tag_hash) {
  return "FakeId:" + tag_hash.value();
}

int64_t SingleTypeMockServer::GetServerVersion(
    const ClientTagHash& tag_hash) const {
  auto it = server_versions_.find(tag_hash);
  // Server versions do not necessarily start at 1 or 0.
  if (it == server_versions_.end()) {
    return 2048;
  }
  return it->second;
}

void SingleTypeMockServer::SetServerVersion(const ClientTagHash& tag_hash,
                                            int64_t version) {
  server_versions_[tag_hash] = version;
}

void SingleTypeMockServer::SetReturnGcDirectiveVersionWatermark(
    bool return_gc_directive) {
  return_gc_directive_version_watermark_ = return_gc_directive;
}

void SingleTypeMockServer::AddCollaboration(
    const std::string& collaboration_id) {
  // Should be used for the shared types only.
  CHECK(SharedTypes().Has(type_));
  active_collaborations_.insert(collaboration_id);
}

void SingleTypeMockServer::RemoveCollaboration(
    const std::string& collaboration_id) {
  active_collaborations_.erase(collaboration_id);
}

}  // namespace syncer
