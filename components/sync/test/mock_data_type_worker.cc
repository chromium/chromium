// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_data_type_worker.h"

#include <map>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::UnorderedElementsAreArray;

class ForwardingCommitQueue : public CommitQueue {
 public:
  explicit ForwardingCommitQueue(base::WeakPtr<CommitQueue> other)
      : other_(std::move(other)) {}
  ~ForwardingCommitQueue() override = default;

  void NudgeForCommit() override {
    if (other_) {
      other_->NudgeForCommit();
    }
  }

 private:
  const base::WeakPtr<CommitQueue> other_;
};

}  // namespace

// static
std::unique_ptr<MockDataTypeWorker>
MockDataTypeWorker::CreateWorkerAndConnectSync(
    std::unique_ptr<DataTypeActivationResponse> context) {
  // WrapUnique() used because the constructor is private.
  auto worker = base::WrapUnique(new MockDataTypeWorker(std::move(context)));
  worker->processor_->ConnectSync(std::make_unique<ForwardingCommitQueue>(
      worker->weak_ptr_factory_.GetWeakPtr()));
  return worker;
}

MockDataTypeWorker::MockDataTypeWorker(
    std::unique_ptr<DataTypeActivationResponse> context)
    : data_type_state_(context->data_type_state),
      processor_(std::move(context->type_processor)) {}

MockDataTypeWorker::~MockDataTypeWorker() = default;

void MockDataTypeWorker::NudgeForCommit() {
  if (get_local_changes_upon_nudge_enabled_) {
    processor_->GetLocalChanges(
        INT_MAX, base::BindRepeating(&MockDataTypeWorker::LocalChangesReceived,
                                     weak_ptr_factory_.GetWeakPtr()));
    // Processors often use a proxy object to communicate with the worker, so it
    // is necessary to process posted tasks.
    base::RunLoop().RunUntilIdle();
  }
}

void MockDataTypeWorker::LocalChangesReceived(
    CommitRequestDataList&& commit_request) {
  if (commit_request.empty()) {
    return;
  }
  // Verify that all request entities have valid id, version combinations.
  for (const std::unique_ptr<CommitRequestData>& commit_request_data :
       commit_request) {
    EXPECT_TRUE(commit_request_data->base_version == -1 ||
                !commit_request_data->entity->id.empty());
  }
  pending_commits_.push_back(std::move(commit_request));
}

size_t MockDataTypeWorker::GetNumPendingCommits() const {
  return pending_commits_.size();
}

std::vector<const CommitRequestData*> MockDataTypeWorker::GetNthPendingCommit(
    size_t n) const {
  EXPECT_LT(n, GetNumPendingCommits());
  if (n >= GetNumPendingCommits()) {
    return {};
  }
  std::vector<const CommitRequestData*> nth_pending_commits;
  for (const std::unique_ptr<CommitRequestData>& request_data :
       pending_commits_[n]) {
    nth_pending_commits.push_back(request_data.get());
  }
  return nth_pending_commits;
}

