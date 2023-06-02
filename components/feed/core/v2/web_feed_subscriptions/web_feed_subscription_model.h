// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_SUBSCRIPTION_MODEL_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_SUBSCRIPTION_MODEL_H_

#include <vector>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"

namespace feed {
class WebFeedIndex;
class WebFeedMetadataModel;
class FeedStore;

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
      feedstore::SubscribedWebFeeds feeds,
      WebFeedMetadataModel* metadata_model);
  WebFeedSubscriptionModel(const WebFeedSubscriptionModel&) = delete;
  WebFeedSubscriptionModel& operator=(const WebFeedSubscriptionModel&) = delete;
  ~WebFeedSubscriptionModel();

  WebFeedSubscriptionInfo GetSubscriptionInfo(const std::string& web_feed_id);
  void OnSubscribed(const feedstore::WebFeedInfo& info);
  void OnUnsubscribed(const std::string& web_feed_id);
  void UpdateIndexAndStore();

  // Updates recommended web feeds in both index and store.
  void UpdateRecommendedFeeds(
      std::vector<feedstore::WebFeedInfo> recommended_web_feeds);

  // Updates subscribed web feeds in both index and store.
  void UpdateSubscribedFeeds(
      std::vector<feedstore::WebFeedInfo> subscribed_web_feeds);

  const std::vector<feedstore::WebFeedInfo>& subscriptions() const {
    return subscriptions_;
  }

 private:
  // Each of these are non-null and guaranteed to remain valid for the lifetime
  // of WebFeedSubscriptionModel.
  raw_ptr<FeedStore, DanglingUntriaged> store_;
  raw_ptr<WebFeedIndex> index_;
  raw_ptr<WebFeedMetadataModel, DanglingUntriaged> metadata_model_;
  // Owned by WebFeedSubscriptionCoordinator so that memory of recent
  // subscriptions is retained when the model is deleted.
  raw_ptr<std::vector<feedstore::WebFeedInfo>> recent_unsubscribed_;

  // The current known state of subscriptions.
  std::vector<feedstore::WebFeedInfo> subscriptions_;
  int64_t update_time_millis_ = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_SUBSCRIPTION_MODEL_H_
