// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_

#include <map>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/core/v2/web_feed_subscriptions/fetch_recommended_web_feeds_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/fetch_subscribed_web_feeds_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/query_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/subscription_datastore_provider.h"
#include "components/feed/core/v2/web_feed_subscriptions/unsubscribe_from_web_feed_task.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"

namespace feed {
class FeedStream;
class WebFeedSubscriptionModel;
class WebFeedMetadataModel;

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
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override;
  void FollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override;
  void UnfollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
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
  void RefreshRecommendedFeeds(
      base::OnceCallback<void(RefreshResult)> callback) override;
  void QueryWebFeed(
      const GURL& url,
      base::OnceCallback<void(QueryWebFeedResult)> callback) override;
  void QueryWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(QueryWebFeedResult)> callback) override;

  // Types / functions exposed for task implementations.

  const feedstore::WebFeedInfo* FindRecentSubscription(
      const std::string& subscription_id);

  WebFeedSubscriptionInfo FindSubscriptionInfo(
      const WebFeedPageInformation& page_info);
  WebFeedSubscriptionInfo FindSubscriptionInfoById(
      const std::string& web_feed_id);
  WebFeedSubscriptionStatus GetWebFeedSubscriptionStatus(
      const std::string& web_feed_id) const;

  WebFeedIndex& index() { return index_; }

  bool is_loading_model_for_testing() const { return loading_model_; }
  std::string DescribeStateForTesting() const;
  void RetryPendingOperationsForTesting() { RetryPendingOperations(); }
  std::vector<feedstore::PendingWebFeedOperation>
  GetPendingOperationStateForTesting();

  struct HooksForTesting {
    HooksForTesting();
    ~HooksForTesting();
    base::RepeatingClosure before_clear_all;
    base::RepeatingClosure after_clear_all;
  };
  void SetHooksForTesting(HooksForTesting* hooks) {
    hooks_for_testing_ = hooks;
  }

 private:
  base::WeakPtr<WebFeedSubscriptionCoordinator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool IsSignedInAndWebFeedsEnabled() const;
  // Queues up tasks to retry any pending subscribe / unsubscribe operations.
  void RetryPendingOperations();

  void FindWebFeedInfoForPageStart(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback);

  void FollowWebFeedInternal(
      const std::string& web_feed_id,
      WebFeedInFlightChangeStrategy strategy,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback);
  void FollowWebFeedFromUrlStart(
      const WebFeedPageInformation& page_info,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback);
  void FollowWebFeedFromIdStart(
      const std::string& web_feed_id,
      WebFeedInFlightChangeStrategy strategy,
      feedwire::webfeed::WebFeedChangeReason change_reason,
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
  void ModelDataLoaded(FeedStore::WebFeedStartupData startup_data);
  void FindWebFeedInfoNonRecommended(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback);

  void FollowWebFeedComplete(
      base::OnceCallback<void(FollowWebFeedResult)> callback,
      bool followed_with_id,
      SubscribeToWebFeedTask::Result result);

  void UnfollowWebFeedInternal(
      const std::string& web_feed_id,
      WebFeedInFlightChangeStrategy strategy,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback);
  void UnfollowWebFeedStart(
      const std::string& web_feed_id,
      WebFeedInFlightChangeStrategy strategy,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback);
  void UnfollowWebFeedComplete(
      base::OnceCallback<void(UnfollowWebFeedResult)> callback,
      UnsubscribeFromWebFeedTask::Result result);

  void QueryWebFeedComplete(
      base::OnceCallback<void(QueryWebFeedResult)> callback,
      QueryWebFeedResult result);

  void EnqueueInFlightChange(
      bool subscribing,
      WebFeedInFlightChangeStrategy strategy,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      std::optional<WebFeedPageInformation> page_information,
      std::optional<feedstore::WebFeedInfo> info);
  const WebFeedInFlightChange* FindInflightChange(
      const std::string& web_feed_id,
      const WebFeedPageInformation* maybe_page_info) const;
  WebFeedInFlightChange DequeueInflightChange();

  void FetchRecommendedWebFeedsIfStale();
  void FetchRecommendedWebFeedsStart();
  void FetchRecommendedWebFeedsComplete(
      FetchRecommendedWebFeedsTask::Result result);
  void CallRefreshRecommendedFeedsCompleteCallbacks(RefreshResult result);

  void FetchSubscribedWebFeedsIfStale(base::OnceClosure callback);
  void FetchSubscribedWebFeedsStart();
  void FetchSubscribedWebFeedsComplete(
      FetchSubscribedWebFeedsTask::Result result);
  void CallRefreshCompleteCallbacks(RefreshResult result);
  void IsWebFeedSubscriberDone(base::OnceCallback<void(bool)> callback);
  void SubscribedWebFeedCountDone(base::OnceCallback<void(int)> callback);

  void UpdatePendingOperationBeforeAttempt(
      const std::string& web_feed_id,
      WebFeedInFlightChangeStrategy strategy,
      feedstore::PendingWebFeedOperation::Kind kind,
      feedwire::webfeed::WebFeedChangeReason change_reason);

  std::vector<std::pair<std::string, WebFeedSubscriptionStatus>>
  GetAllWebFeedSubscriptionStatus() const;
  // Called any time GetWebFeedSubscriptionStatus() may change.
  void SubscriptionsChanged();

  void ReadWebFeedStartupDataTask();

  raw_ptr<Delegate> delegate_;       // Always non-null.
  raw_ptr<FeedStream> feed_stream_;  // Always non-null, it owns this.
  WebFeedIndex index_;
  SubscriptionDatastoreProvider datastore_provider_;

  // Whether `Populate()` has been called.
  bool populated_ = false;
  std::vector<base::OnceClosure> on_populated_;
  // Non-null after `Populate()`.
  std::unique_ptr<WebFeedMetadataModel> metadata_model_;
  // A model of subscriptions. In memory only while needed.
  // TODO(harringtond): Unload the model eventually.
  std::unique_ptr<WebFeedSubscriptionModel> model_;
  bool loading_model_ = false;
  OperationToken loading_token_ = OperationToken::MakeInvalid();

  // This operation is destroyed upon a ClearAll event, to cancel all
  // in flight operations. Note that we don't destroy the whole coordinator
  // on a ClearAll event to ensure that all callbacks are eventually called.
  OperationToken::Operation token_generator_;

  std::vector<WebFeedInFlightChange> in_flight_changes_;

  // Feeds which were subscribed to recently, but are no longer.
  // These are not persisted, so we only remember recent subscriptions until the
  // Chrome process goes down.
  std::vector<feedstore::WebFeedInfo> recent_unsubscribed_;
  std::vector<base::OnceClosure> when_model_loads_;
  std::vector<base::OnceCallback<void(RefreshResult)>>
      on_refresh_subscriptions_;
  std::vector<base::OnceCallback<void(RefreshResult)>>
      on_refresh_recommended_feeds_;
  bool fetching_recommended_web_feeds_ = false;
  bool fetching_subscribed_web_feeds_ = false;
  bool fetching_subscribed_web_feeds_because_stale_ = false;

  raw_ptr<HooksForTesting> hooks_for_testing_ = nullptr;

  base::WeakPtrFactory<WebFeedSubscriptionCoordinator> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTION_COORDINATOR_H_
