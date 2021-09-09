// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/core/v2/web_feed_subscriptions/fetch_recommended_web_feeds_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/fetch_subscribed_web_feeds_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/unsubscribe_from_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"

namespace feed {
namespace internal {
class WebFeedSubscriptionModel;
struct InFlightChange;

}  // namespace internal
class FeedStream;

// Coordinates the state of subscription to web feeds.
class WebFeedSubscriptionCoordinator : public WebFeedSubscriptions {
 public:
  class Delegate {
   public:
    virtual void RegisterFollowingFeedFollowCountFieldTrial(
        size_t follow_count) = 0;
  };

  explicit WebFeedSubscriptionCoordinator(Delegate* delegate,
                                          FeedStream* feed_stream);
  virtual ~WebFeedSubscriptionCoordinator();
  WebFeedSubscriptionCoordinator(const WebFeedSubscriptionCoordinator&) =
      delete;
  WebFeedSubscriptionCoordinator& operator=(
      const WebFeedSubscriptionCoordinator&) = delete;

  void Populate(const FeedStore::WebFeedStartupData& startup_data);
  // Called after a FeedStream::ClearAll operation completes. Clears any state
  // owned by this.
  void ClearAllFinished();

  // WebFeedSubscriptions implementation.

  void FollowWebFeed(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override;
  void FollowWebFeed(
      const std::string& web_feed_id,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override;
  void UnfollowWebFeed(
      const std::string& web_feed_id,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback) override;
  void FindWebFeedInfoForPage(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback) override;
  void FindWebFeedInfoForWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback) override;
  void GetAllSubscriptions(
      base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback) override;
  void RefreshSubscriptions(
      base::OnceCallback<void(RefreshResult)> callback) override;
  void IsWebFeedSubscriber(base::OnceCallback<void(bool)> callback) override;
  void SubscribedWebFeedCount(base::OnceCallback<void(int)> callback) override;
  void DumpStateForDebugging(std::ostream& ss) override;
  void RefreshRecommendedFeeds() override;

  // Types / functions exposed for task implementations.

  struct SubscriptionInfo {
    WebFeedSubscriptionStatus status = WebFeedSubscriptionStatus::kUnknown;
    feedstore::WebFeedInfo web_feed_info;
  };

  const feedstore::WebFeedInfo* FindRecentSubscription(
      const std::string& subscription_id);

  SubscriptionInfo FindSubscriptionInfo(
      const WebFeedPageInformation& page_info);
  SubscriptionInfo FindSubscriptionInfoById(const std::string& web_feed_id);

  WebFeedIndex& index() { return index_; }

  bool is_loading_model_for_testing() const { return loading_model_; }

 private:
  using WebFeedSubscriptionModel = internal::WebFeedSubscriptionModel;
  using InFlightChange = internal::InFlightChange;
  base::WeakPtr<WebFeedSubscriptionCoordinator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool IsSignedInAndWebFeedsEnabled() const;

  void FindWebFeedInfoForPageStart(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback);
  void FollowWebFeedFromUrlStart(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(FollowWebFeedResult)> callback);
  void FollowWebFeedFromIdStart(
      const std::string& web_feed_id,
      base::OnceCallback<void(FollowWebFeedResult)> callback);

  void LookupWebFeedDataAndRespond(
      const std::string& web_feed_id,
      const WebFeedPageInformation* maybe_page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback);

  void LookupWebFeedDataAndRespond(
      const WebFeedIndex::Entry& entry,
      WebFeedSubscriptionStatus subscription_status,
      base::OnceCallback<void(WebFeedMetadata)> callback);

  void FindWebFeedInfoForWebFeedIdStart(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback);
  void GetAllSubscriptionsStart(
      base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback);

  // Run `closure` after the model is loaded.
  void WithModel(base::OnceClosure closure);
  void LoadSubscriptionModel();
  void ModelDataLoaded(FeedStore::WebFeedStartupData startup_data);
  void FindWebFeedInfoNonRecommended(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback);

  void FollowWebFeedComplete(
      base::OnceCallback<void(FollowWebFeedResult)> callback,
      bool followed_with_id,
      SubscribeToWebFeedTask::Result result);

  void UnfollowWebFeedStart(
      const std::string& web_feed_id,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback);
  void UnfollowWebFeedComplete(
      base::OnceCallback<void(UnfollowWebFeedResult)> callback,
      UnsubscribeFromWebFeedTask::Result result);

  void EnqueueInFlightChange(
      bool subscribing,
      absl::optional<WebFeedPageInformation> page_information,
      absl::optional<feedstore::WebFeedInfo> info);
  const InFlightChange* FindInflightChange(
      const std::string& web_feed_id,
      const WebFeedPageInformation* maybe_page_info);
  void DequeueInflightChange();

  void FetchRecommendedWebFeedsIfStale();
  void FetchRecommendedWebFeedsStart();
  void FetchRecommendedWebFeedsComplete(
      FetchRecommendedWebFeedsTask::Result result);

  void FetchSubscribedWebFeedsIfStale(base::OnceClosure callback);
  void FetchSubscribedWebFeedsStart();
  void FetchSubscribedWebFeedsComplete(
      FetchSubscribedWebFeedsTask::Result result);
  void CallRefreshCompleteCallbacks(RefreshResult);
  void IsWebFeedSubscriberDone(base::OnceCallback<void(bool)> callback);
  void SubscribedWebFeedCountDone(base::OnceCallback<void(int)> callback);

  Delegate* delegate_;       // Always non-null.
  FeedStream* feed_stream_;  // Always non-null, it owns this.
  WebFeedIndex index_;
  // Whether `Populate()` has been called.
  bool populated_ = false;
  std::vector<base::OnceClosure> on_populated_;

  // A model of subscriptions. In memory only while needed.
  // TODO(harringtond): Unload the model eventually.
  std::unique_ptr<WebFeedSubscriptionModel> model_;
  bool loading_model_ = false;

  // Queue of in-flight changes. For each of these, there exists a task in the
  // TaskQueue.
  std::vector<InFlightChange> in_flight_changes_;
  // Feeds which were subscribed to recently, but are no longer.
  // These are not persisted, so we only remember recent subscriptions until the
  // Chrome process goes down.
  std::vector<feedstore::WebFeedInfo> recent_unsubscribed_;
  std::vector<base::OnceClosure> when_model_loads_;
  std::vector<base::OnceCallback<void(RefreshResult)>>
      on_refresh_subscriptions_;
  bool fetching_recommended_web_feeds_ = false;
  bool fetching_subscribed_web_feeds_ = false;
  bool fetching_subscribed_web_feeds_because_stale_ = false;

  base::WeakPtrFactory<WebFeedSubscriptionCoordinator> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_
