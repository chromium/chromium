// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/wire_to_store.h"

namespace feed {

feedstore::UriMatcher ConvertToStore(feedwire::webfeed::UriMatcher value) {
  feedstore::UriMatcher result;
  if (!value.domain_match().empty()) {
    result.set_allocated_domain_match(value.release_domain_match());
  }
  return result;
}

feedstore::Image ConvertToStore(feedwire::webfeed::Image value) {
  feedstore::Image result;
  result.set_allocated_url(value.release_uri());
  return result;
}

feedstore::WebFeedInfo::State ConvertToStore(
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

feedstore::WebFeedInfo ConvertToStore(feedwire::webfeed::WebFeed web_feed) {
  feedstore::WebFeedInfo result;
  result.set_allocated_web_feed_id(web_feed.release_name());
  result.set_allocated_title(web_feed.release_title());
  result.set_allocated_subtitle(web_feed.release_subtitle());
  result.set_allocated_detail_text(web_feed.release_detail_text());
  result.set_allocated_visit_uri(web_feed.release_visit_uri());
  result.set_allocated_rss_uri(web_feed.release_rss_uri());

  if (web_feed.has_favicon())
    *result.mutable_favicon() = ConvertToStore(*web_feed.mutable_favicon());
  result.set_follower_count(web_feed.follower_count());
  result.set_state(ConvertToStore(web_feed.state()));
  for (auto& matcher : web_feed.uri_matchers()) {
    *result.add_uri_matchers() = ConvertToStore(std::move(matcher));
  }
  return result;
}

}  // namespace feed
