// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/feed_feature_list.h"

namespace feed {
namespace {
using LoadType = LoadStreamTask::LoadType;
using Result = LoadStreamTask::Result;

feedwire::FeedQuery::RequestReason GetRequestReason(
    const StreamType& stream_type,
    LoadType load_type) {
  switch (load_type) {
    case LoadType::kInitialLoad:
      return stream_type.IsForYou() ? feedwire::FeedQuery::MANUAL_REFRESH
                                    : feedwire::FeedQuery::INTERACTIVE_WEB_FEED;
    case LoadType::kBackgroundRefresh:
      return stream_type.IsForYou()
                 ? feedwire::FeedQuery::SCHEDULED_REFRESH
                 // TODO(b/185848601): Switch back to PREFETCHED_WEB_FEED when
                 // the server supports it.
                 : feedwire::FeedQuery::INTERACTIVE_WEB_FEED;
  }
}

}  // namespace

Result::Result() = default;
Result::Result(const StreamType& a_stream_type, LoadStreamStatus status)
    : stream_type(a_stream_type), final_status(status) {}
Result::~Result() = default;
Result::Result(Result&&) = default;
Result& Result::operator=(Result&&) = default;

LoadStreamTask::LoadStreamTask(const Options& options,
                               FeedStream* stream,
                               base::OnceCallback<void(Result)> done_callback)
    : options_(options),
      stream_(stream),
      done_callback_(std::move(done_callback)) {
  DCHECK(options.stream_type.IsValid()) << "A stream type must be chosen";
  latencies_ = std::make_unique<LoadLatencyTimes>();
}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  if (stream_->ClearAllInProgress()) {
    Done(LoadStreamStatus::kAbortWithPendingClearAll);
    return;
  }
  latencies_->StepComplete(LoadLatencyTimes::kTaskExecution);
  // Phase 1: Try to load from persistent storage.

  // TODO(harringtond): We're checking ShouldAttemptLoad() here and before the
  // task is added to the task queue. Maybe we can simplify this.

  // First, ensure we still should load the model.
  LoadStreamStatus should_not_attempt_reason =
      stream_->ShouldAttemptLoad(options_.stream_type,
                                 /*model_loading=*/true);
  if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    return Done(should_not_attempt_reason);
  }

  if (options_.abort_if_unread_content &&
      stream_->HasUnreadContent(options_.stream_type)) {
    Done(LoadStreamStatus::kAlreadyHaveUnreadContent);
    return;
  }

  // Use |kLoadNoContent| to short-circuit loading from store if we don't
  // need the full stream state.
  auto load_from_store_type =
      (options_.load_type == LoadType::kInitialLoad)
          ? LoadStreamFromStoreTask::LoadType::kFullLoad
          : LoadStreamFromStoreTask::LoadType::kLoadNoContent;
  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      load_from_store_type, stream_, options_.stream_type, stream_->GetStore(),
      stream_->MissedLastRefresh(options_.stream_type),
      base::BindOnce(&LoadStreamTask::LoadFromStoreComplete, GetWeakPtr()));
  load_from_store_task_->Execute(base::DoNothing());
}

void LoadStreamTask::LoadFromStoreComplete(
    LoadStreamFromStoreTask::Result result) {
  load_from_store_status_ = result.status;
  latencies_->StepComplete(LoadLatencyTimes::kLoadFromStore);
  stored_content_age_ = result.content_age;
  content_ids_ = result.content_ids;

  // Phase 2. Process the result of `LoadStreamFromStoreTask`.

  if (!options_.refresh_even_when_not_stale &&
      result.status == LoadStreamStatus::kLoadedFromStore) {
    update_request_ = std::move(result.update_request);
    Done(LoadStreamStatus::kLoadedFromStore);
    return;
  }

  const bool store_is_stale =
      (result.status == LoadStreamStatus::kDataInStoreStaleMissedLastRefresh ||
       result.status == LoadStreamStatus::kDataInStoreIsStale ||
       result.status == LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture);

  // If data in store is stale, we'll continue with a network request, but keep
  // the stale model data in case we fail to load a fresh feed.
  if (options_.load_type == LoadType::kInitialLoad && store_is_stale) {
    stale_store_state_ = std::move(result.update_request);
  }

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(options_.stream_type);
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
      stream_->ShouldForceSignedOutFeedQueryRequest(options_.stream_type);
  upload_actions_result_ =
      std::make_unique<UploadActionsTask::Result>(std::move(result));
  latencies_->StepComplete(LoadLatencyTimes::kUploadActions);

  feedwire::Request request = CreateFeedQueryRefreshRequest(
      options_.stream_type,
      GetRequestReason(options_.stream_type, options_.load_type),
      stream_->GetRequestMetadata(options_.stream_type,
                                  /*is_for_next_page=*/false),
      stream_->GetMetadata().consistency_token());

  const std::string gaia =
      force_signed_out_request ? std::string() : stream_->GetSyncSignedInGaia();

  if (options_.stream_type.IsForYou() ||
      GetFeedConfig().use_feed_query_requests_for_web_feeds) {
    if (base::FeatureList::IsEnabled(kDiscoFeedEndpoint)) {
      switch (options_.load_type) {
        case LoadType::kInitialLoad:
          stream_->GetNetwork()
              ->SendApiRequest<QueryInteractiveFeedDiscoverApi>(
                  request, gaia,
                  base::BindOnce(&LoadStreamTask::QueryApiRequestComplete,
                                 GetWeakPtr()));
          break;
        case LoadType::kBackgroundRefresh:
          stream_->GetNetwork()->SendApiRequest<QueryBackgroundFeedDiscoverApi>(
              request, gaia,
              base::BindOnce(&LoadStreamTask::QueryApiRequestComplete,
                             GetWeakPtr()));
          break;
      }
    } else {
      stream_->GetNetwork()->SendQueryRequest(
          NetworkRequestType::kFeedQuery, request, gaia,
          base::BindOnce(&LoadStreamTask::QueryRequestComplete, GetWeakPtr()));
    }
  } else {
    DCHECK(options_.stream_type.IsWebFeed());
    stream_->GetNetwork()->SendApiRequest<WebFeedListContentsDiscoverApi>(
        std::move(request), gaia,
        base::BindOnce(&LoadStreamTask::QueryApiRequestComplete, GetWeakPtr()));
  }
}

void LoadStreamTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  ProcessNetworkResponse(std::move(result.response_body),
                         std::move(result.response_info));
}

void LoadStreamTask::QueryApiRequestComplete(
    FeedNetwork::ApiResult<feedwire::Response> result) {
  ProcessNetworkResponse(std::move(result.response_body),
                         std::move(result.response_info));
}

void LoadStreamTask::ProcessNetworkResponse(
    std::unique_ptr<feedwire::Response> response_body,
    NetworkResponseInfo response_info) {
  latencies_->StepComplete(LoadLatencyTimes::kQueryRequest);

  DCHECK(!stream_->GetModel(options_.stream_type));

  network_response_info_ = response_info;

  if (response_info.status_code != 200)
    return Done(LoadStreamStatus::kNetworkFetchFailed);

  if (!response_body) {
    if (response_info.response_body_bytes > 0)
      return Done(LoadStreamStatus::kCannotParseNetworkResponseBody);
    else
      return Done(LoadStreamStatus::kNoResponseBody);
  }

  RefreshResponseData response_data =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *response_body, StreamModelUpdateRequest::Source::kNetworkUpdate,
          response_info.was_signed_in, base::Time::Now());
  if (!response_data.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  loaded_new_content_from_network_ = true;
  content_ids_ =
      feedstore::GetContentIds(response_data.model_update_request->stream_data);

  stream_->GetStore()->OverwriteStream(
      options_.stream_type,
      std::make_unique<StreamModelUpdateRequest>(
          *response_data.model_update_request),
      base::DoNothing());

  fetched_content_has_notice_card_ =
      response_data.model_update_request->stream_data
          .privacy_notice_fulfilled();

  MetricsReporter::NoticeCardFulfilled(*fetched_content_has_notice_card_);

  absl::optional<feedstore::Metadata> updated_metadata =
      feedstore::MaybeUpdateSessionId(stream_->GetMetadata(),
                                      response_data.session_id);
  if (updated_metadata) {
    stream_->SetMetadata(std::move(*updated_metadata));
  }
  if (response_data.experiments)
    experiments_ = *response_data.experiments;

  if (options_.load_type != LoadType::kBackgroundRefresh) {
    update_request_ = std::move(response_data.model_update_request);
  }

  request_schedule_ = std::move(response_data.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadStreamTask::Done(LoadStreamStatus status) {
  // If the network load fails, but there is stale content in the store, use
  // that stale content.
  if (stale_store_state_ && !update_request_) {
    update_request_ = std::move(stale_store_state_);
    status = LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure;
  }
  Result result;
  result.stream_type = options_.stream_type;
  result.load_from_store_status = load_from_store_status_;
  result.stored_content_age = stored_content_age_;
  result.content_ids = content_ids_;
  result.final_status = status;
  result.load_type = options_.load_type;
  result.update_request = std::move(update_request_);
  result.request_schedule = std::move(request_schedule_);
  result.network_response_info = network_response_info_;
  result.loaded_new_content_from_network = loaded_new_content_from_network_;
  result.latencies = std::move(latencies_);
  result.fetched_content_has_notice_card = fetched_content_has_notice_card_;
  result.upload_actions_result = std::move(upload_actions_result_);
  result.experiments = experiments_;
  std::move(done_callback_).Run(std::move(result));
  TaskComplete();
}

std::ostream& operator<<(std::ostream& os,
                         const LoadStreamTask::Result& result) {
  os << "LoadStreamTask::Result{" << result.stream_type
     << " final_status=" << result.final_status
     << " load_from_store_status=" << result.load_from_store_status
     << " stored_content_age=" << result.stored_content_age
     << " load_type=" << static_cast<int>(result.load_type)
     << " request_schedule?=" << result.request_schedule.has_value();
  if (result.network_response_info)
    os << " network_response_info=" << *result.network_response_info;
  return os << " loaded_new_content_from_network="
            << result.loaded_new_content_from_network << "}";
}

}  // namespace feed
