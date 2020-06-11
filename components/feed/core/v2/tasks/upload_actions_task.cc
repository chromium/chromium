// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/upload_actions_task.h"

#include <memory>
#include <vector>
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/action_request.pb.h"
#include "components/feed/core/proto/v2/wire/feed_action_request.pb.h"
#include "components/feed/core/proto/v2/wire/feed_action_response.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/request_throttler.h"

namespace feed {
using feedstore::StoredAction;

namespace {

bool ShouldUpload(const StoredAction& action) {
  base::Time action_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(
          action.action().client_data().timestamp_seconds());
  base::TimeDelta age = base::Time::Now() - action_time;
  if (age < base::TimeDelta())
    age = base::TimeDelta();

  return action.upload_attempt_count() <
             GetFeedConfig().max_action_upload_attempts &&
         age < GetFeedConfig().max_action_age;
}

}  // namespace

class UploadActionsTask::Batch {
 public:
  Batch()
      : feed_action_request_(std::make_unique<feedwire::FeedActionRequest>()) {}
  Batch(const Batch&) = delete;
  Batch& operator=(const Batch&) = delete;
  ~Batch() = default;

  // Consumes one or more actions and erases them from |actions_in|. Actions
  // that should be updated in the store and uploaded are added to |to_update|
  // and IDs of actions that should be erased from the store are added to
  // |to_erase|.
  void BiteOffAFewActions(std::vector<StoredAction>* actions_in,
                          std::vector<StoredAction>* to_update,
                          std::vector<LocalActionId>* to_erase) {
    size_t upload_size = 0ul;
    for (StoredAction& action : *actions_in) {
      if (ShouldUpload(action)) {
        size_t message_size = action.ByteSizeLong();
        // In the weird event that a single action is larger than the limit, it
        // will be uploaded by itself.
        if (upload_size > 0ul && message_size + upload_size >
                                     GetFeedConfig().max_action_upload_bytes)
          break;

        *feed_action_request_->add_feed_action() = action.action();
        action.set_upload_attempt_count(action.upload_attempt_count() + 1);
        uploaded_ids_.push_back(LocalActionId(action.id()));
        to_update->push_back(std::move(action));

        upload_size += message_size;
      } else {
        to_erase->push_back(LocalActionId(action.id()));
      }
    }

    size_t actions_consumed = to_update->size() + to_erase->size();
    actions_in->erase(actions_in->begin(),
                      actions_in->begin() + actions_consumed);
    stale_count_ = to_erase->size();
  }

  size_t UploadCount() const { return uploaded_ids_.size(); }
  size_t StaleCount() const { return stale_count_; }

  std::unique_ptr<feedwire::FeedActionRequest> disown_feed_action_request() {
    return std::move(feed_action_request_);
  }
  std::vector<LocalActionId> disown_uploaded_ids() {
    return std::move(uploaded_ids_);
  }

