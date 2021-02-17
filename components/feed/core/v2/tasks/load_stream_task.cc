// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"

namespace feed {
namespace {
using LoadType = LoadStreamTask::LoadType;
using Result = LoadStreamTask::Result;

feedwire::FeedQuery::RequestReason GetRequestReason(LoadType load_type) {
  switch (load_type) {
    case LoadType::kInitialLoad:
      return feedwire::FeedQuery::MANUAL_REFRESH;
    case LoadType::kBackgroundRefresh:
      return feedwire::FeedQuery::SCHEDULED_REFRESH;
  }
}

}  // namespace

Result::Result() = default;
Result::Result(LoadStreamStatus status) : final_status(status) {}
Result::~Result() = default;
Result::Result(Result&&) = default;
Result& Result::operator=(Result&&) = default;

LoadStreamTask::LoadStreamTask(LoadType load_type,
                               const StreamType& stream_type,
                               FeedStream* stream,
                               base::OnceCallback<void(Result)> done_callback)
    : load_type_(load_type),
      stream_type_(stream_type),
      stream_(stream),
      done_callback_(std::move(done_callback)) {
  latencies_ = std::make_unique<LoadLatencyTimes>();
}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  latencies_->StepComplete(LoadLatencyTimes::kTaskExecution);
  // Phase 1: Try to load from persistent storage.

  // TODO(harringtond): We're checking ShouldAttemptLoad() here and before the
  // task is added to the task queue. Maybe we can simplify this.

  // First, ensure we still should load the model.
  LoadStreamStatus should_not_attempt_reason =
      stream_->ShouldAttemptLoad(stream_type_,
                                 /*model_loading=*/true);
  if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    return Done(should_not_attempt_reason);
  }

  // Use |kConsistencyTokenOnly| to short-circuit loading from store if we don't
  // need the full stream state.
  auto load_from_store_type =
      (load_type_ == LoadType::kInitialLoad)
          ? LoadStreamFromStoreTask::LoadType::kFullLoad
          : LoadStreamFromStoreTask::LoadType::kPendingActionsOnly;
  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      load_from_store_type, stream_type_, stream_->GetStore(),
      stream_->MissedLastRefresh(),
      base::BindOnce(&LoadStreamTask::LoadFromStoreComplete, GetWeakPtr()));
  load_from_store_task_->Execute(base::DoNothing());
}

void LoadStreamTask::LoadFromStoreComplete(
    LoadStreamFromStoreTask::Result result) {
  load_from_store_status_ = result.status;
  latencies_->StepComplete(LoadLatencyTimes::kLoadFromStore);
  stored_content_age_ = result.content_age;

  // Phase 2.
  //  - If loading from store works, update the model.
  //  - Otherwise, try to load from the network.

  if (load_type_ == LoadType::kInitialLoad &&
      result.status == LoadStreamStatus::kLoadedFromStore) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(result.update_request));
    stream_->LoadModel(stream_type_, std::move(model));
    Done(LoadStreamStatus::kLoadedFromStore);
    return;
  }

  // If data in store is stale, we'll continue with a network request, but keep
  // the stale model data in case we fail to load a fresh feed.
  if (load_type_ == LoadType::kInitialLoad &&
      (result.status == LoadStreamStatus::kDataInStoreStaleMissedLastRefresh ||
       result.status == LoadStreamStatus::kDataInStoreIsStale ||
       result.status ==
           LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture)) {
    stale_store_state_ = std::move(result.update_request);
  }

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(stream_type_);
  if (final_status != LoadStreamStatus::kNoStatus) {
    Done(final_status);
    return;
  }

  // If making a request, first try to upload pending actions.
  upload_actions_task_ = std::make_unique<UploadActionsTask>(
      std::move(result.pending_actions), stream_,
      base::BindOnce(&LoadStreamTask::UploadActionsComplete, GetWeakPtr()));
  upload_actions_task_->Execute(base::DoNothing());
}

void LoadStreamTask::UploadActionsComplete(UploadActionsTask::Result result) {
  bool force_signed_out_request =
      stream_->ShouldForceSignedOutFeedQueryRequest();
  upload_actions_result_ =
      std::make_unique<UploadActionsTask::Result>(std::move(result));
  latencies_->StepComplete(LoadLatencyTimes::kUploadActions);
  // TODO(crbug/1152592): Send a different network request type for WebFeeds.
  stream_->GetNetwork()->SendQueryRequest(
      NetworkRequestType::kFeedQuery,
      CreateFeedQueryRefreshRequest(
          GetRequestReason(load_type_),
          stream_->GetRequestMetadata(stream_type_, /*is_for_next_page=*/false),
          stream_->GetMetadata()->GetConsistencyToken()),
      force_signed_out_request,
      base::BindOnce(&LoadStreamTask::QueryRequestComplete, GetWeakPtr()));
}

void LoadStreamTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  latencies_->StepComplete(LoadLatencyTimes::kQueryRequest);

  DCHECK(!stream_->GetModel(stream_type_));

  network_response_info_ = result.response_info;

  if (result.response_info.status_code != 200)
    return Done(LoadStreamStatus::kNetworkFetchFailed);

  if (!result.response_body) {
    if (result.response_info.response_body_bytes > 0)
      return Done(LoadStreamStatus::kCannotParseNetworkResponseBody);
    else
      return Done(LoadStreamStatus::kNoResponseBody);
  }

  RefreshResponseData response_data =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body,
          StreamModelUpdateRequest::Source::kNetworkUpdate,
          result.response_info.was_signed_in, base::Time::Now());
  if (!response_data.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  loaded_new_content_from_network_ = true;

  stream_->GetStore()->OverwriteStream(
      stream_type_,
      std::make_unique<StreamModelUpdateRequest>(
          *response_data.model_update_request),
      base::DoNothing());

  bool isNoticeCardFulfilled = response_data.model_update_request->stream_data
                                   .privacy_notice_fulfilled();
  stream_->SetLastStreamLoadHadNoticeCard(isNoticeCardFulfilled);
  MetricsReporter::NoticeCardFulfilled(isNoticeCardFulfilled);

  stream_->GetMetadata()->MaybeUpdateSessionId(response_data.session_id);
  if (response_data.experiments)
    experiments_ = *response_data.experiments;

  if (load_type_ != LoadType::kBackgroundRefresh) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(response_data.model_update_request));
    stream_->LoadModel(stream_type_, std::move(model));
  }

  if (response_data.request_schedule)
    stream_->SetRequestSchedule(*response_data.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadStreamTask::Done(LoadStreamStatus status) {
  // If the network load fails, but there is stale content in the store, use
  // that stale content.
  if (stale_store_state_ && status != LoadStreamStatus::kLoadedFromNetwork) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(stale_store_state_));
    stream_->LoadModel(stream_type_, std::move(model));
    status = LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure;
  }
  Result result;
  result.stream_type = stream_type_;
  result.load_from_store_status = load_from_store_status_;
  result.stored_content_age = stored_content_age_;
  result.final_status = status;
  result.load_type = load_type_;
  result.network_response_info = network_response_info_;
  result.loaded_new_content_from_network = loaded_new_content_from_network_;
  result.latencies = std::move(latencies_);
  result.upload_actions_result = std::move(upload_actions_result_);
  result.experiments = experiments_;
  std::move(done_callback_).Run(std::move(result));
  TaskComplete();
}

}  // namespace feed
