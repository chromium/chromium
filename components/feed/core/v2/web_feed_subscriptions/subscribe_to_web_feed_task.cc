// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"

#include <algorithm>

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

namespace feed {

namespace {

feedstore::UriMatcher ConvertToStorage(feedwire::webfeed::UriMatcher value) {
  feedstore::UriMatcher result;
  if (!value.domain_match().empty()) {
    result.set_allocated_domain_match(value.release_domain_match());
  }
  return result;
}

feedstore::Image ConvertToStorage(feedwire::webfeed::Image value) {
  feedstore::Image result;
  result.set_allocated_url(value.release_uri());
  return result;
}

feedstore::WebFeedInfo::State ConvertToStorage(
    feedwire::webfeed::WebFeed::State value) {
  switch (value) {
    case feedwire::webfeed::WebFeed::State::WebFeed_State_ACTIVE:
      return feedstore::WebFeedInfo::State::WebFeedInfo_State_ACTIVE;
    case feedwire::webfeed::WebFeed::State::WebFeed_State_INACTIVE:
      return feedstore::WebFeedInfo::State::WebFeedInfo_State_INACTIVE;
    default:
      return feedstore::WebFeedInfo::State::WebFeedInfo_State_STATE_UNSPECIFIED;
  }
}

feedstore::WebFeedInfo ConvertToStorage(feedwire::webfeed::WebFeed web_feed) {
  feedstore::WebFeedInfo result;
  result.set_allocated_web_feed_id(web_feed.release_name());
  result.set_allocated_title(web_feed.release_title());
  result.set_allocated_subtitle(web_feed.release_subtitle());
  result.set_allocated_detail_text(web_feed.release_detail_text());
  result.set_allocated_visit_uri(web_feed.release_visit_uri());
  result.set_allocated_rss_uri(web_feed.release_rss_uri());

  if (web_feed.has_favicon())
    *result.mutable_favicon() = ConvertToStorage(*web_feed.mutable_favicon());
  result.set_follower_count(web_feed.follower_count());
  result.set_state(ConvertToStorage(web_feed.state()));
  for (auto& matcher : web_feed.uri_matchers()) {
    *result.add_uri_matchers() = ConvertToStorage(std::move(matcher));
  }
  return result;
}

}  // namespace

SubscribeToWebFeedTask::SubscribeToWebFeedTask(
    FeedStream* stream,
    Request request,
    base::OnceCallback<void(Result)> callback)
    : stream_(stream),
      request_(std::move(request)),
      callback_(std::move(callback)) {}

SubscribeToWebFeedTask::~SubscribeToWebFeedTask() = default;

void SubscribeToWebFeedTask::Run() {
  if (!request_.web_feed_id.empty()) {
    DCHECK(request_.page_info.url.is_empty());
    WebFeedSubscriptionCoordinator::SubscriptionInfo info =
        stream_->subscriptions().FindSubscriptionInfoById(request_.web_feed_id);
    if (info.status == WebFeedSubscriptionStatus::kSubscribed) {
      subscribed_web_feed_info_ = info.web_feed_info;
      Done(WebFeedSubscriptionRequestStatus::kSuccess);
      return;
    }
    if (stream_->IsOffline()) {
      Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
      return;
    }
    feedwire::webfeed::FollowWebFeedRequest request;
    request.set_name(request_.web_feed_id);
    stream_->GetNetwork()->SendApiRequest<FollowWebFeedDiscoverApi>(
        request, base::BindOnce(&SubscribeToWebFeedTask::RequestComplete,
                                base::Unretained(this)));
  } else {
    DCHECK(request_.page_info.url.is_valid());
    WebFeedSubscriptionCoordinator::SubscriptionInfo info =
        stream_->subscriptions().FindSubscriptionInfo(request_.page_info);
    if (info.status == WebFeedSubscriptionStatus::kSubscribed) {
      subscribed_web_feed_info_ = info.web_feed_info;
      Done(WebFeedSubscriptionRequestStatus::kSuccess);
      return;
    }
    if (stream_->IsOffline()) {
      Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
      return;
    }
    feedwire::webfeed::FollowWebFeedRequest request;
    request.set_web_feed_uri(request_.page_info.url.spec());
    stream_->GetNetwork()->SendApiRequest<FollowWebFeedDiscoverApi>(
        request, base::BindOnce(&SubscribeToWebFeedTask::RequestComplete,
                                base::Unretained(this)));
  }
}

void SubscribeToWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::FollowWebFeedResponse> result) {
  if (result.response_body) {
    subscribed_web_feed_info_ =
        ConvertToStorage(*result.response_body->mutable_web_feed());
    Done(WebFeedSubscriptionRequestStatus::kSuccess);
    return;
  }
  // TODO(crbug/1152592): Check for 'too many subscriptions' error.
  Done(WebFeedSubscriptionRequestStatus::kFailedUnknownError);
}

void SubscribeToWebFeedTask::Done(WebFeedSubscriptionRequestStatus status) {
  Result result;
  result.request_status = status;
  result.web_feed_info = subscribed_web_feed_info_;
  result.followed_web_feed_id = subscribed_web_feed_info_.web_feed_id();
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

}  // namespace feed
