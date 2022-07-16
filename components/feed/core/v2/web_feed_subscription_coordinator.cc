// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

#include <memory>
#include <ostream>

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/task/closure_task.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feed {
namespace {
using InFlightChange = feed::internal::InFlightChange;
using SubscriptionInfo = WebFeedSubscriptionCoordinator::SubscriptionInfo;

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

feedstore::WebFeedInfo Remove(
    const std::string& web_feed_id,
    std::vector<feedstore::WebFeedInfo>& feed_info_list) {
  feedstore::WebFeedInfo result;
  for (size_t i = 0; i < feed_info_list.size(); ++i) {
    if (feed_info_list[i].web_feed_id() == web_feed_id) {
      result = std::move(feed_info_list[i]);
      feed_info_list.erase(feed_info_list.begin() + i);
      break;
    }
  }
  return result;
}

}  // namespace

namespace internal {
struct InFlightChange {
  // Either subscribing or unsubscribing.
  bool subscribing = false;
  // Set only when subscribing from a web page.
  absl::optional<WebFeedPageInformation> page_information;
  // We may or may not know about this web feed when subscribing; always known
  // when unsubscribing.
  absl::optional<feedstore::WebFeedInfo> web_feed_info;
};

// An in-memory model of the subscribed web feeds. This should be loaded before
// making any changes to stored web feeds, so that we rarely need to wait on
// other tasks to load the subscribed feeds. Additionally, any changes to stored
// feeds also need to update this in-memory model.
class WebFeedSubscriptionModel {
 public:
  WebFeedSubscriptionModel(
      FeedStore* store,
      WebFeedIndex* index,
      std::vector<feedstore::WebFeedInfo>* recent_subscriptions,
      const feedstore::SubscribedWebFeeds& feeds)
      : store_(store),
        index_(index),
        recent_unsubscribed_(recent_subscriptions) {
    subscriptions_.assign(feeds.feeds().begin(), feeds.feeds().end());
    update_time_millis_ = feeds.update_time_millis();
  }

  SubscriptionInfo GetSubscriptionInfo(const std::string& web_feed_id) {
    SubscriptionInfo result;
    for (const feedstore::WebFeedInfo& info : *recent_unsubscribed_) {
      if (info.web_feed_id() == web_feed_id) {
        result.status = WebFeedSubscriptionStatus::kNotSubscribed;
        result.web_feed_info = info;
        break;
      }
    }

    for (const feedstore::WebFeedInfo& info : subscriptions_) {
      if (info.web_feed_id() == web_feed_id) {
        result.status = WebFeedSubscriptionStatus::kSubscribed;
        result.web_feed_info = info;
        break;
      }
    }

    return result;
  }

  void OnSubscribed(const feedstore::WebFeedInfo& info) {
    Remove(info.web_feed_id(), *recent_unsubscribed_);
    Remove(info.web_feed_id(), subscriptions_);
    subscriptions_.emplace_back(info);
    UpdateIndexAndStore();
  }

  void OnUnsubscribed(const std::string& web_feed_id) {
    feedstore::WebFeedInfo info = Remove(web_feed_id, subscriptions_);
    if (!info.web_feed_id().empty()) {
      recent_unsubscribed_->push_back(std::move(info));
    }
    UpdateIndexAndStore();
  }

  void UpdateIndexAndStore() {
    feedstore::SubscribedWebFeeds state;
    for (const feedstore::WebFeedInfo& info : subscriptions_) {
      *state.add_feeds() = info;
    }
    state.set_update_time_millis(update_time_millis_);
    index_->Populate(state);
    store_->WriteSubscribedFeeds(std::move(state), base::DoNothing());
  }

  // Updates recommended web feeds in both index and store.
  void UpdateRecommendedFeeds(
      std::vector<feedstore::WebFeedInfo> recommended_web_feeds) {
    feedstore::RecommendedWebFeedIndex store_index;
    store_index.set_update_time_millis(
        feedstore::ToTimestampMillis(base::Time::Now()));
    for (const feedstore::WebFeedInfo& info : recommended_web_feeds) {
      feedstore::RecommendedWebFeedIndex::Entry& entry =
          *store_index.add_entries();
      entry.set_web_feed_id(info.web_feed_id());
      *entry.mutable_matchers() = info.matchers();
    }
    index_->Populate(store_index);
    store_->WriteRecommendedFeeds(std::move(store_index),
                                  std::move(recommended_web_feeds),
                                  base::DoNothing());
  }

  // Updates subscribed web feeds in both index and store.
  void UpdateSubscribedFeeds(
      std::vector<feedstore::WebFeedInfo> subscribed_web_feeds) {
    feedstore::SubscribedWebFeeds store_index;
    update_time_millis_ = feedstore::ToTimestampMillis(base::Time::Now());
    store_index.set_update_time_millis(update_time_millis_);
    for (const feedstore::WebFeedInfo& info : subscribed_web_feeds) {
      *store_index.add_feeds() = info;
    }
    index_->Populate(store_index);
    store_->WriteSubscribedFeeds(std::move(store_index), base::DoNothing());
    subscriptions_ = subscribed_web_feeds;
  }

  const std::vector<feedstore::WebFeedInfo>& subscriptions() const {
    return subscriptions_;
  }

 private:
  // Each of these are non-null and guaranteed to remain valid for the lifetime
  // of WebFeedSubscriptionModel.
  FeedStore* store_;
  WebFeedIndex* index_;
  // Owned by WebFeedSubscriptionCoordinator so that memory of recent
  // subscriptions is retained when the model is deleted.
  std::vector<feedstore::WebFeedInfo>* recent_unsubscribed_;

  // The current known state of subscriptions.
  std::vector<feedstore::WebFeedInfo> subscriptions_;
  int64_t update_time_millis_ = 0;
};

}  // namespace internal

WebFeedSubscriptionCoordinator::WebFeedSubscriptionCoordinator(
    Delegate* delegate,
    FeedStream* feed_stream)
    : delegate_(delegate), feed_stream_(feed_stream) {
  base::TimeDelta delay = GetFeedConfig().fetch_web_feed_info_delay;
  if (IsSignedInAndWebFeedsEnabled() && !delay.is_zero()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsIfStale,
            GetWeakPtr()),
        delay);
    base::OnceClosure do_nothing = base::DoNothing();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsIfStale,
            GetWeakPtr(), std::move(do_nothing)),
        delay);
  }
}

