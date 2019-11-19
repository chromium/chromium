// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/mock_model_type_processor.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/hash/sha1.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/commit_queue.h"

namespace syncer {

MockModelTypeProcessor::MockModelTypeProcessor() : is_synchronous_(true) {}

MockModelTypeProcessor::~MockModelTypeProcessor() {}

void MockModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> commit_queue) {
  NOTREACHED();
}

void MockModelTypeProcessor::DisconnectSync() {
  if (!disconnect_callback_.is_null()) {
    disconnect_callback_.Run();
  }
}

void MockModelTypeProcessor::GetLocalChanges(size_t max_entries,
                                             GetLocalChangesCallback callback) {
  DCHECK_LE(commit_request_.size(), max_entries);
  get_local_changes_call_count_++;
  std::move(callback).Run(std::move(commit_request_));
  commit_request_.clear();
}

void MockModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {
  pending_tasks_.push_back(
      base::BindOnce(&MockModelTypeProcessor::OnCommitCompletedImpl,
                     base::Unretained(this), type_state, response_list));
  if (is_synchronous_)
    RunQueuedTasks();
}

void MockModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList response_list) {
  pending_tasks_.push_back(base::BindOnce(
      &MockModelTypeProcessor::OnUpdateReceivedImpl, base::Unretained(this),
      type_state, std::move(response_list)));
  if (is_synchronous_)
    RunQueuedTasks();
}

void MockModelTypeProcessor::SetSynchronousExecution(bool is_synchronous) {
  is_synchronous_ = is_synchronous;
}

void MockModelTypeProcessor::RunQueuedTasks() {
  for (auto it = pending_tasks_.begin(); it != pending_tasks_.end(); ++it) {
    std::move(*it).Run();
  }
  pending_tasks_.clear();
}

std::unique_ptr<CommitRequestData> MockModelTypeProcessor::CommitRequest(
    const ClientTagHash& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  const int64_t base_version = GetBaseVersion(tag_hash);

  auto data = std::make_unique<syncer::EntityData>();

  if (HasServerAssignedId(tag_hash)) {
    data->id = GetServerAssignedId(tag_hash);
  }

  data->client_tag_hash = tag_hash;
  data->specifics = specifics;

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data->creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data->modification_time =
      data->creation_time + base::TimeDelta::FromSeconds(base_version);
  data->name = "Name: " + tag_hash.value();

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->entity = std::move(data);
  request_data->sequence_number = GetNextSequenceNumber(tag_hash);
  request_data->base_version = base_version;
  base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()),
                     &request_data->specifics_hash);

  return request_data;
}

std::unique_ptr<CommitRequestData> MockModelTypeProcessor::DeleteRequest(
    const ClientTagHash& tag_hash) {
  const int64_t base_version = GetBaseVersion(tag_hash);

  auto data = std::make_unique<syncer::EntityData>();

  if (HasServerAssignedId(tag_hash)) {
    data->id = GetServerAssignedId(tag_hash);
  }

  data->client_tag_hash = tag_hash;

  // These fields have little or no effect on behavior.  We set them anyway to
  // make the test more realistic.
  data->creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data->name = "Name deleted";

  data->modification_time =
      data->creation_time + base::TimeDelta::FromSeconds(base_version);

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->entity = std::move(data);
  request_data->sequence_number = GetNextSequenceNumber(tag_hash);
  request_data->base_version = base_version;

  pending_deleted_hashes_.insert(tag_hash);

  return request_data;
}

size_t MockModelTypeProcessor::GetNumUpdateResponses() const {
  return received_update_responses_.size();
}

std::vector<const UpdateResponseData*>
MockModelTypeProcessor::GetNthUpdateResponse(size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> nth_update_responses;
  for (const std::unique_ptr<UpdateResponseData>& response :
       received_update_responses_[n]) {
    nth_update_responses.push_back(response.get());
  }
  return nth_update_responses;
}

sync_pb::ModelTypeState MockModelTypeProcessor::GetNthUpdateState(
    size_t n) const {
  DCHECK_LT(n, GetNumUpdateResponses());
  return type_states_received_on_update_[n];
}

size_t MockModelTypeProcessor::GetNumCommitResponses() const {
  return received_commit_responses_.size();
}

CommitResponseDataList MockModelTypeProcessor::GetNthCommitResponse(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return received_commit_responses_[n];
}

