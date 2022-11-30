// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_subscription_model.h"

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_metadata_model.h"

namespace feed {
namespace {

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

WebFeedSubscriptionModel::WebFeedSubscriptionModel(
    FeedStore* store,
    WebFeedIndex* index,
    std::vector<feedstore::WebFeedInfo>* recent_subscriptions,
    feedstore::SubscribedWebFeeds feeds,
    WebFeedMetadataModel* metadata_model)
    : store_(store),
      index_(index),
      metadata_model_(metadata_model),
      recent_unsubscribed_(recent_subscriptions) {
  subscriptions_.assign(std::make_move_iterator(feeds.feeds().begin()),
                        std::make_move_iterator(feeds.feeds().end()));
  update_time_millis_ = feeds.update_time_millis();
}

WebFeedSubscriptionModel::~WebFeedSubscriptionModel() = default;

WebFeedSubscriptionInfo WebFeedSubscriptionModel::GetSubscriptionInfo(
    const std::string& web_feed_id) {
  WebFeedSubscriptionInfo result;
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

void WebFeedSubscriptionModel::OnSubscribed(
    const feedstore::WebFeedInfo& info) {
  Remove(info.web_feed_id(), *recent_unsubscribed_);
  Remove(info.web_feed_id(), subscriptions_);
  metadata_model_->RemovePendingOperationsForWebFeed(info.web_feed_id());

  subscriptions_.emplace_back(info);
  UpdateIndexAndStore();
}

void WebFeedSubscriptionModel::OnUnsubscribed(const std::string& web_feed_id) {
  feedstore::WebFeedInfo info = Remove(web_feed_id, subscriptions_);
  if (!info.web_feed_id().empty()) {
    metadata_model_->RemovePendingOperationsForWebFeed(info.web_feed_id());
    recent_unsubscribed_->push_back(std::move(info));
  }
  UpdateIndexAndStore();
}

void WebFeedSubscriptionModel::UpdateIndexAndStore() {
  feedstore::SubscribedWebFeeds state;
  for (const feedstore::WebFeedInfo& info : subscriptions_) {
    *state.add_feeds() = info;
  }
  state.set_update_time_millis(update_time_millis_);
  index_->Populate(state);
  store_->WriteSubscribedFeeds(std::move(state), base::DoNothing());
}

// Updates recommended web feeds in both index and store.
void WebFeedSubscriptionModel::UpdateRecommendedFeeds(
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
void WebFeedSubscriptionModel::UpdateSubscribedFeeds(
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

}  // namespace feed
