// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_

#include <iosfwd>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/proto_util.h"

class GURL;
namespace feedstore {
class UriMatcher;
}
namespace feed {

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

  // Returns the `Entry` for `url`. If more than one web feed matches,
  // this returns only one. Subscribed feeds, and more specific URL matches are
  // returned preferentially.
  Entry FindWebFeedForUrl(const GURL& url);

  Entry FindWebFeed(const std::string& id);
  bool IsRecommended(const std::string& web_feed_id) const;

  base::Time GetRecommendedFeedsUpdateTime() const {
    return recommended_feeds_update_time_;
  }
  base::Time GetSubscribedFeedsUpdateTime() const {
    return subscribed_feeds_update_time_;
  }

  std::vector<Entry> GetRecommendedEntriesForTesting() const;

 private:
  // TODO(crbug/1152592): This code is temporary, we will need to have
  // additional matching criteria. Plan to use url_matcher.h instead.
  struct EntrySet {
    EntrySet();
    ~EntrySet();
    EntrySet(const EntrySet&);
    EntrySet& operator=(const EntrySet&);
    // Maps from domain -> entries_ index.
    base::flat_map<std::string, int> domains;
    std::vector<Entry> entries;
  };
  void AddMatcher(const std::string& web_feed_id,
                  const feedstore::UriMatcher& matcher);
  const Entry& FindWebFeedForDomain(base::StringPiece domain);
  const Entry& FindWebFeedForDomain(const EntrySet& entry_set,
                                    base::StringPiece domain);

  base::Time recommended_feeds_update_time_;
  base::Time subscribed_feeds_update_time_;
  EntrySet subscribed_;
  EntrySet recommended_;
  Entry empty_entry_;
};

// For tests.
std::ostream& operator<<(std::ostream& os, const WebFeedIndex::Entry& entry);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_INDEX_H_