sync_pb::ModelTypeState MockModelTypeProcessor::GetNthCommitState(
    size_t n) const {
  DCHECK_LT(n, GetNumCommitResponses());
  return type_states_received_on_commit_[n];
}

bool MockModelTypeProcessor::HasUpdateResponse(
    const ClientTagHash& tag_hash) const {
  auto it = update_response_items_.find(tag_hash);
  return it != update_response_items_.end();
}

const UpdateResponseData& MockModelTypeProcessor::GetUpdateResponse(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasUpdateResponse(tag_hash));
  auto it = update_response_items_.find(tag_hash);
  return *it->second;
}

bool MockModelTypeProcessor::HasCommitResponse(
    const ClientTagHash& tag_hash) const {
  auto it = commit_response_items_.find(tag_hash);
  return it != commit_response_items_.end();
}

CommitResponseData MockModelTypeProcessor::GetCommitResponse(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasCommitResponse(tag_hash));
  auto it = commit_response_items_.find(tag_hash);
  return it->second;
}

void MockModelTypeProcessor::SetDisconnectCallback(
    const DisconnectCallback& callback) {
  disconnect_callback_ = callback;
}

void MockModelTypeProcessor::SetCommitRequest(
    CommitRequestDataList commit_request) {
  commit_request_ = std::move(commit_request);
}

int MockModelTypeProcessor::GetLocalChangesCallCount() const {
  return get_local_changes_call_count_;
}

void MockModelTypeProcessor::OnCommitCompletedImpl(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {
  received_commit_responses_.push_back(response_list);
  type_states_received_on_commit_.push_back(type_state);
  for (auto it = response_list.begin(); it != response_list.end(); ++it) {
    const ClientTagHash& tag_hash = it->client_tag_hash;
    commit_response_items_.insert(std::make_pair(tag_hash, *it));

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
      SetBaseVersion(tag_hash, it->response_version);
      SetServerAssignedId(tag_hash, it->id);
    }
  }
}

void MockModelTypeProcessor::OnUpdateReceivedImpl(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList response_list) {
  type_states_received_on_update_.push_back(type_state);
  for (auto it = response_list.begin(); it != response_list.end(); ++it) {
    const ClientTagHash& client_tag_hash = (*it)->entity->client_tag_hash;
    // Server wins.  Set the model's base version.
    SetBaseVersion(client_tag_hash, (*it)->response_version);
    SetServerAssignedId(client_tag_hash, (*it)->entity->id);

    update_response_items_.insert(std::make_pair(client_tag_hash, it->get()));
  }
  received_update_responses_.push_back(std::move(response_list));
}

// Fetches the sequence number as of the most recent update request.
int64_t MockModelTypeProcessor::GetCurrentSequenceNumber(
    const ClientTagHash& tag_hash) const {
  auto it = sequence_numbers_.find(tag_hash);
  if (it == sequence_numbers_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

// The model thread should be sending us items with strictly increasing
// sequence numbers.  Here's where we emulate that behavior.
int64_t MockModelTypeProcessor::GetNextSequenceNumber(
    const ClientTagHash& tag_hash) {
  int64_t sequence_number = GetCurrentSequenceNumber(tag_hash);
  sequence_number++;
  sequence_numbers_[tag_hash] = sequence_number;
  return sequence_number;
}

int64_t MockModelTypeProcessor::GetBaseVersion(
    const ClientTagHash& tag_hash) const {
  auto it = base_versions_.find(tag_hash);
  if (it == base_versions_.end()) {
    return kUncommittedVersion;
  } else {
    return it->second;
  }
}

void MockModelTypeProcessor::SetBaseVersion(const ClientTagHash& tag_hash,
                                            int64_t version) {
  base_versions_[tag_hash] = version;
}

bool MockModelTypeProcessor::HasServerAssignedId(
    const ClientTagHash& tag_hash) const {
  return assigned_ids_.find(tag_hash) != assigned_ids_.end();
}

const std::string& MockModelTypeProcessor::GetServerAssignedId(
    const ClientTagHash& tag_hash) const {
  DCHECK(HasServerAssignedId(tag_hash));
  return assigned_ids_.find(tag_hash)->second;
}

void MockModelTypeProcessor::SetServerAssignedId(const ClientTagHash& tag_hash,
                                                 const std::string& id) {
  assigned_ids_[tag_hash] = id;
}

}  // namespace syncer
