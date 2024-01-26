// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
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

namespace feed {
class FeedStream;
class LaunchReliabilityLogger;

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
  struct WireAction {
    WireAction(feedwire::FeedAction action,
               const LoggingParameters& logging_parameters,
               bool upload_now);
    WireAction(const WireAction&);
    WireAction(WireAction&&);
    WireAction& operator=(const WireAction&);
    WireAction& operator=(WireAction&&);
    ~WireAction();

    feedwire::FeedAction action;
    LoggingParameters logging_parameters;
    // Indicates whether to upload all the stored pending actions.
    bool upload_now = false;
  };

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
    std::optional<NetworkResponseInfo> last_network_response_info;
  };

  // For all constructors:
  // If `stream_type` is unknown, the logging will be skipped.
  // `callback` is called with the new consistency token (or empty string if no
  // token was received).

  // Store the pending `wire_action`.
  UploadActionsTask(WireAction wire_action,
                    const StreamType& stream_type,
                    FeedStream* stream,
                    base::OnceCallback<void(Result)> callback);
  // Upload `pending_actions` which have already been read from the store.
  // If `pending_actions` is empty, read them first from the store. After the
  // upload is completed, the store will be updated.
  UploadActionsTask(std::vector<feedstore::StoredAction> pending_actions,
                    bool is_from_load_more,
                    const StreamType& stream_type,
                    FeedStream* stream,
                    base::OnceCallback<void(Result)> callback);

  ~UploadActionsTask() override;
  UploadActionsTask(const UploadActionsTask&) = delete;
  UploadActionsTask& operator=(const UploadActionsTask&) = delete;

 private:
  UploadActionsTask(
      const StreamType& stream_type,
      FeedStream* stream,
      base::OnceCallback<void(UploadActionsTask::Result)> callback);
  class Batch;

  void Run() override;

  void StorePendingAction();
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

  LaunchReliabilityLogger* GetLaunchReliabilityLogger() const;

  StreamType stream_type_;
  const raw_ref<FeedStream> stream_;

  // Pending action to be stored.
  std::optional<WireAction> wire_action_;

  // Pending actions to be uploaded. If empty, they will be read from the store
  // and set here. Ignored when `wire_action_` is present.
  std::vector<feedstore::StoredAction> pending_actions_;

  // Whether the actions upload is caused by the load more request.
  bool from_load_more_ = false;

  // This copy of the consistency token is set in Run(), possibly updated
  // through batch uploads, and then persisted before the task finishes.
  std::string consistency_token_;
  base::OnceCallback<void(Result)> callback_;

  // Number of actions for which upload was attempted.
  size_t upload_attempt_count_ = 0;
  // Number of stale actions.
  size_t stale_count_ = 0;
  std::optional<NetworkResponseInfo> last_network_response_info_;
  AccountInfo account_info_;
  NetworkRequestId last_network_request_id_;

  base::WeakPtrFactory<UploadActionsTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_UPLOAD_ACTIONS_TASK_H_
