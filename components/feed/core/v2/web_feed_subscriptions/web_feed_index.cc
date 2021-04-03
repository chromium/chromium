// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"

#include <ostream>

#include "base/strings/string_piece.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feed_matcher.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "url/gurl.h"

namespace feed {

namespace {
using Entry = WebFeedIndex::Entry;
void AddEntries(std::vector<Entry>& entries,
                std::vector<std::pair<std::string, int>>& domain_list,
                const ::google::protobuf::RepeatedPtrField<
                    feedwire::webfeed::WebFeedMatcher>& matchers,
                bool is_recommended,
                const std::string& web_feed_id) {
  int index = static_cast<int>(entries.size());
  entries.push_back({web_feed_id, is_recommended});
  for (const feedwire::webfeed::WebFeedMatcher& matcher : matchers) {
    // TODO(crbug/1152592): This code is wrong! We need to match ALL criteria
    // provided. Also, we need to support the initial set of criteria types.
    if (!web_feed_id.empty()) {
      for (const auto& criteria : matcher.criteria()) {
        if (criteria.criteria_type() == feedwire::webfeed::WebFeedMatcher::
                                            Criteria::PAGE_URL_HOST_SUFFIX &&
            !criteria.text().empty()) {
          domain_list.emplace_back(criteria.text(), index);
        }
      }
    }
  }
}

}  // namespace

WebFeedIndex::EntrySet::EntrySet() = default;
WebFeedIndex::EntrySet::~EntrySet() = default;
WebFeedIndex::EntrySet::EntrySet(const EntrySet&) = default;
WebFeedIndex::EntrySet& WebFeedIndex::EntrySet::operator=(const EntrySet&) =
    default;
WebFeedIndex::WebFeedIndex() = default;
WebFeedIndex::~WebFeedIndex() = default;

void WebFeedIndex::Populate(
    const feedstore::RecommendedWebFeedIndex& recommended_feed_index) {
  int64_t update_time_millis = recommended_feed_index.update_time_millis();
  recommended_feeds_update_time_ =
      update_time_millis <= 0
          ? base::Time()
          : feedstore::FromTimestampMillis(update_time_millis);
  recommended_ = {};
  std::vector<std::pair<std::string, int>> domain_list;

  for (const feedstore::RecommendedWebFeedIndex::Entry& entry :
       recommended_feed_index.entries()) {
    AddEntries(recommended_.entries, domain_list, entry.matchers(),
               /*is_recommended=*/true, entry.web_feed_id());
  }

  recommended_.domains =
      base::flat_map<std::string, int>(std::move(domain_list));
}

void WebFeedIndex::Populate(
    const feedstore::SubscribedWebFeeds& subscribed_feeds) {
  int64_t update_time_millis = subscribed_feeds.update_time_millis();
  subscribed_feeds_update_time_ =
      update_time_millis <= 0
          ? base::Time()
          : feedstore::FromTimestampMillis(update_time_millis);
  subscribed_ = {};
  std::vector<std::pair<std::string, int>> domain_list;
  // TODO(crbug/1152592): Record UMA for subscribed and recommended lists.
  // Note that flat_map will keep only the first entry with a given key.
  for (const auto& info : subscribed_feeds.feeds()) {
    AddEntries(subscribed_.entries, domain_list, info.matchers(),
               /*is_recommended=*/false, info.web_feed_id());
  }

  subscribed_.domains =
      base::flat_map<std::string, int>(std::move(domain_list));
}

// For host a.b.c.d.com ->
// Lookup, in this order: a.b.c.d.com, b.c.d.com, c.d.com, d.com, com.
WebFeedIndex::Entry WebFeedIndex::FindWebFeedForUrl(const GURL& url) {
  std::string host_string = url.host();
  base::StringPiece host(host_string);
  if (host.empty())
    return empty_entry_;
  // Ignore a trailing dot for a FQDN.
  if (host[host.size() - 1] == '.')
    host = host.substr(0, host.size() - 1);

  const Entry* result = &FindWebFeedForDomain(host);
  for (size_t i = 0; i < host.size() && result->web_feed_id.empty(); ++i) {
    if (host[i] == '.')
      result = &FindWebFeedForDomain(host.substr(i + 1));
  }
  return *result;
}

WebFeedIndex::Entry WebFeedIndex::FindWebFeed(const std::string& web_feed_id) {
  for (const Entry& e : subscribed_.entries) {
    if (e.web_feed_id == web_feed_id)
      return e;
  }
  for (const Entry& e : recommended_.entries) {
    if (e.web_feed_id == web_feed_id)
      return e;
  }
  return {};
}

const WebFeedIndex::Entry& WebFeedIndex::FindWebFeedForDomain(
    base::StringPiece domain) {
  const Entry& result = FindWebFeedForDomain(subscribed_, domain);
  if (!result.web_feed_id.empty())
    return result;
  return FindWebFeedForDomain(recommended_, domain);
}

const WebFeedIndex::Entry& WebFeedIndex::FindWebFeedForDomain(
    const EntrySet& entry_set,
    base::StringPiece domain) {
  auto iter = entry_set.domains.find(domain);
  return (iter != entry_set.domains.end()) ? entry_set.entries[iter->second]
                                           : empty_entry_;
}

bool WebFeedIndex::IsRecommended(const std::string& web_feed_id) const {
  if (web_feed_id.empty())
    return false;
  for (const Entry& e : recommended_.entries) {
    if (e.web_feed_id == web_feed_id)
      return true;
  }
  return false;
}

std::vector<WebFeedIndex::Entry> WebFeedIndex::GetRecommendedEntriesForTesting()
    const {
  return recommended_.entries;
}

std::ostream& operator<<(std::ostream& os, const WebFeedIndex::Entry& entry) {
  if (entry) {
    return os << "Entry{" << entry.web_feed_id << " "
              << (entry.recommended() ? "recommended" : "subscribed") << "}";
  } else {
    return os << "Entry{}";
  }
}

}  // namespace feed
