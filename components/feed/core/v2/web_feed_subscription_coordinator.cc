// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/web_feed_subscriptions/fetch_subscribed_web_feeds_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_metadata_model.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_subscription_model.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/task/closure_task.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {
WebFeedMetadata MakeWebFeedMetadata(
    WebFeedSubscriptionStatus subscribe_status,
    const feedstore::WebFeedInfo& web_feed_info) {
  WebFeedMetadata result;
  result.web_feed_id = web_feed_info.web_feed_id();
  result.availability_status =
      static_cast<WebFeedAvailabilityStatus>(web_feed_info.state());

  if (!web_feed_info.rss_uri().empty()) {
    result.publisher_url = GURL(web_feed_info.rss_uri());
  } else {
    result.publisher_url = GURL(web_feed_info.visit_uri());
  }
  result.title = web_feed_info.title();
  result.subscription_status = subscribe_status;
  result.favicon_url = GURL(web_feed_info.favicon().url());
  return result;
}

WebFeedMetadata MakeWebFeedMetadata(WebFeedSubscriptionStatus subscribe_status,
                                    std::string web_feed_id = std::string()) {
  WebFeedMetadata result;
  result.web_feed_id = std::move(web_feed_id);
  result.subscription_status = subscribe_status;
  return result;
}

bool IsTerminalStatus(WebFeedSubscriptionRequestStatus status) {
  switch (status) {
    case WebFeedSubscriptionRequestStatus::kUnknown:
      return false;
    case WebFeedSubscriptionRequestStatus::kSuccess:
      return true;
    case WebFeedSubscriptionRequestStatus::kFailedOffline:
      return false;
    case WebFeedSubscriptionRequestStatus::kFailedTooManySubscriptions:
      return true;
    case WebFeedSubscriptionRequestStatus::kFailedUnknownError:
      return false;
    case WebFeedSubscriptionRequestStatus::
        kAbortWebFeedSubscriptionPendingClearAll:
      return true;
  }
}

}  // namespace

WebFeedSubscriptionCoordinator::HooksForTesting::HooksForTesting() = default;
WebFeedSubscriptionCoordinator::HooksForTesting::~HooksForTesting() = default;

WebFeedSubscriptionCoordinator::WebFeedSubscriptionCoordinator(
    Delegate* delegate,
    FeedStream* feed_stream)
    : delegate_(delegate),
      feed_stream_(feed_stream),
      datastore_provider_(&feed_stream->GetGlobalXsurfaceDatastore()) {
  base::TimeDelta delay = GetFeedConfig().fetch_web_feed_info_delay;
  if (IsSignedInAndWebFeedsEnabled() && !delay.is_zero()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsIfStale,
            GetWeakPtr()),
        delay);
    base::OnceClosure do_nothing = base::DoNothing();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsIfStale,
            GetWeakPtr(), std::move(do_nothing)),
        delay);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebFeedSubscriptionCoordinator::RetryPendingOperations,
                       GetWeakPtr()),
        delay);
  }
}

bool WebFeedSubscriptionCoordinator::IsSignedInAndWebFeedsEnabled() const {
  return feed_stream_->IsEnabledAndVisible() &&
         feed_stream_->IsWebFeedEnabled() && feed_stream_->IsSignedIn();
}

WebFeedSubscriptionCoordinator::~WebFeedSubscriptionCoordinator() = default;

void WebFeedSubscriptionCoordinator::Populate(
    const FeedStore::WebFeedStartupData& startup_data) {
  index_.Populate(startup_data.recommended_feed_index);
  index_.Populate(startup_data.subscribed_web_feeds);
  metadata_model_ = std::make_unique<WebFeedMetadataModel>(
      &feed_stream_->GetStore(), std::move(startup_data.pending_operations));
  populated_ = true;

  if (IsSignedInAndWebFeedsEnabled()) {
    delegate_->RegisterFollowingFeedFollowCountFieldTrial(
        startup_data.subscribed_web_feeds.feeds_size());
  }

  SubscriptionsChanged();

  auto on_populated = std::move(on_populated_);
  for (base::OnceClosure& callback : on_populated) {
    std::move(callback).Run();
  }
}

