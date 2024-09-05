// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/upload_actions_task.h"

#include <memory>
#include <vector>
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/action_surface.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/types.h"

namespace feed {
using feedstore::StoredAction;

namespace {

bool ShouldUpload(const StoredAction& action) {
  base::Time action_time =
      base::Time::UnixEpoch() +
      base::Seconds(action.action().client_data().timestamp_seconds());
  base::TimeDelta age = base::Time::Now() - action_time;
  if (age.is_negative())
    age = base::TimeDelta();

  return action.upload_attempt_count() <
             GetFeedConfig().max_action_upload_attempts &&
         age < GetFeedConfig().max_action_age;
}

}  // namespace

UploadActionsTask::Result::Result() = default;
UploadActionsTask::Result::~Result() = default;
UploadActionsTask::Result::Result(UploadActionsTask::Result&&) = default;
UploadActionsTask::Result& UploadActionsTask::Result::operator=(Result&&) =
    default;

class UploadActionsTask::Batch {
 public:
  Batch()
      : feed_action_request_(
            std::make_unique<feedwire::UploadActionsRequest>()) {}
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
        *feed_action_request_->add_feed_actions() = action.action();
        action.set_upload_attempt_count(action.upload_attempt_count() + 1);
        uploaded_ids_.emplace_back(action.id());
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

  std::unique_ptr<feedwire::UploadActionsRequest> disown_feed_action_request() {
    return std::move(feed_action_request_);
  }
  std::vector<LocalActionId> disown_uploaded_ids() {
    return std::move(uploaded_ids_);
  }

