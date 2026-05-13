// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_commit_tracker.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace send_tab_to_self {

namespace {
constexpr base::TimeDelta kCommitTimeout = base::Seconds(3);
}  // namespace

SendTabToSelfCommitTracker::PendingCommit::PendingCommit(
    std::string guid,
    base::OnceCallback<void(SendTabToSelfResult)> callback)
    : guid(std::move(guid)), callback(std::move(callback)) {}

SendTabToSelfCommitTracker::PendingCommit::~PendingCommit() = default;

SendTabToSelfCommitTracker::PendingCommit::PendingCommit(PendingCommit&&) =
    default;

SendTabToSelfCommitTracker::PendingCommit&
SendTabToSelfCommitTracker::PendingCommit::operator=(PendingCommit&&) = default;

SendTabToSelfCommitTracker::SendTabToSelfCommitTracker(
    syncer::DataTypeLocalChangeProcessor* change_processor)
    : change_processor_(change_processor) {
  CHECK(change_processor_);
}

SendTabToSelfCommitTracker::~SendTabToSelfCommitTracker() = default;

void SendTabToSelfCommitTracker::TrackCommit(
    const std::string& guid,
    base::OnceCallback<void(SendTabToSelfResult)> callback) {
  if (!callback) {
    return;
  }

  syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::SEND_TAB_TO_SELF, guid);
  pending_commits_.emplace(client_tag_hash,
                           PendingCommit{guid, std::move(callback)});

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SendTabToSelfCommitTracker::HandleTimeout,
                     weak_ptr_factory_.GetWeakPtr(), client_tag_hash),
      kCommitTimeout);
}

void SendTabToSelfCommitTracker::OnIncrementalSyncComplete() {
  base::EraseIf(pending_commits_, [this](auto& pair) {
    if (!change_processor_->IsEntityUnsynced(pair.second.guid)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(pair.second.callback),
                                    SendTabToSelfResult::kSuccess));
      return true;
    }
    return false;
  });
}

void SendTabToSelfCommitTracker::OnCommitErrors(
    const syncer::FailedCommitResponseDataList& error_list) {
  for (const syncer::FailedCommitResponseData& error : error_list) {
    InvokeCallbackAndErase(error.client_tag_hash,
                           SendTabToSelfResult::kFailureCommitAttemptError);
  }
}

void SendTabToSelfCommitTracker::OnCommitAttemptFailed() {
  ClearAllAndInvokeCallbacks(SendTabToSelfResult::kFailureCommitAttemptFailed);
}

void SendTabToSelfCommitTracker::OnSyncDisabled() {
  ClearAllAndInvokeCallbacks(SendTabToSelfResult::kFailureSyncDisabled);
}

void SendTabToSelfCommitTracker::OnEntryRemoved(const std::string& guid) {
  InvokeCallbackAndErase(
      syncer::ClientTagHash::FromUnhashed(syncer::SEND_TAB_TO_SELF, guid),
      SendTabToSelfResult::kFailureEntryRemoved);
}

void SendTabToSelfCommitTracker::OnAllEntriesRemoved() {
  ClearAllAndInvokeCallbacks(SendTabToSelfResult::kFailureEntryRemoved);
}

void SendTabToSelfCommitTracker::InvokeCallbackAndErase(
    const syncer::ClientTagHash& client_tag_hash,
    SendTabToSelfResult result) {
  auto it = pending_commits_.find(client_tag_hash);
  if (it != pending_commits_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(it->second.callback), result));
    pending_commits_.erase(it);
  }
}

void SendTabToSelfCommitTracker::ClearAllAndInvokeCallbacks(
    SendTabToSelfResult result) {
  for (auto& [hash, pending] : pending_commits_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending.callback), result));
  }
  pending_commits_.clear();
}

void SendTabToSelfCommitTracker::HandleTimeout(
    const syncer::ClientTagHash& client_tag_hash) {
  InvokeCallbackAndErase(client_tag_hash,
                         SendTabToSelfResult::kFailureCommitTimeout);
}

}  // namespace send_tab_to_self