void WebFeedSubscriptionCoordinator::ClearAllFinished() {
  if (hooks_for_testing_)
    hooks_for_testing_->before_clear_all.Run();

  token_generator_.Reset();
  index_.Clear();
  if (metadata_model_) {
    metadata_model_ = std::make_unique<WebFeedMetadataModel>(
        &feed_stream_->GetStore(),
        std::vector<feedstore::PendingWebFeedOperation>());
  }
  if (model_) {
    model_ = std::make_unique<WebFeedSubscriptionModel>(
        &feed_stream_->GetStore(), &index_, &recent_unsubscribed_,
        feedstore::SubscribedWebFeeds(), metadata_model_.get());
  }
  FetchRecommendedWebFeedsIfStale();
  FetchSubscribedWebFeedsIfStale(base::DoNothing());
  SubscriptionsChanged();

  if (hooks_for_testing_)
    hooks_for_testing_->after_clear_all.Run();
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const WebFeedPageInformation& page_info,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  EnqueueInFlightChange(/*subscribing=*/true,
                        WebFeedInFlightChangeStrategy::kNotDurableRequest,
                        change_reason, page_info,
                        /*info=*/std::nullopt);
  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart,
      base::Unretained(this), page_info, change_reason, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart(
    const WebFeedPageInformation& page_info,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);

  SubscribeToWebFeedTask::Request request;
  request.page_info = page_info;
  request.change_reason = change_reason;
  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<SubscribeToWebFeedTask>(
          feed_stream_, token_generator_.Token(), std::move(request),
          base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                         base::Unretained(this), std::move(callback),
                         /*followed_with_id=*/false)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const std::string& web_feed_id,
    bool is_durable_request,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  FollowWebFeedInternal(web_feed_id,
                        is_durable_request
                            ? WebFeedInFlightChangeStrategy::kNewDurableRequest
                            : WebFeedInFlightChangeStrategy::kNotDurableRequest,
                        change_reason, std::move(callback));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedInternal(
    const std::string& web_feed_id,
    WebFeedInFlightChangeStrategy strategy,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  feedstore::WebFeedInfo info;
  info.set_web_feed_id(web_feed_id);
  EnqueueInFlightChange(/*subscribing=*/true, strategy, change_reason,
                        /*page_information=*/std::nullopt, info);
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart,
                     base::Unretained(this), web_feed_id, strategy,
                     change_reason, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::UpdatePendingOperationBeforeAttempt(
    const std::string& web_feed_id,
    WebFeedInFlightChangeStrategy strategy,
    feedstore::PendingWebFeedOperation::Kind kind,
    feedwire::webfeed::WebFeedChangeReason change_reason) {
  DCHECK(metadata_model_);
  switch (strategy) {
    case WebFeedInFlightChangeStrategy::kNewDurableRequest:
      metadata_model_->AddPendingOperation(kind, web_feed_id, change_reason);
      break;
    case WebFeedInFlightChangeStrategy::kNotDurableRequest:
      // Let other user actions override previous requests to follow or
      // unfollow.
      metadata_model_->RemovePendingOperationsForWebFeed(web_feed_id);
      break;
    case WebFeedInFlightChangeStrategy::kRetry:
      break;
    case WebFeedInFlightChangeStrategy::kPending:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart(
    const std::string& web_feed_id,
    WebFeedInFlightChangeStrategy strategy,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);
  UpdatePendingOperationBeforeAttempt(
      web_feed_id, strategy, feedstore::PendingWebFeedOperation::SUBSCRIBE,
      change_reason);
  WebFeedSubscriptionInfo info = model_->GetSubscriptionInfo(web_feed_id);
  SubscribeToWebFeedTask::Request request;
  request.web_feed_id = web_feed_id;
  request.change_reason = change_reason;

  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<SubscribeToWebFeedTask>(
          feed_stream_, token_generator_.Token(), std::move(request),
          base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                         base::Unretained(this), std::move(callback),
                         /*followed_with_id=*/true)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedComplete(
    base::OnceCallback<void(FollowWebFeedResult)> callback,
    bool followed_with_id,
    SubscribeToWebFeedTask::Result result) {
  DCHECK(model_);
  WebFeedInFlightChange change = DequeueInflightChange();
  if (change.strategy != WebFeedInFlightChangeStrategy::kNotDurableRequest) {
    if (IsTerminalStatus(result.request_status)) {
      metadata_model_->RemovePendingOperationsForWebFeed(
          change.web_feed_info->web_feed_id());
    } else {
      metadata_model_->RecordPendingOperationsForWebFeedAttempt(
          change.web_feed_info->web_feed_id());
    }
  }

  if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
    model_->OnSubscribed(result.web_feed_info);
    feed_stream_->SetStreamStale(StreamType(StreamKind::kFollowing), true);
  }

  SubscriptionsChanged();

  WebFeedSubscriptionInfo info =
      model_->GetSubscriptionInfo(result.followed_web_feed_id);
  FollowWebFeedResult callback_result;
  callback_result.web_feed_metadata =
      MakeWebFeedMetadata(info.status, info.web_feed_info);
  callback_result.web_feed_metadata.is_recommended =
      index_.IsRecommended(result.followed_web_feed_id);
  callback_result.request_status = result.request_status;
  callback_result.subscription_count = index_.SubscriptionCount();
  callback_result.change_reason = result.change_reason;
  feed_stream_->GetMetricsReporter().OnFollowAttempt(followed_with_id,
                                                     callback_result);
  std::move(callback).Run(std::move(callback_result));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeed(
    const std::string& web_feed_id,
    bool is_durable_request,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(UnfollowWebFeedResult)> callback) {
  UnfollowWebFeedInternal(
      web_feed_id,
      is_durable_request ? WebFeedInFlightChangeStrategy::kNewDurableRequest
                         : WebFeedInFlightChangeStrategy::kNotDurableRequest,
      change_reason, std::move(callback));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedInternal(
    const std::string& web_feed_id,
    WebFeedInFlightChangeStrategy strategy,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(UnfollowWebFeedResult)> callback) {
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::UnfollowWebFeedStart,
                     base::Unretained(this), web_feed_id, strategy,
                     change_reason, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedStart(
    const std::string& web_feed_id,
    WebFeedInFlightChangeStrategy strategy,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(UnfollowWebFeedResult)> callback) {
  UpdatePendingOperationBeforeAttempt(
      web_feed_id, strategy, feedstore::PendingWebFeedOperation::UNSUBSCRIBE,
      change_reason);

  WebFeedSubscriptionInfo info_lookup =
      model_->GetSubscriptionInfo(web_feed_id);

  feedstore::WebFeedInfo info = info_lookup.web_feed_info;
  info.set_web_feed_id(web_feed_id);
  EnqueueInFlightChange(/*subscribing=*/false, strategy, change_reason,
                        /*page_information=*/std::nullopt, info);

  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<UnsubscribeFromWebFeedTask>(
          feed_stream_, token_generator_.Token(), web_feed_id, change_reason,
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete,
              base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete(
    base::OnceCallback<void(UnfollowWebFeedResult)> callback,
    UnsubscribeFromWebFeedTask::Result result) {
  if (!result.unsubscribed_feed_name.empty()) {
    model_->OnUnsubscribed(result.unsubscribed_feed_name);
    feed_stream_->SetStreamStale(StreamType(StreamKind::kFollowing), true);
  }

  WebFeedInFlightChange change = DequeueInflightChange();

  if (change.strategy != WebFeedInFlightChangeStrategy::kNotDurableRequest) {
    if (IsTerminalStatus(result.request_status)) {
      metadata_model_->RemovePendingOperationsForWebFeed(
          change.web_feed_info->web_feed_id());
    } else {
      metadata_model_->RecordPendingOperationsForWebFeedAttempt(
          change.web_feed_info->web_feed_id());
    }
  }

  SubscriptionsChanged();

  UnfollowWebFeedResult callback_result;
  callback_result.request_status = result.request_status;
  callback_result.subscription_count = index_.SubscriptionCount();
  feed_stream_->GetMetricsReporter().OnUnfollowAttempt(callback_result);
  std::move(callback).Run(callback_result);
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForPage(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  if (!model_ && !loading_model_) {
    // No model loaded, try to answer the request without it.
    WebFeedIndex::Entry entry = index_.FindWebFeed(page_info);
    if (!entry.followed()) {
      LookupWebFeedDataAndRespond(
          entry.web_feed_id, /*maybe_page_info=*/nullptr, std::move(callback));
      return;
    }
  }

  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FindWebFeedInfoForPageStart,
      base::Unretained(this), page_info, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForPageStart(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  DCHECK(model_);
  LookupWebFeedDataAndRespond(std::string(), &page_info, std::move(callback));
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForWebFeedId(
    const std::string& web_feed_id,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  if (!model_ && !loading_model_) {
    // No model loaded, try to answer the request without it.
    WebFeedIndex::Entry entry = index_.FindWebFeed(web_feed_id);
    if (!entry.followed()) {
      LookupWebFeedDataAndRespond(web_feed_id,
                                  /*maybe_page_info=*/nullptr,
                                  std::move(callback));
      return;
    }
  }
  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FindWebFeedInfoForWebFeedIdStart,
      base::Unretained(this), web_feed_id, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForWebFeedIdStart(
    const std::string& web_feed_id,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  DCHECK(model_);
  LookupWebFeedDataAndRespond(web_feed_id,
                              /*maybe_page_info=*/nullptr, std::move(callback));
}

WebFeedSubscriptionStatus
WebFeedSubscriptionCoordinator::GetWebFeedSubscriptionStatus(
    const std::string& web_feed_id) const {
  const WebFeedInFlightChange* in_flight_change =
      FindInflightChange(web_feed_id, nullptr);
  if (in_flight_change) {
    return in_flight_change->subscribing
               ? WebFeedSubscriptionStatus::kSubscribeInProgress
               : WebFeedSubscriptionStatus::kUnsubscribeInProgress;
  }
  return index_.FindWebFeed(web_feed_id).followed()
             ? WebFeedSubscriptionStatus::kSubscribed
             : WebFeedSubscriptionStatus::kNotSubscribed;
}

void WebFeedSubscriptionCoordinator::LookupWebFeedDataAndRespond(
    const std::string& web_feed_id,
    const WebFeedPageInformation* maybe_page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  WebFeedSubscriptionStatus subscription_status =
      WebFeedSubscriptionStatus::kUnknown;
  // Override status and `web_feed_info` if there's an in-flight operation.
  std::string id = web_feed_id;
  const WebFeedInFlightChange* in_flight_change =
      FindInflightChange(id, maybe_page_info);
  const feedstore::WebFeedInfo* web_feed_info = nullptr;

  if (in_flight_change) {
    subscription_status =
        in_flight_change->subscribing
            ? WebFeedSubscriptionStatus::kSubscribeInProgress
            : WebFeedSubscriptionStatus::kUnsubscribeInProgress;
    if (in_flight_change->web_feed_info) {
      web_feed_info = &*in_flight_change->web_feed_info;
      if (id.empty())
        id = web_feed_info->web_feed_id();
    }
  }

  WebFeedIndex::Entry entry;
  if (!id.empty()) {
    entry = index_.FindWebFeed(id);
  } else if (maybe_page_info) {
    entry = index_.FindWebFeed(*maybe_page_info);
    if (entry)
      id = entry.web_feed_id;
  }

  // Try using `model_` if it's loaded.
  WebFeedSubscriptionInfo subscription_info;
  if (!web_feed_info && model_ && !id.empty()) {
    subscription_info = model_->GetSubscriptionInfo(id);
    if (subscription_info.status != WebFeedSubscriptionStatus::kUnknown &&
        !subscription_info.web_feed_info.web_feed_id().empty()) {
      web_feed_info = &subscription_info.web_feed_info;
      if (subscription_status == WebFeedSubscriptionStatus::kUnknown)
        subscription_status = subscription_info.status;
    }
  }

  // If status is still unknown, it's now known to be unsubscribed.
  if (subscription_status == WebFeedSubscriptionStatus::kUnknown)
    subscription_status = WebFeedSubscriptionStatus::kNotSubscribed;

  // If we have `WebFeedInfo`, use it to make a reply.
  if (web_feed_info) {
    WebFeedMetadata metadata =
        MakeWebFeedMetadata(subscription_status, *web_feed_info);
    if (entry) {
      metadata.is_recommended =
          entry.is_recommended || index_.IsRecommended(id);
    }
    std::move(callback).Run(std::move(metadata));
    return;
  }

  // Reply with just status and name if it's not a recommended Web Feed.
  if (!entry.recommended()) {
    std::move(callback).Run(MakeWebFeedMetadata(subscription_status, id));
    return;
  }

  // Look up recommended data from store, and return.

  auto adapt_callback =
      [](const std::string& web_feed_id,
         WebFeedSubscriptionStatus subscription_status,
         base::OnceCallback<void(WebFeedMetadata)> callback,
         std::unique_ptr<feedstore::WebFeedInfo> web_feed_info) {
        WebFeedMetadata result;
        if (web_feed_info) {
          result = MakeWebFeedMetadata(subscription_status, *web_feed_info);
          result.is_recommended = true;
        } else {
          // This branch might be hit if the recommended list was updated before
          // the post task to storage completes.
          result.web_feed_id = web_feed_id;
        }
        std::move(callback).Run(std::move(result));
      };

  feed_stream_->GetStore().ReadRecommendedWebFeedInfo(
      entry.web_feed_id,
      base::BindOnce(adapt_callback, entry.web_feed_id, subscription_status,
                     std::move(callback)));
}

void WebFeedSubscriptionCoordinator::WithModel(base::OnceClosure closure) {
  if (model_) {
    std::move(closure).Run();
  } else {
    when_model_loads_.push_back(std::move(closure));
    if (!loading_model_) {
      loading_model_ = true;
      loading_token_ = token_generator_.Token();
      feed_stream_->GetTaskQueue().AddTask(
          FROM_HERE,
          std::make_unique<offline_pages::ClosureTask>(base::BindOnce(
              &WebFeedSubscriptionCoordinator::ReadWebFeedStartupDataTask,
              base::Unretained(this))));
    }
  }
}

void WebFeedSubscriptionCoordinator::ReadWebFeedStartupDataTask() {
  DCHECK(populated_);
  feed_stream_->GetStore().ReadWebFeedStartupData(
      base::BindOnce(&WebFeedSubscriptionCoordinator::ModelDataLoaded,
                     base::Unretained(this)));
}

void WebFeedSubscriptionCoordinator::ModelDataLoaded(
    FeedStore::WebFeedStartupData startup_data) {
  DCHECK(loading_model_);
  DCHECK(metadata_model_);
  DCHECK(!model_);
  loading_model_ = false;
  if (!loading_token_) {
    // ClearAll happened, so ignore any stored data and allow the model to load.
    startup_data = {};
  }

  // TODO(crbug.com/40158714): Don't need recommended feed data, we could add a
  // new function on FeedStore to fetch only subscribed feed data.
  model_ = std::make_unique<WebFeedSubscriptionModel>(
      &feed_stream_->GetStore(), &index_, &recent_unsubscribed_,
      std::move(startup_data.subscribed_web_feeds), metadata_model_.get());
  for (base::OnceClosure& callback : when_model_loads_) {
    std::move(callback).Run();
  }
  when_model_loads_.clear();
}

void WebFeedSubscriptionCoordinator::EnqueueInFlightChange(
    bool subscribing,
    WebFeedInFlightChangeStrategy strategy,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    std::optional<WebFeedPageInformation> page_information,
    std::optional<feedstore::WebFeedInfo> info) {
  WebFeedInFlightChange change;
  change.token = token_generator_.Token();
  change.subscribing = subscribing;
  change.strategy = strategy;
  change.change_reason = change_reason;
  change.page_information = std::move(page_information);
  change.web_feed_info = std::move(info);
  in_flight_changes_.push_back(std::move(change));
  SubscriptionsChanged();
}

WebFeedInFlightChange WebFeedSubscriptionCoordinator::DequeueInflightChange() {
  // O(N), but N is very small.
  DCHECK(!in_flight_changes_.empty());
  auto top = std::move(in_flight_changes_.front());
  in_flight_changes_.erase(in_flight_changes_.begin());
  return top;
}

// Return the last in-flight change which matches either `id` or
// `maybe_page_info`, ignoring changes before ClearAll.
const WebFeedInFlightChange* WebFeedSubscriptionCoordinator::FindInflightChange(
    const std::string& web_feed_id,
    const WebFeedPageInformation* maybe_page_info) const {
  const WebFeedInFlightChange* result = nullptr;
  if (metadata_model_) {
    result = metadata_model_->FindInFlightChange(web_feed_id);
  }

  for (const WebFeedInFlightChange& change : in_flight_changes_) {
    if (!change.token)
      continue;
    if ((maybe_page_info && change.page_information &&
         change.page_information->url() == maybe_page_info->url()) ||
        (!web_feed_id.empty() && change.web_feed_info &&
         change.web_feed_info->web_feed_id() == web_feed_id)) {
      result = &change;
    }
  }
  return result;
}

void WebFeedSubscriptionCoordinator::GetAllSubscriptions(
    base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback) {
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::GetAllSubscriptionsStart,
                     base::Unretained(this), std::move(callback)));
}

void WebFeedSubscriptionCoordinator::GetAllSubscriptionsStart(
    base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback) {
  DCHECK(model_);
  std::vector<WebFeedMetadata> result;
  for (const feedstore::WebFeedInfo& info : model_->subscriptions()) {
    WebFeedSubscriptionStatus status = WebFeedSubscriptionStatus::kSubscribed;
    const WebFeedInFlightChange* change =
        FindInflightChange(info.web_feed_id(), /*maybe_page_info=*/nullptr);
    if (change && !change->subscribing) {
      status = WebFeedSubscriptionStatus::kUnsubscribeInProgress;
    }
    result.push_back(MakeWebFeedMetadata(status, info));
  }
  std::move(callback).Run(std::move(result));
}

void WebFeedSubscriptionCoordinator::RefreshSubscriptions(
    base::OnceCallback<void(RefreshResult)> callback) {
  RetryPendingOperations();

  on_refresh_subscriptions_.push_back(std::move(callback));

  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsStart,
      base::Unretained(this)));
}

WebFeedSubscriptionInfo WebFeedSubscriptionCoordinator::FindSubscriptionInfo(
    const WebFeedPageInformation& page_info) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(index_.FindWebFeed(page_info).web_feed_id);
}
WebFeedSubscriptionInfo
WebFeedSubscriptionCoordinator::FindSubscriptionInfoById(
    const std::string& web_feed_id) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(web_feed_id);
}

void WebFeedSubscriptionCoordinator::RefreshRecommendedFeeds(
    base::OnceCallback<void(RefreshResult)> callback) {
  on_refresh_recommended_feeds_.push_back(std::move(callback));
  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsStart,
      base::Unretained(this)));
}

void WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsIfStale() {
  if (!IsSignedInAndWebFeedsEnabled())
    return;

  base::TimeDelta staleness =
      base::Time::Now() - index_.GetRecommendedFeedsUpdateTime();
  if (staleness > GetFeedConfig().recommended_feeds_staleness_threshold ||
      staleness < -base::Hours(1)) {
    RefreshRecommendedFeeds(base::DoNothing());
  }
}

void WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsStart() {
  DCHECK(model_);
  if (fetching_recommended_web_feeds_ || !IsSignedInAndWebFeedsEnabled())
    return;
  fetching_recommended_web_feeds_ = true;
  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<FetchRecommendedWebFeedsTask>(
          feed_stream_, token_generator_.Token(),
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsComplete,
              base::Unretained(this))));
}

void WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsComplete(
    FetchRecommendedWebFeedsTask::Result result) {
  DCHECK(model_);
  feed::WebFeedSubscriptions::RefreshResult refresh_result;
  refresh_result.success = false;
  fetching_recommended_web_feeds_ = false;
  feed_stream_->GetMetricsReporter().RefreshRecommendedWebFeedsAttempted(
      result.status, result.recommended_web_feeds.size());
  if (result.status == WebFeedRefreshStatus::kSuccess) {
    refresh_result.success = true;
    model_->UpdateRecommendedFeeds(std::move(result.recommended_web_feeds));
  }
  CallRefreshRecommendedFeedsCompleteCallbacks(refresh_result);
}

void WebFeedSubscriptionCoordinator::
    CallRefreshRecommendedFeedsCompleteCallbacks(RefreshResult result) {
  std::vector<base::OnceCallback<void(RefreshResult)>> callbacks;
  on_refresh_recommended_feeds_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

void WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsIfStale(
    base::OnceClosure callback) {
  if (!populated_) {
    on_populated_.push_back(base::BindOnce(
        &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsIfStale,
        base::Unretained(this), std::move(callback)));
    return;
  }

  if (!IsSignedInAndWebFeedsEnabled()) {
    std::move(callback).Run();
    return;
  }

  base::TimeDelta staleness =
      base::Time::Now() - index_.GetSubscribedFeedsUpdateTime();
  if (staleness > GetFeedConfig().subscribed_feeds_staleness_threshold ||
      staleness < -base::Hours(1)) {
    fetching_subscribed_web_feeds_because_stale_ = true;
    auto callback_adaptor = [](base::OnceClosure callback, RefreshResult) {
      std::move(callback).Run();
    };
    on_refresh_subscriptions_.push_back(
        base::BindOnce(callback_adaptor, std::move(callback)));

    WithModel(base::BindOnce(
        &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsStart,
        base::Unretained(this)));
  } else {
    std::move(callback).Run();
  }
}

void WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsStart() {
  DCHECK(model_);
  if (fetching_subscribed_web_feeds_)
    return;
  if (!IsSignedInAndWebFeedsEnabled()) {
    CallRefreshCompleteCallbacks({});
    return;
  }
  fetching_subscribed_web_feeds_ = true;
  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<FetchSubscribedWebFeedsTask>(
          feed_stream_, token_generator_.Token(),
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsComplete,
              base::Unretained(this))));
}

void WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsComplete(
    FetchSubscribedWebFeedsTask::Result result) {
  if (result.status ==
      WebFeedRefreshStatus::kAbortFetchWebFeedPendingClearAll) {
    // Retry the task if it was cancelled by a ClearAll, and don't call the
    // callbacks.
    fetching_subscribed_web_feeds_ = false;
    FetchSubscribedWebFeedsStart();
    return;
  }

  feed_stream_->GetMetricsReporter().RefreshSubscribedWebFeedsAttempted(
      fetching_subscribed_web_feeds_because_stale_, result.status,
      result.subscribed_web_feeds.size());

  DCHECK(model_);
  fetching_subscribed_web_feeds_because_stale_ = false;
  fetching_subscribed_web_feeds_ = false;

  if (result.status == WebFeedRefreshStatus::kSuccess)
    model_->UpdateSubscribedFeeds(std::move(result.subscribed_web_feeds));

  SubscriptionsChanged();

  CallRefreshCompleteCallbacks(
      RefreshResult{result.status == WebFeedRefreshStatus::kSuccess});
}

void WebFeedSubscriptionCoordinator::CallRefreshCompleteCallbacks(
    RefreshResult result) {
  std::vector<base::OnceCallback<void(RefreshResult)>> callbacks;
  on_refresh_subscriptions_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

void WebFeedSubscriptionCoordinator::IsWebFeedSubscriber(
    base::OnceCallback<void(bool)> callback) {
  FetchSubscribedWebFeedsIfStale(
      base::BindOnce(&WebFeedSubscriptionCoordinator::IsWebFeedSubscriberDone,
                     base::Unretained(this), std::move(callback)));
}

void WebFeedSubscriptionCoordinator::SubscribedWebFeedCount(
    base::OnceCallback<void(int)> callback) {
  FetchSubscribedWebFeedsIfStale(base::BindOnce(
      &WebFeedSubscriptionCoordinator::SubscribedWebFeedCountDone,
      base::Unretained(this), std::move(callback)));
}

std::vector<std::pair<std::string, WebFeedSubscriptionStatus>>
WebFeedSubscriptionCoordinator::GetAllWebFeedSubscriptionStatus() const {
  // Collect all the WebFeed IDs, using kNotSubscribed as a placeholder.
  std::vector<std::pair<std::string, WebFeedSubscriptionStatus>> result;
  for (const WebFeedInFlightChange& change : in_flight_changes_) {
    if (!change.web_feed_info || change.web_feed_info->web_feed_id().empty())
      continue;
    result.emplace_back(change.web_feed_info->web_feed_id(),
                        WebFeedSubscriptionStatus::kNotSubscribed);
  }

  for (const WebFeedMetadataModel::Operation& op :
       metadata_model_->pending_operations()) {
    result.emplace_back(op.operation.web_feed_id(),
                        WebFeedSubscriptionStatus::kNotSubscribed);
  }
  for (const WebFeedIndex::Entry& entry : index_.GetSubscribedEntries()) {
    result.emplace_back(entry.web_feed_id,
                        WebFeedSubscriptionStatus::kNotSubscribed);
  }

  // Remove duplicates, and fetch WebFeed status.
  base::ranges::sort(result);
  result.erase(base::ranges::unique(result), result.end());
  for (auto& entry : result) {
    entry.second = GetWebFeedSubscriptionStatus(entry.first);
  }

  return result;
}

void WebFeedSubscriptionCoordinator::SubscriptionsChanged() {
  if (!populated_)
    return;
  datastore_provider_.Update(GetAllWebFeedSubscriptionStatus());
}

void WebFeedSubscriptionCoordinator::DumpStateForDebugging(std::ostream& os) {
  if (populated_) {
    index_.DumpStateForDebugging(os);
  } else {
    os << "index not populated";
  }
  os << '\n';
}

void WebFeedSubscriptionCoordinator::IsWebFeedSubscriberDone(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(IsSignedInAndWebFeedsEnabled() &&
                          index_.HasSubscriptions());
}

void WebFeedSubscriptionCoordinator::SubscribedWebFeedCountDone(
    base::OnceCallback<void(int)> callback) {
  std::move(callback).Run(
      IsSignedInAndWebFeedsEnabled() ? index_.SubscriptionCount() : 0);
}

void WebFeedSubscriptionCoordinator::RetryPendingOperations() {
  // Since metadata is loaded directly after startup, this is usually available.
  // If not, just give up.
  if (!metadata_model_) {
    DLOG(WARNING) << "RetryPendingOperations running before loading metadata.";
    return;
  }

  // Skip if offline.
  if (feed_stream_->IsOffline())
    return;

  for (const WebFeedMetadataModel::Operation& op :
       metadata_model_->pending_operations()) {
    switch (op.operation.kind()) {
      case feedstore::PendingWebFeedOperation::SUBSCRIBE:
        FollowWebFeedInternal(op.operation.web_feed_id(),
                              WebFeedInFlightChangeStrategy::kRetry,
                              op.operation.change_reason(), base::DoNothing());
        break;
      case feedstore::PendingWebFeedOperation::UNSUBSCRIBE:
        UnfollowWebFeedInternal(
            op.operation.web_feed_id(), WebFeedInFlightChangeStrategy::kRetry,
            op.operation.change_reason(), base::DoNothing());
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported operation kind " << op.operation.kind();
    }
  }
}

std::string WebFeedSubscriptionCoordinator::DescribeStateForTesting() const {
  std::stringstream ss;
  if (!populated_) {
    ss << "Not yet populated\n";
  }
  if (metadata_model_) {
    for (const WebFeedMetadataModel::Operation& op :
         metadata_model_->pending_operations()) {
      ss << "Pending " << op << '\n';
    }
  }

  for (const WebFeedInFlightChange& change : in_flight_changes_) {
    DCHECK_NE(change.strategy, WebFeedInFlightChangeStrategy::kPending);
    ss << change << '\n';
  }
  for (const WebFeedIndex::Entry& entry :
       index_.GetSubscribedEntriesForTesting()) {
    ss << "Subscribed to " << entry.web_feed_id << '\n';
  }
  return ss.str();
}

std::vector<feedstore::PendingWebFeedOperation>
WebFeedSubscriptionCoordinator::GetPendingOperationStateForTesting() {
  std::vector<feedstore::PendingWebFeedOperation> result;
  for (const WebFeedMetadataModel::Operation& op :
       metadata_model_->pending_operations()) {
    result.push_back(op.operation);
  }
  return result;
}

void WebFeedSubscriptionCoordinator::QueryWebFeed(
    const GURL& url,
    base::OnceCallback<void(QueryWebFeedResult)> callback) {
  // TODO(crbug.com/40889279) Combine subscription status into result callback.
  // This would require binding a start call via WithModel and updating the
  // local state to match the result from the server,
  QueryWebFeedTask::Request request;
  request.web_feed_url = url;

  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<QueryWebFeedTask>(
          feed_stream_, token_generator_.Token(), std::move(request),
          base::BindOnce(&WebFeedSubscriptionCoordinator::QueryWebFeedComplete,
                         base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::QueryWebFeedId(
    const std::string& id,
    base::OnceCallback<void(QueryWebFeedResult)> callback) {
  // TODO(crbug.com/40889279) Combine subscription status into result callback.
  // This would require binding a start call via WithModel and updating the
  // local state to match the result from the server,
  QueryWebFeedTask::Request request;
  request.web_feed_id = id;

  feed_stream_->GetTaskQueue().AddTask(
      FROM_HERE,
      std::make_unique<QueryWebFeedTask>(
          feed_stream_, token_generator_.Token(), std::move(request),
          base::BindOnce(&WebFeedSubscriptionCoordinator::QueryWebFeedComplete,
                         base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::QueryWebFeedComplete(
    base::OnceCallback<void(QueryWebFeedResult)> callback,
    QueryWebFeedResult result) {
  QueryWebFeedResult callback_result;
  callback_result.web_feed_id = result.web_feed_id;
  callback_result.url = result.url;
  callback_result.title = result.title;
  callback_result.request_status = result.request_status;
  feed_stream_->GetMetricsReporter().OnQueryAttempt(callback_result);
  std::move(callback).Run(std::move(callback_result));
}

}  // namespace feed