 private:
  std::unique_ptr<feedwire::UploadActionsRequest> feed_action_request_;
  std::vector<LocalActionId> uploaded_ids_;
  size_t stale_count_ = 0;
};

UploadActionsTask::WireAction::WireAction(
    feedwire::FeedAction action,
    const LoggingParameters& logging_parameters,
    bool upload_now)
    : action(std::move(action)),
      logging_parameters(logging_parameters),
      upload_now(upload_now) {}

UploadActionsTask::WireAction::WireAction(const WireAction&) = default;
UploadActionsTask::WireAction::WireAction(WireAction&&) = default;
UploadActionsTask::WireAction& UploadActionsTask::WireAction::operator=(
    const WireAction&) = default;
UploadActionsTask::WireAction& UploadActionsTask::WireAction::operator=(
    WireAction&&) = default;
UploadActionsTask::WireAction::~WireAction() = default;

UploadActionsTask::UploadActionsTask(
    const StreamType& stream_type,
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : stream_type_(stream_type),
      stream_(*stream),
      callback_(std::move(callback)) {
  account_info_ = stream_->GetAccountInfo();
}

UploadActionsTask::UploadActionsTask(
    WireAction wire_action,
    const StreamType& stream_type,
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : UploadActionsTask(stream_type, stream, std::move(callback)) {
  wire_action_ = std::move(wire_action);
}

UploadActionsTask::UploadActionsTask(
    std::vector<feedstore::StoredAction> pending_actions,
    bool from_load_more,
    const StreamType& stream_type,
    FeedStream* stream,
    base::OnceCallback<void(UploadActionsTask::Result)> callback)
    : UploadActionsTask(stream_type, stream, std::move(callback)) {
  pending_actions_ = std::move(pending_actions);
  from_load_more_ = from_load_more;
}

UploadActionsTask::~UploadActionsTask() = default;

void UploadActionsTask::Run() {
  if (stream_->ClearAllInProgress()) {
    Done(UploadActionsStatus::kAbortUploadActionsWithPendingClearAll);
    return;
  }

  consistency_token_ = stream_->GetMetadata().consistency_token();

  // From constructor 1: If there is an action to store, store it and maybe try
  // to upload all pending actions.
  if (wire_action_.has_value()) {
    StorePendingAction();
    return;
  }

  // From constructor 2: upload the pending actions. If not provided, read them
  // first from the store.
  if (pending_actions_.empty()) {
    ReadActions();
  } else {
    UploadPendingActions();
  }
}

void UploadActionsTask::StorePendingAction() {
  // Abort if we shouldn't be sending or storing this action.
  if (wire_action_->logging_parameters.email.empty()) {
    Done(UploadActionsStatus::kAbortUploadForSignedOutUser);
    return;
  }
  // Are logging parameters associated with a different account?
  if (wire_action_->logging_parameters.email != account_info_.email
      // Is the datastore associated with a different account?
      || stream_->GetMetadata().gaia() != account_info_.gaia) {
    Done(UploadActionsStatus::kAbortUploadForWrongUser);
    return;
  }

  auto* client_data = wire_action_->action.mutable_client_data();
  client_data->set_timestamp_seconds(
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
  client_data->set_action_surface(
      feedwire::ActionSurface::ANDROID_CHROME_NEW_TAB);

  StoredAction stored_action;

  feedstore::Metadata metadata = stream_->GetMetadata();
  int32_t action_id = feedstore::GetNextActionId(metadata).GetUnsafeValue();
  stream_->SetMetadata(std::move(metadata));
  stored_action.set_id(action_id);
  wire_action_->action.mutable_client_data()->set_sequence_number(action_id);
  *stored_action.mutable_action() = std::move(wire_action_->action);
  // No need to set upload_attempt_count as it defaults to 0.
  // WriteActions() sets the ID.
  stream_->GetStore().WriteActions(
      {std::move(stored_action)},
      base::BindOnce(&UploadActionsTask::OnStorePendingActionFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UploadActionsTask::OnStorePendingActionFinished(bool write_ok) {
  if (!write_ok) {
    Done(UploadActionsStatus::kFailedToStorePendingAction);
    return;
  }

  if (!wire_action_->upload_now) {
    Done(UploadActionsStatus::kStoredPendingAction);
    return;
  }

  // If the new action was stored and upload_now was set, load all pending
  // actions and try to upload.
  ReadActions();
}

void UploadActionsTask::ReadActions() {
  stream_->GetStore().ReadActions(
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
  // Can't upload actions for signed-out users, so abort.
  if (!stream_->IsSignedIn()) {
    Done(UploadActionsStatus::kAbortUploadForSignedOutUser);
    return;
  }
  // Can't upload actions for another user, so abort.
  if (stream_->GetAccountInfo() != account_info_ ||
      // Is the datastore associated with a different account?
      stream_->GetMetadata().gaia() != account_info_.gaia) {
    Done(UploadActionsStatus::kAbortUploadForWrongUser);
    return;
  }
  UpdateAndUploadNextBatch();
}

void UploadActionsTask::UpdateAndUploadNextBatch() {
  // Finish if there's no quota remaining for actions uploads.
  if (!stream_->GetRequestThrottler().RequestQuota(
          NetworkRequestType::kUploadActions)) {
    return BatchComplete(UploadActionsBatchStatus::kExhaustedUploadQuota);
  }

  // Grab a few actions to be processed and erase them from pending_actions_.
  auto batch = std::make_unique<Batch>();
  std::vector<feedstore::StoredAction> to_update;
  std::vector<LocalActionId> to_erase;
  batch->BiteOffAFewActions(&pending_actions_, &to_update, &to_erase);

  // Update upload_attempt_count, remove old actions, then try to upload.
  stream_->GetStore().UpdateActions(
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

  std::unique_ptr<feedwire::UploadActionsRequest> request =
      batch->disown_feed_action_request();
  SetConsistencyToken(*request, consistency_token_);

  LaunchReliabilityLogger* launch_reliability_logger =
      GetLaunchReliabilityLogger();
  if (launch_reliability_logger) {
    last_network_request_id_ =
        launch_reliability_logger->LogActionsUploadRequestStart();
    if (from_load_more_) {
      launch_reliability_logger->LogLoadMoreActionUploadRequestStarted();
    }
  }

  stream_->GetNetwork().SendApiRequest<UploadActionsDiscoverApi>(
      *request, account_info_, stream_->GetSignedInRequestMetadata(),
      base::BindOnce(&UploadActionsTask::OnUploadFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(batch)));
}

void UploadActionsTask::OnUploadFinished(
    std::unique_ptr<UploadActionsTask::Batch> batch,
    FeedNetwork::ApiResult<feedwire::UploadActionsResponse> result) {
  last_network_response_info_ = result.response_info;

  LaunchReliabilityLogger* launch_reliability_logger =
      GetLaunchReliabilityLogger();
  if (launch_reliability_logger) {
    launch_reliability_logger->LogRequestSent(
        last_network_request_id_, result.response_info.loader_start_time_ticks);

    if (result.response_info.status_code > 0) {
      launch_reliability_logger->LogResponseReceived(
          last_network_request_id_, /*server_receive_timestamp_ns=*/0l,
          /*server_send_timestamp_ns=*/0l,
          result.response_info.fetch_time_ticks);
    }

    launch_reliability_logger->LogRequestFinished(
        last_network_request_id_, result.response_info.status_code);
  }

  if (!result.response_body)
    return BatchComplete(UploadActionsBatchStatus::kFailedToUpload);

  consistency_token_ =
      std::move(result.response_body->consistency_token().token());

  stream_->GetStore().RemoveActions(
      batch->disown_uploaded_ids(),
      base::BindOnce(&UploadActionsTask::OnUploadedActionsRemoved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UploadActionsTask::OnUploadedActionsRemoved(bool remove_ok) {
  if (remove_ok) {
    BatchComplete(UploadActionsBatchStatus::kSuccessfullyUploadedBatch);
  } else {
    BatchComplete(UploadActionsBatchStatus::kFailedToRemoveUploadedActions);
  }
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
  feedstore::Metadata metadata = stream_->GetMetadata();
  metadata.set_consistency_token(consistency_token_);
  stream_->SetMetadata(metadata);
  Done(UploadActionsStatus::kUpdatedConsistencyToken);
}

void UploadActionsTask::Done(UploadActionsStatus status) {
  stream_->GetMetricsReporter().OnUploadActions(status);
  Result result;
  result.status = status;
  result.upload_attempt_count = upload_attempt_count_;
  result.stale_count = stale_count_;
  result.last_network_response_info = std::move(last_network_response_info_);
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

LaunchReliabilityLogger* UploadActionsTask::GetLaunchReliabilityLogger() const {
  if (!stream_type_.IsValid()) {
    return nullptr;
  }
  return &(stream_->GetLaunchReliabilityLogger(stream_type_));
}

}  // namespace feed
