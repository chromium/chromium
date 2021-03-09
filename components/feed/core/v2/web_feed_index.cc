// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_index.h"

#include "base/strings/string_piece.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "url/gurl.h"

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
  result.has_web_feed_id_ = true;
  result.id_ = std::move(web_feed_id);
  return result;
}
// static
WebFeedId WebFeedId::FromFollowId(std::string subscription_id) {
  WebFeedId result;
  result.has_web_feed_id_ = false;
  result.id_ = std::move(subscription_id);
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

bool WebFeedId::operator<(const WebFeedId& rhs) const {
  return std::tie(has_web_feed_id_, id_) <
         std::tie(rhs.has_web_feed_id_, rhs.id_);
}
bool WebFeedId::operator==(const WebFeedId& rhs) const {
  return std::tie(has_web_feed_id_, id_) ==
         std::tie(rhs.has_web_feed_id_, rhs.id_);
}

std::string WebFeedId::DebugString() const {
  if (id_.empty())
    return "None";
  if (has_web_feed_id_) {
    return id_;
  }
  return "followid:" + id_;
}

WebFeedIndex::WebFeedIndex() = default;
WebFeedIndex::~WebFeedIndex() = default;

void WebFeedIndex::Populate(const FeedStore::WebFeedStartupData& data) {
  // TODO(crbug/1152592): Record UMA for size of recommended and subscribed
  // lists. Sum of these two may also be useful.
  std::vector<std::pair<std::string, WebFeedId>> domain_list;
  auto add_entries =
      [&](const ::google::protobuf::RepeatedPtrField<feedstore::UriMatcher>&
              matchers,
          const WebFeedId& id) {
        for (const feedstore::UriMatcher& matcher : matchers) {
          if (!id || matcher.domain_match().empty())
            continue;
          domain_list.emplace_back(matcher.domain_match(), id);
        }
      };

  // Add entries for both subscribed and recommended feeds. Note that flat_map
  // will keep only the first entry with a given key.
  for (const auto& info : data.subscribed_web_feeds.feeds()) {
    add_entries(info.uri_matchers(), WebFeedId::FromInfo(info));
  }

  for (const feedstore::RecommendedWebFeedIndex::Entry& entry :
       data.recommended_feed_index.entries()) {
    add_entries(entry.matchers(),
                WebFeedId::FromWebFeedId(entry.web_feed_id()));
  }

  domains_ = base::flat_map<std::string, WebFeedId>(std::move(domain_list));
}

// For host a.b.c.d.com ->
// Lookup, in this order: a.b.c.d.com, b.c.d.com, c.d.com, d.com, com.
WebFeedId WebFeedIndex::FindWebFeedForUrl(const GURL& url) {
  WebFeedId result;
  std::string host_string = url.host();
  base::StringPiece host(host_string);
  if (host.empty())
    return result;
  // Ignore a trailing dot for a FQDN.
  if (host[host.size() - 1] == '.')
    host = host.substr(0, host.size() - 1);

  result = FindWebFeedForDomain(host);
  for (size_t i = 0; i < host.size() && !result; ++i) {
    if (host[i] == '.')
      result = FindWebFeedForDomain(host.substr(i + 1));
  }
  return result;
}

WebFeedId WebFeedIndex::FindWebFeedForDomain(base::StringPiece domain) {
  auto iter = domains_.find(domain);
  return (iter != domains_.end()) ? iter->second : WebFeedId();
}

}  // namespace feed