 private:
  std::unique_ptr<feedwire::FeedActionRequest> feed_action_request_;
  std::vector<LocalActionId> uploaded_ids_;
  size_t stale_count_ = 0;
};

UploadActionsTask::UploadActionsTask(
    feedwire::FeedAction action,
    bool upload_now,
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : stream_(stream),
      upload_now_(upload_now),
      wire_action_(std::move(action)),
      callback_(std::move(callback)) {
  wire_action_->mutable_client_data()->set_timestamp_seconds(
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
}

UploadActionsTask::UploadActionsTask(
    std::vector<feedstore::StoredAction> pending_actions,
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : stream_(stream),
      pending_actions_(std::move(pending_actions)),
      callback_(std::move(callback)) {}

UploadActionsTask::UploadActionsTask(
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : stream_(stream),
      read_pending_actions_(true),
      callback_(std::move(callback)) {}

UploadActionsTask::~UploadActionsTask() = default;

void UploadActionsTask::Run() {
  consistency_token_ = stream_->GetMetadata()->GetConsistencyToken();

  // From constructor 1: If there is an action to store, store it and maybe try
  // to upload all pending actions.
  if (wire_action_) {
    StoredAction action;
    action.set_id(stream_->GetMetadata()->GetNextActionId().GetUnsafeValue());
    *action.mutable_action() = std::move(*wire_action_);
    // No need to set upload_attempt_count as it defaults to 0.
    // WriteActions() sets the ID.
    stream_->GetStore()->WriteActions(
        {std::move(action)},
        base::BindOnce(&UploadActionsTask::OnStorePendingActionFinished,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // From constructor 3: Read actions and upload.
  if (read_pending_actions_) {
    ReadActions();
    return;
  }

  // From constructor 2: Upload whatever was passed to us.
  UploadPendingActions();
}

void UploadActionsTask::OnStorePendingActionFinished(bool write_ok) {
  if (!write_ok) {
    Done(UploadActionsStatus::kFailedToStorePendingAction);
    return;
  }

  if (!upload_now_) {
    Done(UploadActionsStatus::kStoredPendingAction);
    return;
  }

  // If the new action was stored and upload_now was set, load all pending
  // actions and try to upload.
  ReadActions();
}

void UploadActionsTask::ReadActions() {
  stream_->GetStore()->ReadActions(
      base::BindOnce(&UploadActionsTask::OnReadPendingActionsFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UploadActionsTask::OnReadPendingActionsFinished(
    std::vector<feedstore::StoredAction> actions) {
  pending_actions_ = std::move(actions);
  UploadPendingActions();
}

void UploadActionsTask::UploadPendingActions() {
  if (pending_actions_.empty()) {
    Done(UploadActionsStatus::kNoPendingActions);
    return;
  }
  UpdateAndUploadNextBatch();
}

void UploadActionsTask::UpdateAndUploadNextBatch() {
  // Finish if there's no quota remaining for actions uploads.
  if (!stream_->GetRequestThrottler()->RequestQuota(
          NetworkRequestType::kUploadActions)) {
    return BatchComplete(UploadActionsBatchStatus::kExhaustedUploadQuota);
  }

  // Grab a few actions to be processed and erase them from pending_actions_.
  auto batch = std::make_unique<Batch>();
  std::vector<feedstore::StoredAction> to_update;
  std::vector<LocalActionId> to_erase;
  batch->BiteOffAFewActions(&pending_actions_, &to_update, &to_erase);

  // Update upload_attempt_count, remove old actions, then try to upload.
  stream_->GetStore()->UpdateActions(
      std::move(to_update), std::move(to_erase),
      base::BindOnce(&UploadActionsTask::OnUpdateActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(batch)));
}

void UploadActionsTask::OnUpdateActionsFinished(
    std::unique_ptr<UploadActionsTask::Batch> batch,
    bool update_ok) {
  // Stop if there are no actions to upload.
  if (batch->UploadCount() == 0ul)
    return BatchComplete(UploadActionsBatchStatus::kAllActionsWereStale);

  // Skip uploading batch if these actions couldn't be updated in the store.
  if (!update_ok)
    return BatchComplete(UploadActionsBatchStatus::kFailedToUpdateStore);

  upload_attempt_count_ += batch->UploadCount();
  stale_count_ += batch->StaleCount();

  std::unique_ptr<feedwire::FeedActionRequest> request =
      batch->disown_feed_action_request();
  request->mutable_consistency_token()->set_token(consistency_token_);

  feedwire::ActionRequest action_request;
  action_request.set_request_version(
      feedwire::ActionRequest::FEED_UPLOAD_ACTION);
  action_request.set_allocated_feed_action_request(request.release());

  FeedNetwork* network = stream_->GetNetwork();
  DCHECK(network);

  network->SendActionRequest(
      action_request,
      base::BindOnce(&UploadActionsTask::OnUploadFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(batch)));
}

void UploadActionsTask::OnUploadFinished(
    std::unique_ptr<UploadActionsTask::Batch> batch,
    FeedNetwork::ActionRequestResult result) {
  if (!result.response_body)
    return BatchComplete(UploadActionsBatchStatus::kFailedToUpload);

  consistency_token_ = std::move(result.response_body->feed_response()
                                     .feed_response()
                                     .consistency_token()
                                     .token());

  stream_->GetStore()->RemoveActions(
      batch->disown_uploaded_ids(),
      base::BindOnce(&UploadActionsTask::OnUploadedActionsRemoved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UploadActionsTask::OnUploadedActionsRemoved(bool remove_ok) {
  if (remove_ok)
    BatchComplete(UploadActionsBatchStatus::kSuccessfullyUploadedBatch);
  else
    BatchComplete(UploadActionsBatchStatus::kFailedToRemoveUploadedActions);
}

void UploadActionsTask::BatchComplete(UploadActionsBatchStatus status) {
  MetricsReporter::OnUploadActionsBatch(status);

  if (pending_actions_.empty() ||
      status == UploadActionsBatchStatus::kExhaustedUploadQuota ||
      status == UploadActionsBatchStatus::kAllActionsWereStale) {
    return UpdateTokenAndFinish();
  }
  UpdateAndUploadNextBatch();
}

void UploadActionsTask::UpdateTokenAndFinish() {
  if (consistency_token_.empty())
    return Done(UploadActionsStatus::kFinishedWithoutUpdatingConsistencyToken);

  stream_->GetMetadata()->SetConsistencyToken(consistency_token_);
  Done(UploadActionsStatus::kUpdatedConsistencyToken);
}

void UploadActionsTask::Done(UploadActionsStatus status) {
  MetricsReporter::OnUploadActions(status);
  std::move(callback_).Run({status, upload_attempt_count_, stale_count_});
  TaskComplete();
}

}  // namespace feed
