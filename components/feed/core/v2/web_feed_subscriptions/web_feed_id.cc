// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_id.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/feed/core/proto/v2/store.pb.h"

namespace feed {

WebFeedId::WebFeedId() = default;
WebFeedId::~WebFeedId() = default;
WebFeedId::WebFeedId(const WebFeedId&) = default;
WebFeedId::WebFeedId(WebFeedId&&) = default;
WebFeedId& WebFeedId::operator=(const WebFeedId&) = default;
WebFeedId& WebFeedId::operator=(WebFeedId&&) = default;

// static
WebFeedId WebFeedId::FromWebFeedId(std::string web_feed_id) {
  WebFeedId result;
  if (!web_feed_id.empty()) {
    result.has_web_feed_id_ = true;
    result.id_ = std::move(web_feed_id);
  }
  return result;
}
// static
WebFeedId WebFeedId::FromFollowId(std::string subscription_id) {
  WebFeedId result;
  if (!subscription_id.empty()) {
    result.has_web_feed_id_ = false;
    result.id_ = std::move(subscription_id);
  }
  return result;
}
// static
WebFeedId WebFeedId::FromInfo(const feedstore::WebFeedInfo& web_feed_info) {
  WebFeedId result;
  if (!web_feed_info.web_feed_id().empty()) {
    result = WebFeedId::FromWebFeedId(web_feed_info.web_feed_id());
  }
  if (!web_feed_info.subscription_id().empty()) {
    result = WebFeedId::FromFollowId(web_feed_info.subscription_id());
  }
  return result;
}
std::string WebFeedId::ToString() const {
  if (id_.empty())
    return "";
  if (has_web_feed_id_) {
    return base::StrCat({"wfi:", id_});
  }
  return base::StrCat({"sub:", id_});
}

// static
WebFeedId WebFeedId::FromString(std::string id) {
  if (base::StartsWith(id, "wfi:")) {
    return WebFeedId::FromWebFeedId(id.substr(4));
  }
  if (base::StartsWith(id, "sub:")) {
    return WebFeedId::FromFollowId(id.substr(4));
  }
  return WebFeedId();
}

bool WebFeedId::operator<(const WebFeedId& rhs) const {
  return std::tie(has_web_feed_id_, id_) <
         std::tie(rhs.has_web_feed_id_, rhs.id_);
}
bool WebFeedId::operator==(const WebFeedId& rhs) const {
  return std::tie(has_web_feed_id_, id_) ==
         std::tie(rhs.has_web_feed_id_, rhs.id_);
}

}  // namespace feed
