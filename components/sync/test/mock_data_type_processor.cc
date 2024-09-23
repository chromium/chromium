// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_data_type_processor.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

MockDataTypeProcessor::MockDataTypeProcessor() : is_synchronous_(true) {}

MockDataTypeProcessor::~MockDataTypeProcessor() = default;

void MockDataTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> commit_queue) {}

void MockDataTypeProcessor::DisconnectSync() {
  if (!disconnect_callback_.is_null()) {
    std::move(disconnect_callback_).Run();
  }
}

void MockDataTypeProcessor::GetLocalChanges(size_t max_entries,
                                            GetLocalChangesCallback callback) {
  get_local_changes_call_count_++;

  // Truncation may be needed due to |max_entries|.
  CommitRequestDataList remaining_changes;
  if (commit_request_.size() > max_entries) {
    for (size_t i = max_entries; i < commit_request_.size(); ++i) {
      remaining_changes.push_back(std::move(commit_request_[i]));
    }
    commit_request_.resize(max_entries);
  }

  CommitRequestDataList returned_changes = std::move(commit_request_);
  commit_request_ = std::move(remaining_changes);
  std::move(callback).Run(std::move(returned_changes));
}

void MockDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  pending_tasks_.push_back(base::BindOnce(
      &MockDataTypeProcessor::OnCommitCompletedImpl, base::Unretained(this),
      type_state, committed_response_list, error_response_list));
  if (is_synchronous_) {
    RunQueuedTasks();
  }
}

void MockDataTypeProcessor::OnCommitFailed(SyncCommitError commit_error) {
  ++commit_failures_count_;
}

void MockDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    UpdateResponseDataList response_list,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  pending_tasks_.push_back(base::BindOnce(
      &MockDataTypeProcessor::OnUpdateReceivedImpl, base::Unretained(this),
      type_state, std::move(response_list), std::move(gc_directive)));
  if (is_synchronous_) {
    RunQueuedTasks();
  }
}

void MockDataTypeProcessor::SetSynchronousExecution(bool is_synchronous) {
  is_synchronous_ = is_synchronous;
}

void MockDataTypeProcessor::RunQueuedTasks() {
  for (base::OnceClosure& pending_task : pending_tasks_) {
    std::move(pending_task).Run();
  }
  pending_tasks_.clear();
}

std::unique_ptr<CommitRequestData> MockDataTypeProcessor::CommitRequest(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  const std::string server_id =
      HasServerAssignedId(tag_hash)
          ? GetServerAssignedId(tag_hash)
          : base::Uuid::GenerateRandomV4().AsLowercaseString();
  return CommitRequest(tag_hash, specifics, server_id);
}

std::unique_ptr<CommitRequestData> MockDataTypeProcessor::CommitRequest(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    const std::string& server_id) {
  const int64_t base_version = GetBaseVersion(tag_hash);

  auto data = std::make_unique<syncer::EntityData>();
  data->id = server_id;
  data->client_tag_hash = tag_hash;
  data->specifics = specifics;

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data->creation_time = base::Time::UnixEpoch() + base::Days(1);
  data->modification_time = data->creation_time + base::Seconds(base_version);
  data->name = "Name: " + tag_hash.value();

  DCHECK(!data->is_deleted());

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->entity = std::move(data);
  request_data->sequence_number = GetNextSequenceNumber(tag_hash);
  request_data->base_version = base_version;
  request_data->specifics_hash =
      base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()));

  if (specifics.has_bookmark()) {
    request_data->deprecated_bookmark_folder =
        (specifics.bookmark().type() == sync_pb::BookmarkSpecifics::FOLDER);
    request_data->deprecated_bookmark_unique_position =
        UniquePosition::FromProto(specifics.bookmark().unique_position());
  }

  return request_data;
}

std::unique_ptr<CommitRequestData> MockDataTypeProcessor::DeleteRequest(
    const ClientTagHash& tag_hash) {
  const int64_t base_version = GetBaseVersion(tag_hash);

  auto data = std::make_unique<syncer::EntityData>();

  if (HasServerAssignedId(tag_hash)) {
    data->id = GetServerAssignedId(tag_hash);
  }

  data->client_tag_hash = tag_hash;

  // These fields have little or no effect on behavior.  We set them anyway to
  // make the test more realistic.
  data->creation_time = base::Time::UnixEpoch() + base::Days(1);
  data->name = "Name deleted";

  data->modification_time = data->creation_time + base::Seconds(base_version);

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->entity = std::move(data);
  request_data->sequence_number = GetNextSequenceNumber(tag_hash);
  request_data->base_version = base_version;

  pending_deleted_hashes_.insert(tag_hash);

  return request_data;
}

size_t MockDataTypeProcessor::GetNumCommitFailures() const {
  return commit_failures_count_;
}

size_t MockDataTypeProcessor::GetNumUpdateResponses() const {
  return received_update_responses_.size();
}

std::vector<const UpdateResponseData*>
MockDataTypeProcessor::GetNthUpdateResponse(size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> nth_update_responses;
  for (const UpdateResponseData& response : received_update_responses_[n]) {
    nth_update_responses.push_back(&response);
  }
  return nth_update_responses;
}

sync_pb::DataTypeState MockDataTypeProcessor::GetNthUpdateState(
    size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  return type_states_received_on_update_[n];
}

sync_pb::GarbageCollectionDirective MockDataTypeProcessor::GetNthGcDirective(
    size_t n) const {
  DCHECK_LT(n, received_gc_directives_.size());
  return received_gc_directives_[n];
}

size_t MockDataTypeProcessor::GetNumCommitResponses() const {
  return received_commit_responses_.size();
}

