// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COMMIT_TRACKER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COMMIT_TRACKER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/client_tag_hash.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
struct FailedCommitResponseData;
using FailedCommitResponseDataList = std::vector<FailedCommitResponseData>;
}  // namespace syncer

namespace send_tab_to_self {

// Encapsulates tracking of pending commits for SendTabToSelf entries.
// Monitors the DataTypeLocalChangeProcessor to determine when an entity
// has been successfully committed to the Sync server, or handles timeouts
// and failure notifications.
class SendTabToSelfCommitTracker {
 public:
  // `change_processor` must not be null and must outlive this object.
  explicit SendTabToSelfCommitTracker(
      syncer::DataTypeLocalChangeProcessor* change_processor);

  SendTabToSelfCommitTracker(const SendTabToSelfCommitTracker&) = delete;
  SendTabToSelfCommitTracker& operator=(const SendTabToSelfCommitTracker&) =
      delete;

  ~SendTabToSelfCommitTracker();

  // Starts tracking a commit for `guid`. `callback` will be invoked when
  // the commit succeeds, fails, or times out.
  void TrackCommit(const std::string& guid,
                   base::OnceCallback<void(SendTabToSelfResult)> callback);

  // Checks if any tracked entities are no longer unsynced, indicating
  // successful commit.
  void OnIncrementalSyncComplete();

  // Called when sync reports commit errors for specific client tags.
  void OnCommitErrors(const syncer::FailedCommitResponseDataList& error_list);

  // Called when sync reports a general commit failure.
  void OnCommitAttemptFailed();

  // Called when sync is disabled. Clears all pending commits with a failure
  // result.
  void OnSyncDisabled();

  // Called when an entry is removed before its commit completes.
  void OnEntryRemoved(const std::string& guid);

  // Called when all entries are removed (bulk deletion).
  void OnAllEntriesRemoved();

 private:
  void InvokeCallbackAndErase(const syncer::ClientTagHash& client_tag_hash,
                              SendTabToSelfResult result);
  void ClearAllAndInvokeCallbacks(SendTabToSelfResult result);

  void HandleTimeout(const syncer::ClientTagHash& client_tag_hash);

  struct PendingCommit {
    PendingCommit(std::string guid,
                  base::OnceCallback<void(SendTabToSelfResult)> callback);
    ~PendingCommit();
    PendingCommit(const PendingCommit&) = delete;
    PendingCommit& operator=(const PendingCommit&) = delete;
    PendingCommit(PendingCommit&&);
    PendingCommit& operator=(PendingCommit&&);

    std::string guid;
    base::OnceCallback<void(SendTabToSelfResult)> callback;
  };

  const raw_ptr<syncer::DataTypeLocalChangeProcessor> change_processor_;

  // Callbacks waiting for a commit response from the Sync server.
  // The key is the ClientTagHash of the SendTabToSelf entry.
  base::flat_map<syncer::ClientTagHash, PendingCommit> pending_commits_;

  base::WeakPtrFactory<SendTabToSelfCommitTracker> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COMMIT_TRACKER_H_