bool MockDataTypeWorker::HasPendingCommitForHash(
    const ClientTagHash& tag_hash) const {
  for (const CommitRequestDataList& commit : pending_commits_) {
    for (const std::unique_ptr<CommitRequestData>& data : commit) {
      if (data && data->entity->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }
  return false;
}

const CommitRequestData* MockDataTypeWorker::GetLatestPendingCommitForHash(
    const ClientTagHash& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (const CommitRequestDataList& commit : base::Reversed(pending_commits_)) {
    for (const std::unique_ptr<CommitRequestData>& data : commit) {
      if (data && data->entity->client_tag_hash == tag_hash) {
        return data.get();
      }
    }
  }
  NOTREACHED_IN_MIGRATION()
      << "Could not find commit for tag hash " << tag_hash << ".";
  return nullptr;
}

void MockDataTypeWorker::VerifyNthPendingCommit(
    size_t n,
    const std::vector<ClientTagHash>& tag_hashes,
    const std::vector<sync_pb::EntitySpecifics>& specifics_list) {
  ASSERT_EQ(tag_hashes.size(), specifics_list.size());
  std::map<ClientTagHash, sync_pb::EntitySpecifics> tag_hash_to_specifics;
  for (size_t i = 0; i < tag_hashes.size(); i++) {
    tag_hash_to_specifics[tag_hashes[i]] = specifics_list[i];
  }

  std::vector<const CommitRequestData*> list = GetNthPendingCommit(n);
  ASSERT_EQ(tag_hashes.size(), list.size());

  for (const CommitRequestData* data : list) {
    ASSERT_TRUE(data);
    const EntityData& actual_entity = *data->entity;

    ASSERT_TRUE(tag_hash_to_specifics.count(actual_entity.client_tag_hash));
    EXPECT_EQ(tag_hash_to_specifics[actual_entity.client_tag_hash]
                  .SerializeAsString(),
              actual_entity.specifics.SerializeAsString());
  }
}

void MockDataTypeWorker::VerifyPendingCommits(
    const std::vector<std::vector<ClientTagHash>>& tag_hashes) {
  ASSERT_EQ(tag_hashes.size(), GetNumPendingCommits());
  for (size_t i = 0; i < tag_hashes.size(); i++) {
    std::vector<ClientTagHash> actual_hashes;
    for (const CommitRequestData* data : GetNthPendingCommit(i)) {
      ASSERT_TRUE(data);
      actual_hashes.push_back(data->entity->client_tag_hash);
    }
    EXPECT_THAT(actual_hashes, UnorderedElementsAreArray(tag_hashes[i]));
  }
}

void MockDataTypeWorker::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  data_type_state_ = data_type_state;
}

void MockDataTypeWorker::UpdateFromServer() {
  UpdateFromServer(UpdateResponseDataList());
}

void MockDataTypeWorker::UpdateFromServer(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  UpdateFromServer(tag_hash, specifics, 1);
}

void MockDataTypeWorker::UpdateFromServer(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset) {
  UpdateFromServer(tag_hash, specifics, version_offset,
                   data_type_state_.encryption_key_name());
}

void MockDataTypeWorker::UpdateFromServer(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset,
    const std::string& ekn) {
  UpdateResponseDataList updates;
  updates.push_back(
      GenerateUpdateData(tag_hash, specifics, version_offset, ekn));
  UpdateFromServer(std::move(updates));
}

void MockDataTypeWorker::UpdateFromServer(
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  data_type_state_.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  processor_->OnUpdateReceived(data_type_state_, std::move(updates),
                               gc_directive);
  // Processors often use a proxy object to communicate with the worker, so it
  // is necessary to process posted tasks.
  base::RunLoop().RunUntilIdle();
}

syncer::UpdateResponseData MockDataTypeWorker::GenerateUpdateData(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset,
    const std::string& ekn) {
  // Overwrite the existing server version if this is the new highest version.
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  syncer::EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.specifics = specifics;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::Days(1);
  data.modification_time = data.creation_time + base::Seconds(version);
  data.name = data.specifics.has_encrypted()
                  ? "encrypted"
                  : data.specifics.preference().name();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  response_data.response_version = version;
  response_data.encryption_key_name = ekn;

  return response_data;
}

syncer::UpdateResponseData MockDataTypeWorker::GenerateUpdateData(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  return GenerateUpdateData(tag_hash, specifics, 1,
                            data_type_state_.encryption_key_name());
}

syncer::UpdateResponseData MockDataTypeWorker::GenerateSharedUpdateData(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    const std::string& collaboration_id) {
  syncer::UpdateResponseData response_data =
      GenerateUpdateData(tag_hash, specifics);
  response_data.entity.collaboration_id = collaboration_id;
  return response_data;
}

syncer::UpdateResponseData MockDataTypeWorker::GenerateTypeRootUpdateData(
    const DataType& data_type) {
  syncer::EntityData data;
  data.id = syncer::DataTypeToProtocolRootTag(data_type);
  data.legacy_parent_id = "r";
  data.server_defined_unique_tag = syncer::DataTypeToProtocolRootTag(data_type);
  syncer::AddDefaultFieldValue(data_type, &data.specifics);
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch();
  data.modification_time = base::Time::UnixEpoch();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseData MockDataTypeWorker::GenerateTombstoneUpdateData(
    const ClientTagHash& tag_hash) {
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + 1;
  SetServerVersion(tag_hash, version);

  syncer::EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::Days(1);
  data.modification_time = data.creation_time + base::Seconds(version);
  data.name = "Name Non Unique";

  UpdateResponseData response_data;
  response_data.entity = std::move(data);
  response_data.response_version = version;
  response_data.encryption_key_name = data_type_state_.encryption_key_name();
  return response_data;
}

void MockDataTypeWorker::TombstoneFromServer(const ClientTagHash& tag_hash) {
  UpdateResponseDataList updates;
  updates.push_back(GenerateTombstoneUpdateData(tag_hash));
  UpdateFromServer(std::move(updates));
}

void MockDataTypeWorker::AckOnePendingCommit() {
  AckOnePendingCommit(1);
}

void MockDataTypeWorker::AckOnePendingCommit(int64_t version_offset) {
  CommitResponseDataList list;
  ASSERT_FALSE(pending_commits_.empty());
  for (const std::unique_ptr<CommitRequestData>& data :
       pending_commits_.front()) {
    list.push_back(SuccessfulCommitResponse(*data, version_offset));
  }
  pending_commits_.pop_front();
  processor_->OnCommitCompleted(
      data_type_state_, list,
      /*error_response_list=*/FailedCommitResponseDataList());
  // Processors often use a proxy object to communicate with the worker, so it
  // is necessary to process posted tasks.
  base::RunLoop().RunUntilIdle();
}

void MockDataTypeWorker::FailOneCommit() {
  FailedCommitResponseDataList list;
  ASSERT_FALSE(pending_commits_.empty());
  for (const std::unique_ptr<CommitRequestData>& data :
       pending_commits_.front()) {
    list.push_back(FailedCommitResponse(*data));
  }
  pending_commits_.pop_front();
  processor_->OnCommitCompleted(
      data_type_state_,
      /*committed_response_list=*/CommitResponseDataList(), list);
  // Processors often use a proxy object to communicate with the worker, so it
  // is necessary to process posted tasks.
  base::RunLoop().RunUntilIdle();
}

void MockDataTypeWorker::FailFullCommitRequest() {
  pending_commits_.pop_front();
  processor_->OnCommitFailed(SyncCommitError::kBadServerResponse);
  // Processors often use a proxy object to communicate with the worker, so it
  // is necessary to process posted tasks.
  base::RunLoop().RunUntilIdle();
}

CommitResponseData MockDataTypeWorker::SuccessfulCommitResponse(
    const CommitRequestData& request_data,
    int64_t version_offset) {
  const EntityData& entity = *request_data.entity;
  const ClientTagHash& client_tag_hash = entity.client_tag_hash;

  CommitResponseData response_data;

  if (request_data.base_version == kUncommittedVersion) {
    // Server assigns new ID to newly committed items.
    DCHECK(entity.id.empty());
    response_data.id = GenerateId(client_tag_hash);
  } else {
    // Otherwise we reuse the ID from the request.
    response_data.id = entity.id;
  }

  response_data.client_tag_hash = client_tag_hash;
  response_data.sequence_number = request_data.sequence_number;
  response_data.specifics_hash = request_data.specifics_hash;

  int64_t old_version = GetServerVersion(client_tag_hash);
  int64_t new_version = old_version + version_offset;
  if (new_version > old_version) {
    SetServerVersion(client_tag_hash, new_version);
  }
  response_data.response_version = new_version;

  return response_data;
}

FailedCommitResponseData MockDataTypeWorker::FailedCommitResponse(
    const CommitRequestData& request_data) {
  const EntityData& entity = *request_data.entity;

  FailedCommitResponseData response_data;
  // We reuse the |client_tag_hash| from the request.
  response_data.client_tag_hash = entity.client_tag_hash;

  response_data.response_type = sync_pb::CommitResponse::TRANSIENT_ERROR;

  return response_data;
}

void MockDataTypeWorker::UpdateWithEncryptionKey(const std::string& ekn) {
  UpdateWithEncryptionKey(ekn, UpdateResponseDataList());
}

void MockDataTypeWorker::UpdateWithEncryptionKey(
    const std::string& ekn,
    UpdateResponseDataList update) {
  data_type_state_.set_encryption_key_name(ekn);
  UpdateFromServer(std::move(update));
}

void MockDataTypeWorker::DisableGetLocalChangesUponNudge() {
  get_local_changes_upon_nudge_enabled_ = false;
}

std::string MockDataTypeWorker::GenerateId(const ClientTagHash& tag_hash) {
  return "FakeId:" + tag_hash.value();
}

int64_t MockDataTypeWorker::GetServerVersion(const ClientTagHash& tag_hash) {
  auto it = server_versions_.find(tag_hash);
  if (it == server_versions_.end()) {
    return 0;
  }
  return it->second;
}

void MockDataTypeWorker::SetServerVersion(const ClientTagHash& tag_hash,
                                          int64_t version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer
