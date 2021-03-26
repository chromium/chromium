// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

#include <memory>

#include "base/memory/checked_ptr.h"
#include "base/optional.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_id.h"
#include "components/offline_pages/task/closure_task.h"

namespace feed {
namespace {
using InFlightChange = feed::internal::InFlightChange;
using SubscriptionInfo = WebFeedSubscriptionCoordinator::SubscriptionInfo;

WebFeedMetadata MakeWebFeedMetadata(
    WebFeedSubscriptionStatus subscribe_status,
    const feedstore::WebFeedInfo& web_feed_info) {
  WebFeedMetadata result;
  result.web_feed_id = WebFeedId::FromInfo(web_feed_info).ToString();
  result.is_active = web_feed_info.is_active();
  result.publisher_url = GURL(web_feed_info.visit_url());
  result.title = web_feed_info.title();
  result.subscription_status = subscribe_status;
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
    WebFeedId id,
    std::vector<feedstore::WebFeedInfo>& feed_info_list) {
  feedstore::WebFeedInfo result;
  for (size_t i = 0; i < feed_info_list.size(); ++i) {
    if (WebFeedId::FromInfo(feed_info_list[i]) == id) {
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
  base::Optional<WebFeedPageInformation> page_information;
  // We may or may not know about this web feed when subscribing; always known
  // when unsubscribing.
  base::Optional<feedstore::WebFeedInfo> web_feed_info;
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

  SubscriptionInfo GetSubscriptionInfo(WebFeedId id) {
    SubscriptionInfo result;
    for (const feedstore::WebFeedInfo& info : *recent_unsubscribed_) {
      if (WebFeedId::FromInfo(info) == id) {
        result.status = WebFeedSubscriptionStatus::kNotSubscribed;
        result.web_feed_info = info;
        break;
      }
    }

    for (const feedstore::WebFeedInfo& info : subscriptions_) {
      if (WebFeedId::FromInfo(info) == id) {
        result.status = WebFeedSubscriptionStatus::kSubscribed;
        result.web_feed_info = info;
        break;
      }
    }

    return result;
  }

  void OnSubscribed(const feedstore::WebFeedInfo& info) {
    auto id = WebFeedId::FromInfo(info);
    Remove(id, *recent_unsubscribed_);
    Remove(id, subscriptions_);
    subscriptions_.emplace_back(info);
    UpdateIndexAndStore();
  }

  void OnUnsubscribed(WebFeedId id) {
    feedstore::WebFeedInfo info = Remove(id, subscriptions_);
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

  const std::vector<feedstore::WebFeedInfo>& subscriptions() const {
    return subscriptions_;
  }

 private:
  // Each of these are non-null and guaranteed to remain valid for the lifetime
  // of WebFeedSubscriptionModel.
  CheckedPtr<FeedStore> store_;
  CheckedPtr<WebFeedIndex> index_;
  // Owned by WebFeedSubscriptionCoordinator so that memory of recent
  // subscriptions is retained when the model is deleted.
  CheckedPtr<std::vector<feedstore::WebFeedInfo>> recent_unsubscribed_;

  // The current known state of subscriptions.
  std::vector<feedstore::WebFeedInfo> subscriptions_;
  int64_t update_time_millis_ = 0;
};

}  // namespace internal

WebFeedSubscriptionCoordinator::WebFeedSubscriptionCoordinator(
    FeedStream* feed_stream)
    : feed_stream_(feed_stream) {}

WebFeedSubscriptionCoordinator::~WebFeedSubscriptionCoordinator() = default;

void WebFeedSubscriptionCoordinator::Populate(
    const FeedStore::WebFeedStartupData& startup_data) {
  index_.Populate(startup_data.recommended_feed_index);
  index_.Populate(startup_data.subscribed_web_feeds);
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  EnqueueInFlightChange(/*subscribing=*/true, page_info,
                        /*info=*/base::nullopt);
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart,
                     base::Unretained(this), page_info, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromUrlStart(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);
  WebFeedIndex::Entry entry = index_.FindWebFeedForUrl(page_info.url);

  SubscribeToWebFeedTask::Request request;
  request.page_info = page_info;
  feed_stream_->GetTaskQueue().AddTask(std::make_unique<SubscribeToWebFeedTask>(
      feed_stream_, std::move(request),
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                     base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::FollowWebFeed(
    const std::string& web_feed_id,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  feedstore::WebFeedInfo info;
  info.set_web_feed_id(web_feed_id);
  EnqueueInFlightChange(/*subscribing=*/true,
                        /*page_information=*/base::nullopt, info);
  WithModel(
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart,
                     base::Unretained(this), web_feed_id, std::move(callback)));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedFromIdStart(
    const std::string& web_feed_id,
    base::OnceCallback<void(FollowWebFeedResult)> callback) {
  DCHECK(model_);
  SubscriptionInfo info =
      model_->GetSubscriptionInfo(WebFeedId::FromWebFeedId(web_feed_id));
  SubscribeToWebFeedTask::Request request;
  request.web_feed_id = web_feed_id;

  feed_stream_->GetTaskQueue().AddTask(std::make_unique<SubscribeToWebFeedTask>(
      feed_stream_, std::move(request),
      base::BindOnce(&WebFeedSubscriptionCoordinator::FollowWebFeedComplete,
                     base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::FollowWebFeedComplete(
    base::OnceCallback<void(FollowWebFeedResult)> callback,
    SubscribeToWebFeedTask::Result result) {
  DCHECK(model_);
  DequeueInflightChange();
  if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
    model_->OnSubscribed(result.web_feed_info);
  }
  SubscriptionInfo info =
      model_->GetSubscriptionInfo(result.followed_web_feed_id);
  FollowWebFeedResult callback_result;
  callback_result.web_feed_metadata =
      MakeWebFeedMetadata(info.status, info.web_feed_info);
  callback_result.web_feed_metadata.is_recommended =
      index_.IsRecommended(result.followed_web_feed_id);
  callback_result.request_status = result.request_status;
  feed_stream_->GetMetricsReporter().OnFollowAttempt(callback_result);
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
  SubscriptionInfo info =
      model_->GetSubscriptionInfo(WebFeedId::FromString(web_feed_id));

  EnqueueInFlightChange(/*subscribing=*/false,
                        /*page_information=*/base::nullopt,
                        info.status != WebFeedSubscriptionStatus::kUnknown
                            ? base::make_optional(info.web_feed_info)
                            : base::nullopt);

  feed_stream_->GetTaskQueue().AddTask(
      std::make_unique<UnsubscribeFromWebFeedTask>(
          feed_stream_, WebFeedId::FromString(web_feed_id),
          base::BindOnce(
              &WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete,
              base::Unretained(this), std::move(callback))));
}

void WebFeedSubscriptionCoordinator::UnfollowWebFeedComplete(
    base::OnceCallback<void(UnfollowWebFeedResult)> callback,
    UnsubscribeFromWebFeedTask::Result result) {
  if (result.unsubscribed_feed_id) {
    model_->OnUnsubscribed(result.unsubscribed_feed_id);
  }
  DequeueInflightChange();
  UnfollowWebFeedResult callback_result;
  callback_result.request_status = result.request_status;
  feed_stream_->GetMetricsReporter().OnUnfollowAttempt(callback_result);
  std::move(callback).Run(callback_result);
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForPage(
    const WebFeedPageInformation& page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  if (!model_ && !loading_model_) {
    // No model loaded, try to answer the request without it.
    WebFeedIndex::Entry entry = index_.FindWebFeedForUrl(page_info.url);
    if (!entry.followed()) {
      LookupWebFeedDataAndRespond(entry.id, /*maybe_page_info=*/nullptr,
                                  std::move(callback));
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
  LookupWebFeedDataAndRespond(WebFeedId(), &page_info, std::move(callback));
}

void WebFeedSubscriptionCoordinator::FindWebFeedInfoForWebFeedId(
    const std::string& web_feed_id,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  if (!model_ && !loading_model_) {
    // No model loaded, try to answer the request without it.
    WebFeedIndex::Entry entry =
        index_.FindWebFeed(WebFeedId::FromWebFeedId(web_feed_id));
    if (!entry.followed()) {
      LookupWebFeedDataAndRespond(WebFeedId::FromWebFeedId(web_feed_id),
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
  LookupWebFeedDataAndRespond(WebFeedId::FromString(web_feed_id),
                              /*maybe_page_info=*/nullptr, std::move(callback));
}

void WebFeedSubscriptionCoordinator::LookupWebFeedDataAndRespond(
    const WebFeedId& web_feed_id,
    const WebFeedPageInformation* maybe_page_info,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  WebFeedSubscriptionStatus subscription_status =
      WebFeedSubscriptionStatus::kUnknown;
  // Override status and `web_feed_info` if there's an in-flight operation.
  WebFeedId id = web_feed_id;
  const InFlightChange* in_flight_change =
      FindInflightChange(web_feed_id, maybe_page_info);

  const feedstore::WebFeedInfo* web_feed_info = nullptr;

  if (in_flight_change) {
    subscription_status =
        in_flight_change->subscribing
            ? WebFeedSubscriptionStatus::kSubscribeInProgress
            : WebFeedSubscriptionStatus::kUnsubscribeInProgress;
    if (in_flight_change->web_feed_info) {
      web_feed_info = &*in_flight_change->web_feed_info;
      if (!id)
        id = WebFeedId::FromWebFeedId(web_feed_info->web_feed_id());
    }
  }

  WebFeedIndex::Entry entry;
  if (id) {
    entry = index_.FindWebFeed(id);
  } else if (maybe_page_info) {
    entry = index_.FindWebFeedForUrl(maybe_page_info->url);
    if (entry)
      id = entry.id;
  }

  // Try using `model_` if it's loaded.
  SubscriptionInfo subscription_info;
  if (!web_feed_info && model_ && id) {
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

  // Reply with just status and id if it's not a recommended Web Feed.
  if (!entry.recommended()) {
    std::move(callback).Run(
        MakeWebFeedMetadata(subscription_status, id.GetValue()));
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

  feed_stream_->GetStore()->ReadRecommendedWebFeedInfo(
      entry.id.GetValue(),
      base::BindOnce(adapt_callback, entry.id.GetValue(), subscription_status,
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
  feed_stream_->GetStore()->ReadWebFeedStartupData(
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
      feed_stream_->GetStore(), &index_, &recent_unsubscribed_,
      startup_data.subscribed_web_feeds);
  for (base::OnceClosure& callback : when_model_loads_) {
    std::move(callback).Run();
  }
  when_model_loads_.clear();
}

void WebFeedSubscriptionCoordinator::EnqueueInFlightChange(
    bool subscribing,
    base::Optional<WebFeedPageInformation> page_information,
    base::Optional<feedstore::WebFeedInfo> info) {
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
    const WebFeedId& id,
    const WebFeedPageInformation* maybe_page_info) {
  const InFlightChange* result = nullptr;
  for (const InFlightChange& change : in_flight_changes_) {
    if ((maybe_page_info && change.page_information &&
         // TODO(crbug/1152592): Decide how much we cna relax URL matching.
         change.page_information->url == maybe_page_info->url) ||
        (id && change.web_feed_info &&
         WebFeedId::FromInfo(*change.web_feed_info) == id)) {
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
    auto id = WebFeedId::FromInfo(info);
    WebFeedSubscriptionStatus status = WebFeedSubscriptionStatus::kSubscribed;
    const InFlightChange* change =
        FindInflightChange(id, /*maybe_page_info=*/nullptr);
    if (change && !change->subscribing) {
      status = WebFeedSubscriptionStatus::kUnsubscribeInProgress;
    }
    result.push_back(MakeWebFeedMetadata(status, info));
  }
  std::move(callback).Run(std::move(result));
}

SubscriptionInfo WebFeedSubscriptionCoordinator::FindSubscriptionInfo(
    const WebFeedPageInformation& page_info) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(
      index_.FindWebFeedForUrl(page_info.url).id);
}
SubscriptionInfo WebFeedSubscriptionCoordinator::FindSubscriptionInfoById(
    const WebFeedId& id) {
  DCHECK(model_);
  return model_->GetSubscriptionInfo(id);
}

}  // namespace feed
