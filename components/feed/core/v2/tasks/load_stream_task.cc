// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/view_demotion.h"
#include "components/feed/feed_feature_list.h"
#include "net/base/net_errors.h"

namespace feed {
namespace {
using Result = LoadStreamTask::Result;

feedwire::FeedQuery::RequestReason GetRequestReason(
    const StreamType& stream_type,
    LoadType load_type) {
  switch (load_type) {
    case LoadType::kInitialLoad:
    case LoadType::kManualRefresh:
      return stream_type.IsForYou() ? feedwire::FeedQuery::MANUAL_REFRESH
                                    : feedwire::FeedQuery::INTERACTIVE_WEB_FEED;
    case LoadType::kBackgroundRefresh:
      return stream_type.IsForYou()
                 ? feedwire::FeedQuery::SCHEDULED_REFRESH
                 // TODO(b/185848601): Switch back to PREFETCHED_WEB_FEED when
                 // the server supports it.
                 : feedwire::FeedQuery::INTERACTIVE_WEB_FEED;
    case LoadType::kFeedCloseBackgroundRefresh:
      return feedwire::FeedQuery::APP_CLOSE_REFRESH;
    case LoadType::kLoadMore:
      NOTREACHED_IN_MIGRATION();
      return feedwire::FeedQuery::MANUAL_REFRESH;
  }
}

}  // namespace

Result::Result() = default;
Result::Result(const StreamType& a_stream_type, LoadStreamStatus status)
    : stream_type(a_stream_type), final_status(status) {}
Result::~Result() = default;
Result::Result(Result&&) = default;
Result& Result::operator=(Result&&) = default;

// static
LaunchResult LoadStreamTask::LaunchResultFromNetworkInfo(
    const NetworkResponseInfo& response_info,
    bool has_parsed_body) {
  if (response_info.status_code == 200) {
    if (has_parsed_body) {
      // Success.
      return {LoadStreamStatus::kNoStatus,
              feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
    }
    if (response_info.response_body_bytes > 0) {
      return {
          LoadStreamStatus::kCannotParseNetworkResponseBody,
          feedwire::DiscoverLaunchResult::NO_CARDS_RESPONSE_ERROR_ZERO_CARDS};
    }
    return {LoadStreamStatus::kNoResponseBody,
            feedwire::DiscoverLaunchResult::NO_CARDS_RESPONSE_ERROR_ZERO_CARDS};
  }

  switch (response_info.account_token_fetch_status) {
    case AccountTokenFetchStatus::kUnspecified:
      if (response_info.status_code == net::ERR_TIMED_OUT) {
        return {LoadStreamStatus::kNetworkFetchTimedOut,
                feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_OTHER};
      }
      break;
    case AccountTokenFetchStatus::kSuccess:
      break;
    case AccountTokenFetchStatus::kUnexpectedAccount:
      return {
          LoadStreamStatus::kAccountTokenFetchFailedWrongAccount,
          feedwire::DiscoverLaunchResult::NO_CARDS_FAILED_TO_GET_AUTH_TOKEN};
    case AccountTokenFetchStatus::kTimedOut:
      return {
          LoadStreamStatus::kAccountTokenFetchTimedOut,
          feedwire::DiscoverLaunchResult::NO_CARDS_FAILED_TO_GET_AUTH_TOKEN};
  }
  return {LoadStreamStatus::kNetworkFetchFailed,
          feedwire::DiscoverLaunchResult::NO_CARDS_RESPONSE_ERROR_NON_200};
}

LoadStreamTask::LoadStreamTask(const Options& options,
                               FeedStream* stream,
                               base::OnceCallback<void(Result)> done_callback)
    : options_(options),
      stream_(*stream),
      done_callback_(std::move(done_callback)) {
  DCHECK(options.stream_type.IsValid()) << "A stream type must be chosen";
  DCHECK(options.load_type != LoadType::kLoadMore);
  latencies_ = std::make_unique<LoadLatencyTimes>();
}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  if (!CheckPreconditions())
    return;

  if (options_.stream_type.IsWebFeed()) {
    Suspend();
    // Unretained is safe because `stream_` owns both this and
    // `subscriptions()`.
    stream_->subscriptions().IsWebFeedSubscriber(base::BindOnce(
        &LoadStreamTask::CheckIfSubscriberComplete, base::Unretained(this)));
    return;
  }

  PassedPreconditions();
}

bool LoadStreamTask::CheckPreconditions() {
  if (stream_->ClearAllInProgress()) {
    Done({LoadStreamStatus::kAbortWithPendingClearAll,
          feedwire::DiscoverLaunchResult::CLEAR_ALL_IN_PROGRESS});
    return false;
  }
  latencies_->StepComplete(LoadLatencyTimes::kTaskExecution);
  // Phase 1: Try to load from persistent storage.

  // TODO(harringtond): We're checking ShouldAttemptLoad() here and before the
  // task is added to the task queue. Maybe we can simplify this.

  // First, ensure we still should load the model.
  LaunchResult should_not_attempt_reason =
      stream_->ShouldAttemptLoad(options_.stream_type, options_.load_type,
                                 /*model_loading=*/true);
  if (should_not_attempt_reason.load_stream_status !=
      LoadStreamStatus::kNoStatus) {
    Done(should_not_attempt_reason);
    return false;
  }

  if (options_.abort_if_unread_content &&
      stream_->HasUnreadContent(options_.stream_type)) {
    Done({LoadStreamStatus::kAlreadyHaveUnreadContent,
          feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED});
    return false;
  }

  return true;
}

void LoadStreamTask::CheckIfSubscriberComplete(bool is_web_feed_subscriber) {
  is_web_feed_subscriber_ = is_web_feed_subscriber;
  if (!is_web_feed_subscriber &&
      !base::FeatureList::IsEnabled(kWebFeedOnboarding)) {
    Done({LoadStreamStatus::kNotAWebFeedSubscriber,
          feedwire::DiscoverLaunchResult::NOT_A_WEB_FEED_SUBSCRIBER});
    return;
  }

  Resume(
      base::BindOnce(&LoadStreamTask::ResumeAtStart, base::Unretained(this)));
}

void LoadStreamTask::ResumeAtStart() {
  // When the task is resumed, we need to ensure the preconditions are still
  // met.
  if (CheckPreconditions())
    PassedPreconditions();
}

void LoadStreamTask::PassedPreconditions() {
  if (options_.load_type != LoadType::kBackgroundRefresh)
    GetLaunchReliabilityLogger().LogCacheReadStart();

  if (options_.load_type == LoadType::kManualRefresh) {
    std::vector<feedstore::StoredAction> empty_pending_actions;
    LoadFromNetwork1(std::move(empty_pending_actions),
                     /*need_to_read_pending_actions=*/true);
    return;
  }

  // Use |kLoadNoContent| to short-circuit loading from store if we don't
  // need the full stream state.
  auto load_from_store_type =
      (options_.load_type == LoadType::kInitialLoad)
          ? LoadStreamFromStoreTask::LoadType::kFullLoad
          : LoadStreamFromStoreTask::LoadType::kLoadNoContent;
  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      load_from_store_type, &*stream_, options_.stream_type,
      &stream_->GetStore(), stream_->MissedLastRefresh(options_.stream_type),
      is_web_feed_subscriber_,
      base::BindOnce(&LoadStreamTask::LoadFromStoreComplete, GetWeakPtr()));
  load_from_store_task_->Execute(base::DoNothing());
}