CommitResponseDataList MockDataTypeProcessor::GetNthCommitResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return received_commit_responses_[n];
}

sync_pb::DataTypeState MockDataTypeProcessor::GetNthCommitState(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return type_states_received_on_commit_[n];
}

bool MockDataTypeProcessor::HasUpdateResponse(
    const ClientTagHash& tag_hash) const {
  auto it = update_response_items_.find(tag_hash);
  return it != update_response_items_.end();
}

const UpdateResponseData& MockDataTypeProcessor::GetUpdateResponse(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasUpdateResponse(tag_hash));
  auto it = update_response_items_.find(tag_hash);
  return *it->second;
}

bool MockDataTypeProcessor::HasCommitResponse(
    const ClientTagHash& tag_hash) const {
  auto it = commit_response_items_.find(tag_hash);
  return it != commit_response_items_.end();
}

CommitResponseData MockDataTypeProcessor::GetCommitResponse(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasCommitResponse(tag_hash));
  auto it = commit_response_items_.find(tag_hash);
  return it->second;
}

void MockDataTypeProcessor::SetDisconnectCallback(DisconnectCallback callback) {
  disconnect_callback_ = std::move(callback);
}

void MockDataTypeProcessor::SetCommitRequest(
    CommitRequestDataList commit_request) {
  commit_request_ = std::move(commit_request);
}

void MockDataTypeProcessor::AppendCommitRequest(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  const std::string server_id =
      HasServerAssignedId(tag_hash)
          ? GetServerAssignedId(tag_hash)
          : base::Uuid::GenerateRandomV4().AsLowercaseString();
  AppendCommitRequest(tag_hash, specifics, server_id);
}

void MockDataTypeProcessor::AppendCommitRequest(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    const std::string& server_id) {
  commit_request_.push_back(CommitRequest(tag_hash, specifics, server_id));
}

int MockDataTypeProcessor::GetLocalChangesCallCount() const {
  return get_local_changes_call_count_;
}

int MockDataTypeProcessor::GetStoreInvalidationsCallCount() const {
  return store_invalidations_call_count_;
}

void MockDataTypeProcessor::OnCommitCompletedImpl(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  received_commit_responses_.push_back(committed_response_list);
  type_states_received_on_commit_.push_back(type_state);
  for (const CommitResponseData& response : committed_response_list) {
    const ClientTagHash& tag_hash = response.client_tag_hash;
    commit_response_items_.insert(std::make_pair(tag_hash, response));

    if (pending_deleted_hashes_.find(tag_hash) !=
        pending_deleted_hashes_.end()) {
      // Delete request was committed on the server. Erase information we track
      // about the entity.
      sequence_numbers_.erase(tag_hash);
      base_versions_.erase(tag_hash);
      assigned_ids_.erase(tag_hash);
      pending_deleted_hashes_.erase(tag_hash);
    } else {
      // Server wins.  Set the model's base version.
      SetBaseVersion(tag_hash, response.response_version);
      SetServerAssignedId(tag_hash, response.id);
    }
  }
}

void MockDataTypeProcessor::OnUpdateReceivedImpl(
    const sync_pb::DataTypeState& type_state,
    UpdateResponseDataList response_list,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  type_states_received_on_update_.push_back(type_state);
  for (const UpdateResponseData& response : response_list) {
    const ClientTagHash& client_tag_hash = response.entity.client_tag_hash;
    // Server wins.  Set the model's base version.
    SetBaseVersion(client_tag_hash, response.response_version);
    SetServerAssignedId(client_tag_hash, response.entity.id);

    update_response_items_.insert(std::make_pair(client_tag_hash, &response));
  }
  received_update_responses_.push_back(std::move(response_list));
  received_gc_directives_.push_back(
      gc_directive.value_or(sync_pb::GarbageCollectionDirective()));
}

void MockDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  store_invalidations_call_count_++;
}

// Fetches the sequence number as of the most recent update request.
int64_t MockDataTypeProcessor::GetCurrentSequenceNumber(
    const ClientTagHash& tag_hash) const {
  auto it = sequence_numbers_.find(tag_hash);
  if (it == sequence_numbers_.end()) {
    return 0;
  }
  return it->second;
}

// The model thread should be sending us items with strictly increasing
// sequence numbers.  Here's where we emulate that behavior.
int64_t MockDataTypeProcessor::GetNextSequenceNumber(
    const ClientTagHash& tag_hash) {
  int64_t sequence_number = GetCurrentSequenceNumber(tag_hash);
  sequence_number++;
  sequence_numbers_[tag_hash] = sequence_number;
  return sequence_number;
}

int64_t MockDataTypeProcessor::GetBaseVersion(
    const ClientTagHash& tag_hash) const {
  auto it = base_versions_.find(tag_hash);
  if (it == base_versions_.end()) {
    return kUncommittedVersion;
  }
  return it->second;
}

void MockDataTypeProcessor::SetBaseVersion(const ClientTagHash& tag_hash,
                                           int64_t version) {
  base_versions_[tag_hash] = version;
}

bool MockDataTypeProcessor::HasServerAssignedId(
    const ClientTagHash& tag_hash) const {
  return assigned_ids_.find(tag_hash) != assigned_ids_.end();
}

const std::string& MockDataTypeProcessor::GetServerAssignedId(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasServerAssignedId(tag_hash));
  return assigned_ids_.find(tag_hash)->second;
}

void MockDataTypeProcessor::SetServerAssignedId(const ClientTagHash& tag_hash,
                                                const std::string& id) {
  assigned_ids_[tag_hash] = id;
}

}  // namespace syncer
