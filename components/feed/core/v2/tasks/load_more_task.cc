// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_more_task.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"

namespace feed {

LoadMoreTask::LoadMoreTask(FeedStream* stream,
                           base::OnceCallback<void(Result)> done_callback)
    : stream_(stream), done_callback_(std::move(done_callback)) {}

LoadMoreTask::~LoadMoreTask() = default;

void LoadMoreTask::Run() {
  // Check prerequisites.
  StreamModel* model = stream_->GetModel();
  if (!model)
    return Done(LoadStreamStatus::kLoadMoreModelIsNotLoaded);

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(/*is_load_more=*/true);
  if (final_status != LoadStreamStatus::kNoStatus)
    return Done(final_status);

  upload_actions_task_ = std::make_unique<UploadActionsTask>(
      stream_,
      base::BindOnce(&LoadMoreTask::UploadActionsComplete, GetWeakPtr()));
  upload_actions_task_->Execute(base::DoNothing());
}

void LoadMoreTask::UploadActionsComplete(UploadActionsTask::Result result) {
  // Send network request.
  bool force_signed_out_request =
      stream_->ShouldForceSignedOutFeedQueryRequest();
  fetch_start_time_ = stream_->GetTickClock()->NowTicks();
  stream_->GetNetwork()->SendQueryRequest(
      CreateFeedQueryLoadMoreRequest(
          stream_->GetRequestMetadata(),
          stream_->GetMetadata()->GetConsistencyToken(),
          stream_->GetModel()->GetNextPageToken()),
      force_signed_out_request,
      base::BindOnce(&LoadMoreTask::QueryRequestComplete, GetWeakPtr(),
                     force_signed_out_request));
}

void LoadMoreTask::QueryRequestComplete(
    bool was_forced_signed_out_request,
    FeedNetwork::QueryRequestResult result) {
  StreamModel* model = stream_->GetModel();
  DCHECK(model) << "Model was unloaded outside of a Task";

  if (!result.response_body)
    return Done(LoadStreamStatus::kNoResponseBody);

  bool was_signed_in_request =
      !was_forced_signed_out_request && stream_->IsSignedIn();

  RefreshResponseData translated_response =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body,
          StreamModelUpdateRequest::Source::kNetworkLoadMore,
          was_signed_in_request, stream_->GetClock()->Now());

  if (!translated_response.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  loaded_new_content_from_network_ =
      !translated_response.model_update_request->stream_structures.empty();
  model->Update(std::move(translated_response.model_update_request));

  if (translated_response.request_schedule)
    stream_->SetRequestSchedule(*translated_response.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadMoreTask::Done(LoadStreamStatus status) {
  Result result;
  result.final_status = status;
  result.loaded_new_content_from_network = loaded_new_content_from_network_;
  std::move(done_callback_).Run(result);
  TaskComplete();
}

}  // namespace feed
