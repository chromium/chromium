// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_more_task.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/wire_response_translator.h"

namespace feed {

LoadMoreTask::Result::~Result() = default;
LoadMoreTask::Result::Result() = default;
LoadMoreTask::Result::Result(Result&&) = default;
LoadMoreTask::Result& LoadMoreTask::Result::operator=(Result&&) = default;

LoadMoreTask::LoadMoreTask(const StreamType& stream_type,
                           FeedStream* stream,
                           base::OnceCallback<void(Result)> done_callback)
    : stream_type_(stream_type),
      stream_(stream),
      done_callback_(std::move(done_callback)) {}

LoadMoreTask::~LoadMoreTask() = default;

void LoadMoreTask::Run() {
  if (stream_->ClearAllInProgress()) {
    Done(LoadStreamStatus::kAbortWithPendingClearAll);
    return;
  }
  // Check prerequisites.
  // TODO(crbug/1152592): Parameterize stream loading by stream type.
  StreamModel* model = stream_->GetModel(stream_type_);
  if (!model)
    return Done(LoadStreamStatus::kLoadMoreModelIsNotLoaded);

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(stream_type_, /*is_load_more=*/true);
  if (final_status != LoadStreamStatus::kNoStatus)
    return Done(final_status);

  upload_actions_task_ = std::make_unique<UploadActionsTask>(
      stream_,
      base::BindOnce(&LoadMoreTask::UploadActionsComplete, GetWeakPtr()));
  upload_actions_task_->Execute(base::DoNothing());
}

void LoadMoreTask::UploadActionsComplete(UploadActionsTask::Result result) {
  // TODO(crbug/1152592): Parameterize stream loading by stream type.
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
  bool force_signed_out_request = !model->signed_in();
  // Send network request.
  fetch_start_time_ = base::TimeTicks::Now();
  // TODO(crbug/1152592): Send a different network request type for WebFeeds.
  stream_->GetNetwork()->SendQueryRequest(
      NetworkRequestType::kNextPage,
      CreateFeedQueryLoadMoreRequest(
          stream_->GetRequestMetadata(stream_type_, /*is_for_next_page=*/true),
          stream_->GetMetadata().consistency_token(),
          stream_->GetModel(stream_type_)->GetNextPageToken()),
      force_signed_out_request, stream_->GetSyncSignedInGaia(),
      base::BindOnce(&LoadMoreTask::QueryRequestComplete, GetWeakPtr()));
}

void LoadMoreTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  StreamModel* model = stream_->GetModel(stream_type_);
  DCHECK(model) << "Model was unloaded outside of a Task";

  if (!result.response_body)
    return Done(LoadStreamStatus::kNoResponseBody);

  RefreshResponseData translated_response =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body,
          StreamModelUpdateRequest::Source::kNetworkLoadMore,
          result.response_info.was_signed_in, base::Time::Now());

  if (!translated_response.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  result_.loaded_new_content_from_network =
      !translated_response.model_update_request->stream_structures.empty();

  base::Optional<feedstore::Metadata> updated_metadata =
      feedstore::MaybeUpdateSessionId(stream_->GetMetadata(),
                                      translated_response.session_id);
  if (updated_metadata) {
    stream_->SetMetadata(std::move(*updated_metadata));
  }

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
