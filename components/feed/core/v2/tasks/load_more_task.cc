// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_more_task.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/wire_response_translator.h"
#include "components/feed/feed_feature_list.h"

namespace feed {

LoadMoreTask::Result::~Result() = default;
LoadMoreTask::Result::Result() = default;
LoadMoreTask::Result::Result(Result&&) = default;
LoadMoreTask::Result& LoadMoreTask::Result::operator=(Result&&) = default;

LoadMoreTask::LoadMoreTask(const StreamType& stream_type,
                           FeedStream* stream,
                           base::OnceCallback<void(Result)> done_callback)
    : stream_type_(stream_type),
      stream_(*stream),
      done_callback_(std::move(done_callback)) {}

LoadMoreTask::~LoadMoreTask() = default;

void LoadMoreTask::Run() {
  if (stream_->ClearAllInProgress()) {
    Done(LoadStreamStatus::kAbortWithPendingClearAll);
    return;
  }
  // Check prerequisites.
  StreamModel* model = stream_->GetModel(stream_type_);
  if (!model)
    return Done(LoadStreamStatus::kLoadMoreModelIsNotLoaded);

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(stream_type_, LoadType::kLoadMore)
          .load_stream_status;
  if (final_status != LoadStreamStatus::kNoStatus)
    return Done(final_status);

  upload_actions_task_ = std::make_unique<UploadActionsTask>(
      &*stream_,
      /*launch_reliability_logger=*/nullptr,
      base::BindOnce(&LoadMoreTask::UploadActionsComplete, GetWeakPtr()));
  upload_actions_task_->Execute(base::DoNothing());
}

void LoadMoreTask::UploadActionsComplete(UploadActionsTask::Result result) {
  StreamModel* model = stream_->GetModel(stream_type_);
  DCHECK(model) << "Model was unloaded outside of a Task";

  // Determine whether the load more request should be forced signed-out
  // regardless of the live sign-in state of the client.
  //
  // The signed-in state of the model is used instead of using
  // FeedStream#ShouldForceSignedOutFeedQueryRequest because the load more
  // requests should be in the same signed-in state as the prior requests that
  // filled the model to have consistent data.
  //
  // The sign-in state of the load stream request that brings the initial
  // content determines the sign-in state of the subsequent load more requests.
  // This avoids a possible situation where there would be a mix of signed-in
  // and signed-out content, which we don't want.
  AccountInfo account_info =
      model->signed_in() ? stream_->GetAccountInfo() : AccountInfo{};
  // Send network request.
  fetch_start_time_ = base::TimeTicks::Now();

  RequestMetadata request_metadata =
      stream_->GetRequestMetadata(stream_type_,
                                  /*is_for_next_page=*/true);
  feedwire::Request request = CreateFeedQueryLoadMoreRequest(
      request_metadata, stream_->GetMetadata().consistency_token(),
      stream_->GetModel(stream_type_)->GetNextPageToken());

  if (base::FeatureList::IsEnabled(kDiscoFeedEndpoint) &&
      !GetFeedConfig().use_feed_query_requests) {
    stream_->GetNetwork().SendApiRequest<QueryNextPageDiscoverApi>(
        request, account_info, std::move(request_metadata),
        base::BindOnce(&LoadMoreTask::QueryApiRequestComplete, GetWeakPtr()));
  } else {
    stream_->GetNetwork().SendQueryRequest(
        NetworkRequestType::kNextPage, request, account_info,
        base::BindOnce(&LoadMoreTask::QueryRequestComplete, GetWeakPtr()));
  }
}

void LoadMoreTask::QueryApiRequestComplete(
    FeedNetwork::ApiResult<feedwire::Response> result) {
  ProcessNetworkResponse(std::move(result.response_body),
                         std::move(result.response_info));
}

void LoadMoreTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  ProcessNetworkResponse(std::move(result.response_body),
                         std::move(result.response_info));
}

void LoadMoreTask::ProcessNetworkResponse(
    std::unique_ptr<feedwire::Response> response_body,
    NetworkResponseInfo response_info) {
  StreamModel* model = stream_->GetModel(stream_type_);
  DCHECK(model) << "Model was unloaded outside of a Task";

  LoadStreamStatus network_status = LoadStreamTask::LaunchResultFromNetworkInfo(
                                        response_info, response_body != nullptr)
                                        .load_stream_status;
  if (network_status != LoadStreamStatus::kNoStatus)
    return Done(network_status);

  RefreshResponseData translated_response =
      stream_->GetWireResponseTranslator().TranslateWireResponse(
          *response_body, StreamModelUpdateRequest::Source::kNetworkLoadMore,
          response_info.account_info, base::Time::Now());

  if (!translated_response.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  result_.loaded_new_content_from_network =
      !translated_response.model_update_request->stream_structures.empty();

  auto updated_metadata = stream_->GetMetadata();
  SetLastFetchTime(updated_metadata, stream_type_,
                   translated_response.last_fetch_timestamp);
  if (translated_response.session_id) {
    feedstore::MaybeUpdateSessionId(updated_metadata,
                                    translated_response.session_id);
  }
  stream_->SetMetadata(std::move(updated_metadata));

  result_.model_update_request =
      std::move(translated_response.model_update_request);

  result_.request_schedule = std::move(translated_response.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadMoreTask::Done(LoadStreamStatus status) {
  result_.stream_type = stream_type_;
  result_.final_status = status;
  std::move(done_callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
