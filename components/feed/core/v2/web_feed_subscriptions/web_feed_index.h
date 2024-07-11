// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_

#include <iosfwd>

#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/url_matcher/url_matcher.h"

namespace feedstore {
class UriMatcher;
}
namespace feed {
namespace web_feed_index_internal {
class EntrySet;
}  // namespace web_feed_index_internal

// Tracks followed web feeds, and recommended web feeds.
class WebFeedIndex {
 public:
  WebFeedIndex();
  ~WebFeedIndex();

  // Index entry.
  // TODO(harringtond): Make this a class.
  struct Entry {
    // Unique ID of the web feed.
    std::string web_feed_id;
    // True if the Web Feed is recommended, false if the Web Feed is followed.
    bool is_recommended = false;
    explicit operator bool() const { return !web_feed_id.empty(); }
    bool followed() const { return !web_feed_id.empty() && !is_recommended; }
    bool recommended() const { return !web_feed_id.empty() && is_recommended; }
  };

  // Populate the subscribed feed index.
  void Populate(const feedstore::SubscribedWebFeeds& subscribed_feeds);
  // Populate the recommended feed index.
  void Populate(const feedstore::RecommendedWebFeedIndex& recommended_feeds);

  void Clear();

  // Returns the Web Feed `Entry` which matches `page_info`. If there's more
  // than one match, preferentially returns subscribed Web Feed entries.
  Entry FindWebFeed(const WebFeedPageInformation& page_info);

  Entry FindWebFeed(const std::string& id) const;
  bool IsRecommended(const std::string& web_feed_id) const;

  base::Time GetRecommendedFeedsUpdateTime() const {
    return recommended_feeds_update_time_;
  }
  base::Time GetSubscribedFeedsUpdateTime() const {
    return subscribed_feeds_update_time_;
  }
  bool HasSubscriptions() const;
  int SubscriptionCount() const;
  int RecommendedWebFeedCount() const;
  const std::vector<Entry>& GetSubscribedEntries() const;

  std::vector<Entry> GetRecommendedEntriesForTesting() const;
  std::vector<Entry> GetSubscribedEntriesForTesting() const;
  void DumpStateForDebugging(std::ostream& os);

 private:
  using EntrySet = web_feed_index_internal::EntrySet;

  void AddMatcher(const std::string& web_feed_id,
                  const feedstore::UriMatcher& matcher);

  base::Time recommended_feeds_update_time_;
  base::Time subscribed_feeds_update_time_;
  Entry empty_entry_;

  std::unique_ptr<EntrySet> recommended_;
  std::unique_ptr<EntrySet> subscribed_;
};

// For tests.
std::ostream& operator<<(std::ostream& os, const WebFeedIndex::Entry& entry);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_
