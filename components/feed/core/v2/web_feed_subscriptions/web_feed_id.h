// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_ID_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_ID_H_

#include <ostream>
#include <string>

namespace feedstore {
class WebFeedInfo;
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

  bool is_web_feed_id() const { return valid() && has_web_feed_id_; }
  bool is_subscription_id() const { return valid() && !has_web_feed_id_; }

  std::string ToString() const;
  static WebFeedId FromString(std::string id);

  // Returns the `web_feed_id`, or `subscription_id`.
  const std::string& GetValue() const { return id_; }

 private:
  bool has_web_feed_id_ = false;
  std::string id_;
};

inline ::std::ostream& operator<<(::std::ostream& os, const WebFeedId& id) {
  return os << id.ToString();
}

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_ID_H_
