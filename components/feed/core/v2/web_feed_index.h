// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_INDEX_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_INDEX_H_

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/proto_util.h"

class GURL;
namespace feedstore {
class UriMatcher;
}
namespace feed {

// Identifies a recommended or followed web feed.
class WebFeedId {
 public:
  // Initialize to an empty / invalid state.
  WebFeedId();
  ~WebFeedId();
  WebFeedId(const WebFeedId&);
  WebFeedId(WebFeedId&&);
  WebFeedId& operator=(const WebFeedId&);
  WebFeedId& operator=(WebFeedId&&);
  // Creates a WebFeedId from a 'web_feed_id'. This is a preferred unique ID for
  // web feeds, but it isn't always available.
  static WebFeedId FromWebFeedId(std::string web_feed_id);
  // Creates a WebFeedId from a 'subscription_id'. This is a fallback for
  // identifying a followed web feed that has not yet been assigned an
  // 'web_feed_id'.
  static WebFeedId FromFollowId(std::string subscription_id);
  static WebFeedId FromInfo(const feedstore::WebFeedInfo& web_feed_info);

  explicit operator bool() const { return valid(); }
  bool valid() const { return !id_.empty(); }

  bool operator<(const WebFeedId& rhs) const;
  bool operator==(const WebFeedId& rhs) const;

  // Returns a string for debug/test printing.
  std::string DebugString() const;

 private:
  bool has_web_feed_id_ = false;
  std::string id_;
};
inline ::std::ostream& operator<<(::std::ostream& os, const WebFeedId& id) {
  return os << id.DebugString();
}

// Tracks followed web feeds, and recommended web feeds.
class WebFeedIndex {
 public:
  WebFeedIndex();
  ~WebFeedIndex();
  // Build the index, replacing any existing data.
  void Populate(const FeedStore::WebFeedStartupData& data);
  // Returns the `WebFeedId` for `url`. If more than one web feed matches, this
  // returns only one. Subscribed feeds, and more specific URL matches are
  // returned preferentially.
  WebFeedId FindWebFeedForUrl(const GURL& url);

 private:
  void AddMatcher(const std::string& web_feed_id,
                  const feedstore::UriMatcher& matcher);
  WebFeedId FindWebFeedForDomain(base::StringPiece domain);
  WebFeedId FindRecommendedWebFeed(const GURL& url);

  // Maps from domain -> WebFeedId.
  base::flat_map<std::string, WebFeedId> domains_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_INDEX_H_
