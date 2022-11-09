// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_

#include <memory>
#include <vector>
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/feed_action.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/public/logging_parameters.h"
#include "components/feed/core/v2/types.h"
#include "components/offline_pages/task/task.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feed {
class FeedStream;

// Uploads user actions and returns a consistency token to be used for loading
// the stream later.
//
// Repeat while pending actions remain to be processed:
//   1. Gather as many actions as can fit in the request size limit. If we
//    encounter stale actions that should be erased, set those aside.
//   2. Increment upload_attempt_count for each action to be uploaded. Write
//    these actions back to the store and erase actions that should be erased.
//   3. Try to upload the batch of actions. If the upload is successful, get
//    the new consistency token and erase the uploaded actions from the store.
//
// If we have a new consistency token, it's the caller's responsibility to write
// it to storage.
class UploadActionsTask : public offline_pages::Task {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);
    UploadActionsStatus status;
    // For testing. Reports the number of actions for which upload was
    // attempted.
    size_t upload_attempt_count;
    // For testing. Reports the number of actions which were erased because of
    // staleness.
    size_t stale_count;
    // Information about the last network request, if one was attempted.
    absl::optional<NetworkResponseInfo> last_network_response_info;
  };

  // Store an action. Use |upload_now|=true to kick off an upload of all pending
  // actions. |callback| is called with the new consistency token (or empty
  // string if no token was received).
  UploadActionsTask(feedwire::FeedAction action,
                    bool upload_now,
                    const LoggingParameters& logging_parameters,
                    FeedStream* stream,
                    base::OnceCallback<void(Result)> callback);
  // Upload |pending_actions| and update the store. Note: |pending_actions|
  // should already be in the store before running the task.
  UploadActionsTask(std::vector<feedstore::StoredAction> pending_actions,
                    FeedStream* stream,
                    LaunchReliabilityLogger* launch_reliability_logger,
                    base::OnceCallback<void(Result)> callback);
  // Same as above, but reads pending actions and consistency token from the
  // store and uploads those.
  UploadActionsTask(FeedStream* stream,
                    LaunchReliabilityLogger* launch_reliability_logger,
                    base::OnceCallback<void(Result)> callback);

  ~UploadActionsTask() override;
  UploadActionsTask(const UploadActionsTask&) = delete;
  UploadActionsTask& operator=(const UploadActionsTask&) = delete;

 private:
  explicit UploadActionsTask(
      FeedStream* stream,
      base::OnceCallback<void(UploadActionsTask::Result)> callback);
  class Batch;

  void Run() override;

  void OnStorePendingActionFinished(bool write_ok);

  void ReadActions();
  void OnReadPendingActionsFinished(
      std::vector<feedstore::StoredAction> result);
  void UploadPendingActions();
  void UpdateAndUploadNextBatch();
  void OnUpdateActionsFinished(std::unique_ptr<Batch> batch, bool update_ok);
  void OnUploadFinished(
      std::unique_ptr<Batch> batch,
      FeedNetwork::ApiResult<feedwire::UploadActionsResponse> result);
  void OnUploadedActionsRemoved(bool remove_ok);
  void UpdateTokenAndFinish();
  void BatchComplete(UploadActionsBatchStatus status);
  void Done(UploadActionsStatus status);

  const raw_ref<FeedStream> stream_;
  bool upload_now_ = false;
  bool read_pending_actions_ = false;
  LoggingParameters logging_parameters_;
  // Pending action to be stored.
  absl::optional<feedwire::FeedAction> wire_action_;

  // Pending actions to be uploaded, set either by the constructor or by
  // OnReadPendingActionsFinished(). Not set if we're just storing an action.
  std::vector<feedstore::StoredAction> pending_actions_;

  // This copy of the consistency token is set in Run(), possibly updated
  // through batch uploads, and then persisted before the task finishes.
  std::string consistency_token_;
  base::OnceCallback<void(Result)> callback_;

  // Number of actions for which upload was attempted.
  size_t upload_attempt_count_ = 0;
  // Number of stale actions.
  size_t stale_count_ = 0;
  absl::optional<NetworkResponseInfo> last_network_response_info_;
  AccountInfo account_info_;
  raw_ptr<LaunchReliabilityLogger> launch_reliability_logger_ = nullptr;
  NetworkRequestId last_network_request_id_;

  base::WeakPtrFactory<UploadActionsTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_