void LoadStreamTask::LoadFromStoreComplete(
    LoadStreamFromStoreTask::Result result) {
  load_from_store_status_ = result.status;
  latencies_->StepComplete(LoadLatencyTimes::kLoadFromStore);
  stored_content_age_ = result.content_age;
  content_ids_ = result.content_ids;

  if (options_.load_type != LoadType::kBackgroundRefresh)
    GetLaunchReliabilityLogger().LogCacheReadEnd(result.reliability_result);

  // Phase 2. Process the result of `LoadStreamFromStoreTask`.

  if (!options_.refresh_even_when_not_stale &&
      result.status == LoadStreamStatus::kLoadedFromStore) {
    update_request_ = std::move(result.update_request);
    return Done({LoadStreamStatus::kLoadedFromStore,
                 feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED});
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

  LoadFromNetwork1(std::move(result.pending_actions),
                   /*need_to_read_pending_actions=*/false);
}

void LoadStreamTask::LoadFromNetwork1(
    std::vector<feedstore::StoredAction> pending_actions_from_store,
    bool need_to_read_pending_actions) {
  // Don't consume quota if refreshed by user.
  LaunchResult should_make_request = stream_->ShouldMakeFeedQueryRequest(
      options_.stream_type, options_.load_type);
  if (should_make_request.load_stream_status != LoadStreamStatus::kNoStatus) {
    return Done(should_make_request);
  }

  ReadDocViewDigestIfEnabled(
      *stream_, base::BindOnce(&LoadStreamTask::LoadFromNetwork2, GetWeakPtr(),
                               std::move(pending_actions_from_store),
                               need_to_read_pending_actions));
}

void LoadStreamTask::LoadFromNetwork2(
    std::vector<feedstore::StoredAction> pending_actions_from_store,
    bool need_to_read_pending_actions,
    DocViewDigest doc_view_digest) {
  stream_->GetStore().RemoveDocViews(doc_view_digest.old_doc_views);
  doc_view_counts_ = std::move(doc_view_digest.doc_view_counts);

  // If no pending action exists in the store, go directly to send query
  // request.
  if (!need_to_read_pending_actions && pending_actions_from_store.empty()) {
    SendFeedQueryRequest();
  } else {
    UploadActions(std::move(pending_actions_from_store));
  }
}

void LoadStreamTask::UploadActions(
    std::vector<feedstore::StoredAction> pending_actions_from_store) {
  // If making a request, first try to upload pending actions.
  upload_actions_task_ = std::make_unique<UploadActionsTask>(
      std::move(pending_actions_from_store),
      /*from_load_more=*/false, options_.stream_type, &*stream_,
      base::BindOnce(&LoadStreamTask::UploadActionsComplete, GetWeakPtr()));
  upload_actions_task_->Execute(base::DoNothing());
}

void LoadStreamTask::UploadActionsComplete(UploadActionsTask::Result result) {
  upload_actions_result_ =
      std::make_unique<UploadActionsTask::Result>(std::move(result));
  latencies_->StepComplete(LoadLatencyTimes::kUploadActions);

  SendFeedQueryRequest();
}

void LoadStreamTask::SendFeedQueryRequest() {
  if (options_.load_type != LoadType::kBackgroundRefresh) {
    if (options_.stream_type.IsForYou())
      network_request_id_ = GetLaunchReliabilityLogger().LogFeedRequestStart();
    else if (options_.stream_type.IsWebFeed())
      network_request_id_ =
          GetLaunchReliabilityLogger().LogWebFeedRequestStart();
    else if (options_.stream_type.IsSingleWebFeed())
      network_request_id_ =
          GetLaunchReliabilityLogger().LogSingleWebFeedRequestStart();
  }
  RequestMetadata request_metadata =
      stream_->GetRequestMetadata(options_.stream_type,
                                  /*is_for_next_page=*/false);

  feedwire::Request request = CreateFeedQueryRefreshRequest(
      options_.stream_type,
      GetRequestReason(options_.stream_type, options_.load_type),
      request_metadata, stream_->GetMetadata().consistency_token(),
      options_.single_feed_entry_point, doc_view_counts_);

  const AccountInfo account_info = stream_->GetAccountInfo();
  stream_->GetMetricsReporter().NetworkRefreshRequestStarted(
      options_.stream_type, request_metadata.content_order);

  FeedNetwork& network = stream_->GetNetwork();
  const bool force_feed_query = GetFeedConfig().use_feed_query_requests;
  if (!force_feed_query && options_.stream_type.IsWebFeed()) {
    // Special case: web feed that is not using Feed Query requests go to
    // WebFeedListContentsDiscoverApi.
    network.SendApiRequest<WebFeedListContentsDiscoverApi>(
        std::move(request), account_info, std::move(request_metadata),
        base::BindOnce(&LoadStreamTask::QueryApiRequestComplete, GetWeakPtr()));
  } else if (!force_feed_query && options_.stream_type.IsSingleWebFeed()) {
    // Special case: web feed that is not using Feed Query requests go to
    // WebFeedListContentsDiscoverApi.
    network.SendApiRequest<SingleWebFeedListContentsDiscoverApi>(
        std::move(request), account_info, std::move(request_metadata),
        base::BindOnce(&LoadStreamTask::QueryApiRequestComplete, GetWeakPtr()));
  } else if (options_.stream_type.IsForYou() &&
             base::FeatureList::IsEnabled(kDiscoFeedEndpoint) &&
             !force_feed_query) {
    // Special case: For You feed using the DiscoFeedEndpoint call
    // Query*FeedDiscoverApi.
    switch (options_.load_type) {
      case LoadType::kInitialLoad:
      case LoadType::kManualRefresh:
        network.SendApiRequest<QueryInteractiveFeedDiscoverApi>(
            request, account_info, std::move(request_metadata),
            base::BindOnce(&LoadStreamTask::QueryApiRequestComplete,
                           GetWeakPtr()));
        break;
      case LoadType::kFeedCloseBackgroundRefresh:
      case LoadType::kBackgroundRefresh:
        network.SendApiRequest<QueryBackgroundFeedDiscoverApi>(
            request, account_info, std::move(request_metadata),
            base::BindOnce(&LoadStreamTask::QueryApiRequestComplete,
                           GetWeakPtr()));
        break;
      case LoadType::kLoadMore:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    // Other requests use GWS.
    NetworkRequestType network_request_type =
        options_.stream_type.IsForSupervisedUser()
            ? NetworkRequestType::kSupervisedFeed
            : NetworkRequestType::kFeedQuery;
    network.SendQueryRequest(
        network_request_type, request, account_info,
        base::BindOnce(&LoadStreamTask::QueryRequestComplete, GetWeakPtr()));
  }
}

void LoadStreamTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  ProcessNetworkResponse<feedwire::Response>(std::move(result.response_body),
                                             std::move(result.response_info));
}

void LoadStreamTask::QueryApiRequestComplete(
    FeedNetwork::ApiResult<feedwire::Response> result) {
  ProcessNetworkResponse<feedwire::Response>(std::move(result.response_body),
                                             std::move(result.response_info));
}

template <typename Response>
void LoadStreamTask::ProcessNetworkResponse(
    std::unique_ptr<Response> response_body,
    NetworkResponseInfo response_info) {
  latencies_->StepComplete(LoadLatencyTimes::kQueryRequest);

  if (options_.load_type != LoadType::kBackgroundRefresh) {
    GetLaunchReliabilityLogger().LogRequestSent(
        network_request_id_, response_info.loader_start_time_ticks);
  }

  network_response_info_ = response_info;

  LaunchResult network_status =
      LaunchResultFromNetworkInfo(response_info, response_body != nullptr);
  if (network_status.load_stream_status != LoadStreamStatus::kNoStatus)
    return RequestFinished(network_status);

  RefreshResponseData response_data =
      stream_->GetWireResponseTranslator().TranslateWireResponse(
          *response_body, StreamModelUpdateRequest::Source::kNetworkUpdate,
          response_info.account_info, base::Time::Now());
  server_send_timestamp_ns_ =
      feedstore::ToTimestampNanos(response_data.server_response_sent_timestamp);
  server_receive_timestamp_ns_ = feedstore::ToTimestampNanos(
      response_data.server_request_received_timestamp);

  if (!response_data.model_update_request) {
    return RequestFinished(
        {LoadStreamStatus::kProtoTranslationFailed,
         feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_OTHER});
  }

  loaded_new_content_from_network_ = true;
  content_ids_ =
      feedstore::GetContentIds(response_data.model_update_request->stream_data);

  stream_->GetStore().OverwriteStream(
      options_.stream_type,
      std::make_unique<StreamModelUpdateRequest>(
          *response_data.model_update_request),
      base::DoNothing());

  const bool fetched_content_has_notice_card =
      response_data.model_update_request->stream_data
          .privacy_notice_fulfilled();
  feed::prefs::SetLastFetchHadNoticeCard(*stream_->profile_prefs(),
                                         fetched_content_has_notice_card);
  MetricsReporter::NoticeCardFulfilled(fetched_content_has_notice_card);

  feedstore::Metadata updated_metadata = stream_->GetMetadata();
  SetLastFetchTime(updated_metadata, options_.stream_type,
                   response_data.last_fetch_timestamp);
  SetLastServerResponseTime(updated_metadata, options_.stream_type,
                            response_data.server_response_sent_timestamp);
  updated_metadata.set_web_and_app_activity_enabled(
      response_data.web_and_app_activity_enabled);
  updated_metadata.set_discover_personalization_enabled(
      response_data.discover_personalization_enabled);
  feedstore::MaybeUpdateSessionId(updated_metadata, response_data.session_id);
  if (response_data.content_lifetime) {
    feedstore::SetContentLifetime(updated_metadata, options_.stream_type,
                                  *response_data.content_lifetime);
  }
  stream_->SetMetadata(std::move(updated_metadata));
  if (response_data.experiments)
    experiments_ = *response_data.experiments;

  if (options_.load_type != LoadType::kBackgroundRefresh) {
    update_request_ = std::move(response_data.model_update_request);
  }

  request_schedule_ = std::move(response_data.request_schedule);
  RequestFinished({LoadStreamStatus::kLoadedFromNetwork,
                   feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED});
}

void LoadStreamTask::RequestFinished(LaunchResult result) {
  if (options_.load_type != LoadType::kBackgroundRefresh) {
    if (network_response_info_->status_code > 0) {
      GetLaunchReliabilityLogger().LogResponseReceived(
          network_request_id_, server_receive_timestamp_ns_,
          server_send_timestamp_ns_, network_response_info_->fetch_time_ticks);
    }
    GetLaunchReliabilityLogger().LogRequestFinished(
        network_request_id_, network_response_info_->status_code);
  }
  Done(result);
}

void LoadStreamTask::Done(LaunchResult launch_result) {
  // If the network load fails, but there is stale content in the store, use
  // that stale content.
  if (stale_store_state_ && !update_request_) {
    update_request_ = std::move(stale_store_state_);
    launch_result.load_stream_status =
        LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure;
    launch_result.launch_result =
        feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED;
  }
  Result result;
  result.stream_type = options_.stream_type;
  result.load_from_store_status = load_from_store_status_;
  result.stored_content_age = stored_content_age_;
  result.content_ids = content_ids_;
  result.final_status = launch_result.load_stream_status;
  result.load_type = options_.load_type;
  result.update_request = std::move(update_request_);
  result.request_schedule = std::move(request_schedule_);
  result.network_response_info = network_response_info_;
  result.loaded_new_content_from_network = loaded_new_content_from_network_;
  result.latencies = std::move(latencies_);
  result.upload_actions_result = std::move(upload_actions_result_);
  result.experiments = experiments_;
  result.launch_result = launch_result.launch_result;
  result.single_feed_entry_point = options_.single_feed_entry_point;
  std::move(done_callback_).Run(std::move(result));
  TaskComplete();
}

LaunchReliabilityLogger& LoadStreamTask::GetLaunchReliabilityLogger() const {
  return stream_->GetLaunchReliabilityLogger(options_.stream_type);
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
            << result.loaded_new_content_from_network
            << " single_feed_entry_point=" << result.single_feed_entry_point
            << "}";
}

}  // namespace feed