bool WebFeedSubscriptionCoordinator::IsSignedInAndWebFeedsEnabled() const {
  return feed_stream_->IsEnabledAndVisible() &&
         base::FeatureList::IsEnabled(kWebFeed) && feed_stream_->IsSignedIn();
}

WebFeedSubscriptionCoordinator::~WebFeedSubscriptionCoordinator() = default;

void WebFeedSubscriptionCoordinator::Populate(
    const FeedStore::WebFeedStartupData& startup_data) {
  index_.Populate(startup_data.recommended_feed_index);
  index_.Populate(startup_data.subscribed_web_feeds);
  populated_ = true;

  if (IsSignedInAndWebFeedsEnabled()) {
    delegate_->RegisterFollowingFeedFollowCountFieldTrial(
        startup_data.subscribed_web_feeds.feeds_size());
  }

  auto on_populated = std::move(on_populated_);
  for (base::OnceClosure& callback : on_populated) {
    std::move(callback).Run();
  }
}

void WebFeedSubscriptionCoordinator::ClearAllFinished() {
  index_.Clear();
  model_.reset();
  FetchRecommendedWebFeedsIfStale();
  FetchSubscribedWebFeedsIfStale(base::DoNothing());
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  EnqueueInFlightChange(/*subscribing=*/true, page_info,
                        /*info=*/absl::nullopt);
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart,
                     base::Unretained(this), page_info, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);
  WebFeedIndex::Entry entry = index_.FindWebFeed(page_info);

  SubscribeToWebFeedTask::Request request;
  request.page_info = page_info;
  feed_stream_->GetTaskQueue().AddTask(std::make_unique<SubscribeToWebFeedTask>(
      feed_stream_, std::move(request),
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                     base::Unretained(this), std::move(callback),
                     /*followed_with_id=*/false)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const std::string& web_feed_id,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  feedstore::WebFeedInfo info;
  info.set_web_feed_id(web_feed_id);
  EnqueueInFlightChange(/*subscribing=*/true,
                        /*page_information=*/absl::nullopt, info);
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart,
                     base::Unretained(this), web_feed_id, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart(
    const std::string& web_feed_id,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);
  SubscriptionInfo info = model_->GetSubscriptionInfo(web_feed_id);
  SubscribeToWebFeedTask::Request request;
  request.web_feed_id = web_feed_id;

  feed_stream_->GetTaskQueue().AddTask(std::make_unique<SubscribeToWebFeedTask>(
      feed_stream_, std::move(request),
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                     base::Unretained(this), std::move(callback),
                     /*followed_with_id=*/true)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedComplete(
    base::OnceCallback<void(FollowWebFeedResult)> callback,
    bool followed_with_id,
    SubscribeToWebFeedTask::Result result) {
  DCHECK(model_);
  DequeueInflightChange();
  if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
    model_->OnSubscribed(result.web_feed_info);
    feed_stream_->SetStreamStale(kWebFeedStream, true);
  }
  SubscriptionInfo info =
      model_->GetSubscriptionInfo(result.followed_web_feed_id);
  FollowWebFeedResult callback_result;
  callback_result.web_feed_metadata =
      MakeWebFeedMetadata(info.status, info.web_feed_info);
  callback_result.web_feed_metadata.is_recommended =
      index_.IsRecommended(result.followed_web_feed_id);
  callback_result.request_status = result.request_status;
  callback_result.subscription_count = index_.SubscriptionCount();
  feed_stream_->GetMetricsReporter().OnFollowAttempt(followed_with_id,
                                                     callback_result);
  std::move(callback).Run(std::move(callback_result));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeed(
    const std::string& web_feed_id,
    base::OnceCallback<void(UnfollowWebFeedResult)> callback) {
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::UnfollowWebFeedStart,
                     base::Unretained(this), web_feed_id, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedStart(
    const std::string& web_feed_id,
    base::OnceCallback<void(UnfollowWebFeedResult)> callback) {
  SubscriptionInfo info = model_->GetSubscriptionInfo(web_feed_id);

  EnqueueInFlightChange(/*subscribing=*/false,
                        /*page_information=*/absl::nullopt,
                        info.status != WebFeedSubscriptionStatus::kUnknown
                            ? absl::make_optional(info.web_feed_info)
                            : absl::nullopt);

  feed_stream_->GetTaskQueue().AddTask(
      std::make_unique<UnsubscribeFromWebFeedTask>(
          feed_stream_, web_feed_id,
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete,
              base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete(
    base::OnceCallback<void(UnfollowWebFeedResult)> callback,
    UnsubscribeFromWebFeedTask::Result result) {
  if (!result.unsubscribed_feed_name.empty()) {
    model_->OnUnsubscribed(result.unsubscribed_feed_name);
    feed_stream_->SetStreamStale(kWebFeedStream, true);
  }
  DequeueInflightChange();
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

void WebFeedSubscriptionCoordinator::LookupWebFeedDataAndRespond(
    const std::string& web_feed_id,
    const WebFeedPageInformation* maybe_page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  WebFeedSubscriptionStatus subscription_status =
      WebFeedSubscriptionStatus::kUnknown;
  // Override status and `web_feed_info` if there's an in-flight operation.
  std::string id = web_feed_id;
  const InFlightChange* in_flight_change =
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
  SubscriptionInfo subscription_info;
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
      LoadSubscriptionModel();
    }
  }
}

void WebFeedSubscriptionCoordinator::LoadSubscriptionModel() {
  DCHECK(!model_);
  feed_stream_->GetStore().ReadWebFeedStartupData(
      base::BindOnce(&WebFeedSubscriptionCoordinator::ModelDataLoaded,
                     base::Unretained(this)));
}

void WebFeedSubscriptionCoordinator::ModelDataLoaded(
    FeedStore::WebFeedStartupData startup_data) {
  DCHECK(loading_model_);
  DCHECK(!model_);
  loading_model_ = false;
  // TODO(crbug/1152592): Don't need recommended feed data, we could add a new
  // function on FeedStore to fetch only subscribed feed data.
  model_ = std::make_unique<WebFeedSubscriptionModel>(
      &feed_stream_->GetStore(), &index_, &recent_unsubscribed_,
      startup_data.subscribed_web_feeds);
  for (base::OnceClosure& callback : when_model_loads_) {
    std::move(callback).Run();
  }
  when_model_loads_.clear();
}

void WebFeedSubscriptionCoordinator::EnqueueInFlightChange(
    bool subscribing,
    absl::optional<WebFeedPageInformation> page_information,
    absl::optional<feedstore::WebFeedInfo> info) {
  in_flight_changes_.push_back(
      {subscribing, std::move(page_information), std::move(info)});
}

void WebFeedSubscriptionCoordinator::DequeueInflightChange() {
  // O(N), but N is very small.
  DCHECK(!in_flight_changes_.empty());
  in_flight_changes_.erase(in_flight_changes_.begin());
}

// Return the last in-flight change which matches either `id` or
// `maybe_page_info`.
const InFlightChange* WebFeedSubscriptionCoordinator::FindInflightChange(
    const std::string& web_feed_id,
    const WebFeedPageInformation* maybe_page_info) {
  const InFlightChange* result = nullptr;
  for (const InFlightChange& change : in_flight_changes_) {
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
    const InFlightChange* change =
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
  on_refresh_subscriptions_.push_back(std::move(callback));

  WithModel(base::BindOnce(
      &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsStart,
      base::Unretained(this)));
}

SubscriptionInfo WebFeedSubscriptionCoordinator::FindSubscriptionInfo(
    const WebFeedPageInformation& page_info) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(index_.FindWebFeed(page_info).web_feed_id);
}
SubscriptionInfo WebFeedSubscriptionCoordinator::FindSubscriptionInfoById(
    const std::string& web_feed_id) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(web_feed_id);
}

void WebFeedSubscriptionCoordinator::RefreshRecommendedFeeds() {
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
    RefreshRecommendedFeeds();
  }
}

void WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsStart() {
  DCHECK(model_);
  if (fetching_recommended_web_feeds_ || !IsSignedInAndWebFeedsEnabled())
    return;
  fetching_recommended_web_feeds_ = true;
  feed_stream_->GetTaskQueue().AddTask(
      std::make_unique<FetchRecommendedWebFeedsTask>(
          feed_stream_,
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsComplete,
              base::Unretained(this))));
}

void WebFeedSubscriptionCoordinator::FetchRecommendedWebFeedsComplete(
    FetchRecommendedWebFeedsTask::Result result) {
  DCHECK(model_);
  fetching_recommended_web_feeds_ = false;
  feed_stream_->GetMetricsReporter().RefreshRecommendedWebFeedsAttempted(
      result.status, result.recommended_web_feeds.size());
  if (result.status == WebFeedRefreshStatus::kSuccess)
    model_->UpdateRecommendedFeeds(std::move(result.recommended_web_feeds));
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
      std::make_unique<FetchSubscribedWebFeedsTask>(
          feed_stream_,
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsComplete,
              base::Unretained(this))));
}

void WebFeedSubscriptionCoordinator::FetchSubscribedWebFeedsComplete(
    FetchSubscribedWebFeedsTask::Result result) {
  DCHECK(model_);
  feed_stream_->GetMetricsReporter().RefreshSubscribedWebFeedsAttempted(
      fetching_subscribed_web_feeds_because_stale_, result.status,
      result.subscribed_web_feeds.size());
  fetching_subscribed_web_feeds_because_stale_ = false;
  fetching_subscribed_web_feeds_ = false;
  if (result.status == WebFeedRefreshStatus::kSuccess)
    model_->UpdateSubscribedFeeds(std::move(result.subscribed_web_feeds));

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

}  // namespace feed
