// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"

#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/substring_set_matcher/matcher_string_pattern.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feed_matcher.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"
namespace feed {
namespace {
using Entry = WebFeedIndex::Entry;

// Given a host and a list of host suffixes, efficiently finds which host
// suffixes match.
class HostSuffixMatcher {
 public:
  // A host suffix to search for.
  struct Entry {
    bool operator<(const Entry& rhs) const {
      return host_suffix < rhs.host_suffix;
    }
    bool operator<(std::string_view other_host_suffix) const {
      return host_suffix < other_host_suffix;
    }
    std::string host_suffix;
    base::MatcherStringPattern::ID condition_id;
  };

  explicit HostSuffixMatcher(std::vector<Entry> entries) {
    entries_ = std::move(entries);
    std::sort(entries_.begin(), entries_.end());
  }

  // Find all host suffixes that match `host_string`, and add the match IDs to
  // `match_set`.
  void FindMatches(const std::string& host_string,
                   std::set<base::MatcherStringPattern::ID>& match_set) {
    std::string_view host(host_string);
    if (host.empty())
      return;
    // Ignore a trailing dot for a FQDN.
    if (host[host.size() - 1] == '.')
      host = host.substr(0, host.size() - 1);

    // Given a `host_string` of 'sub.foo.com', search for 'sub.foo.com',
    // 'foo.com', and then finally 'com'.
    FindExactMatches(host, match_set);
    for (size_t i = 0; i < host.size(); ++i) {
      if (host[i] == '.')
        FindExactMatches(host.substr(i + 1), match_set);
    }
  }

 private:
  void FindExactMatches(std::string_view prefix,
                        std::set<base::MatcherStringPattern::ID>& match_set) {
    auto iter = std::lower_bound(entries_.begin(), entries_.end(), prefix);
    while (iter != entries_.end() && iter->host_suffix == prefix) {
      match_set.insert(iter->condition_id);
      ++iter;
    }
  }

  std::vector<Entry> entries_;
};

constexpr base::MatcherStringPattern::ID kFirstConditionId = 1;

// References a set of conditions that all must be true to identify a Web Feed.
// Conditions in this context are represented by unique integer IDs. Each
// MultiConditionSet represents a set of sequential contion IDs, and an entry
// index which points to the Web Feed with these associated match conditions.
// See its use in EntrySet::Search() for context.
struct MultiConditionSet {
  // The number of conditions that must be true. Because MultiConditionSet is
  // always stored as a vector<MultiConditionSet>, this alone uniquely
  // identifies the condition IDs that must be met, given the following
  // assumptions:
  // 1. The first MultiConditionSet starts with kFirstConditionId.
  // 2. Each MultiConditionSet claims sequential condition IDs.
  //
  // For example, given the list of MultiConditionSets with the condition
  // counts [2,3,1], the condition sets correspond to the condition IDs: [{1,2},
  // {3,4,5}, {6}].
  int condition_count = 0;
  // The index of the Web Feed entry which is identified by these conditions.
  int entry_index;
};

}  // namespace

namespace web_feed_index_internal {
class EntrySetBuilder;

// A set of WebFeedIndex::Entry objects.
class EntrySet {
 public:
  EntrySet() = default;
  // Returns the Web Feed entry for the Web Feed that matches `page_url` and
  // `rss_urls`. Returns an invalid Entry if none is found.
  Entry Search(const GURL& page_url, const std::vector<GURL>& rss_urls) {
    // Search each 'vertical slice' independently: page URL, page host suffix,
    // page suffix, and RSS URL. Collect set of matching IDs.

    std::set<base::MatcherStringPattern::ID> matching_ids =
        page_url_matcher_.MatchURL(page_url);
    for (const GURL& rss_url : rss_urls) {
      auto ids = rss_url_matcher_.MatchURL(rss_url);
      matching_ids.insert(ids.begin(), ids.end());
    }
    page_host_matcher_.Match(page_url.host(), &matching_ids);
    host_suffix_matcher_.FindMatches(page_url.host(), matching_ids);
    page_path_matcher_.Match(page_url.path(), &matching_ids);

    // Iterate through `condition_sets_` to find any which has all conditions
    // met. Because condition IDs used in condition_sets_ and matching_ids
    // are both sorted, this can be done in a single O(N) pass.
    // Note: If we make the assumption that `matching_ids` is very small, we can
    // do better by binary searching for each of `matching_ids`. But a linear
    // scan is easier and likely pretty fast since condition_sets_ is dense and
    // flat.

    base::MatcherStringPattern::ID mcs_first_condition_id = kFirstConditionId;
    int matches_for_mcs = 0;
    auto match_iter = matching_ids.begin();
    for (size_t i = 0;
         i < condition_sets_.size() && match_iter != matching_ids.end();) {
      MultiConditionSet& mcs = condition_sets_[i];
      if (*match_iter < mcs_first_condition_id) {
        ++match_iter;
        matches_for_mcs = 0;
        continue;
      }
      if (mcs_first_condition_id + mcs.condition_count <= *match_iter) {
        matches_for_mcs = 0;
        mcs_first_condition_id += mcs.condition_count;
        ++i;
        continue;
      }
      ++matches_for_mcs;
      ++match_iter;
      if (mcs.condition_count == matches_for_mcs) {
        return entries_[mcs.entry_index];
      }
    }
    return {};
  }

  const std::vector<Entry>& entries() const { return entries_; }
  void DumpStateForDebugging(std::ostream& os) {
    os << entries_.size() << " entries " << good_matcher_count_
       << " good matchers " << bad_matcher_count_ << " bad matchers";
  }

 private:
  friend class EntrySetBuilder;

  // Web Feeds in this set.
  std::vector<Entry> entries_;

  // Members that just hold on to memory used in matchers.
  url_matcher::URLMatcherConditionFactory condition_factory_;
  std::vector<base::MatcherStringPattern> host_match_patterns_;
  std::vector<base::MatcherStringPattern> path_match_patterns_;

  // List of `MultiConditionSet`. Each one knows how to aggregate match IDs
  // reported from matchers below into a WebFeed match.
  std::vector<MultiConditionSet> condition_sets_;

  // Each of the following matcher have the capability to match different
  // things, and report integer IDs for each match.
  url_matcher::URLMatcher page_url_matcher_;
  url_matcher::RegexSetMatcher page_host_matcher_;
  url_matcher::RegexSetMatcher page_path_matcher_;
  HostSuffixMatcher host_suffix_matcher_{{}};
  url_matcher::URLMatcher rss_url_matcher_;

  // Counts of how many matchers were successfully parsed when building this
  // `EntrySet`.
  int good_matcher_count_ = 0;
  int bad_matcher_count_ = 0;
};

class EntrySetBuilder {
 public:
  void AddSubscribed(const feedstore::WebFeedInfo& web_feed_info) {
    int index = static_cast<int>(entry_set_->entries_.size());
    entry_set_->entries_.push_back(
        {web_feed_info.web_feed_id(), /*is_recommended=*/false});
    for (const auto& matcher : web_feed_info.matchers()) {
      CreateMatcherConditions(matcher, index);
    }
  }

  void AddRecommended(const feedstore::RecommendedWebFeedIndex::Entry& entry) {
    int index = static_cast<int>(entry_set_->entries_.size());
    entry_set_->entries_.push_back(
        {entry.web_feed_id(), /*is_recommended=*/true});
    for (const auto& matcher : entry.matchers()) {
      CreateMatcherConditions(matcher, index);
    }
  }

  std::unique_ptr<EntrySet> Build() && {
    DCHECK(entry_set_) << "Build can only be called once";
    entry_set_->page_url_matcher_.AddConditionSets(page_conditions_);
    entry_set_->rss_url_matcher_.AddConditionSets(rss_conditions_);
    entry_set_->page_host_matcher_.AddPatterns(
        MakeVectorOfPointers(entry_set_->host_match_patterns_));
    entry_set_->page_path_matcher_.AddPatterns(
        MakeVectorOfPointers(entry_set_->path_match_patterns_));

    entry_set_->host_suffix_matcher_ =
        HostSuffixMatcher(std::move(host_suffix_entries_));
    return std::move(entry_set_);
  }

 private:
  void CreateMatcherConditions(
      const feedwire::webfeed::WebFeedMatcher& web_feed_matcher,
      int web_feed_entry_index) {
    if (!CreateMatcherConditionsWithStatus(web_feed_matcher,
                                           web_feed_entry_index)) {
      ++entry_set_->bad_matcher_count_;
    } else {
      ++entry_set_->good_matcher_count_;
    }
  }

  bool CreateMatcherConditionsWithStatus(
      const feedwire::webfeed::WebFeedMatcher& web_feed_matcher,
      int web_feed_entry_index) {
    // Interpret all of the criteria first. If there are any errors, or unknown
    // criteria types, abort adding the conditions by returning early.
    if (web_feed_matcher.criteria_size() == 0)
      return false;
    for (const auto& criteria : web_feed_matcher.criteria()) {
      const std::string* text = nullptr;
      const std::string* regex = nullptr;
      switch (criteria.match_case()) {
        case feedwire::webfeed::WebFeedMatcher::Criteria::MatchCase::
            kPartialMatchRegex:
          regex = &criteria.partial_match_regex();
          if (regex->empty())
            return false;
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::MatchCase::kText:
          text = &criteria.text();
          if (text->empty())
            return false;
          break;
        default:
          return false;
      }

      switch (criteria.criteria_type()) {
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX:
          if (!text)
            return false;
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_MATCH:
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_PATH_MATCH:
        case feedwire::webfeed::WebFeedMatcher::Criteria::RSS_URL_MATCH:
          if (text || regex)
            break;
          return false;
        default:
          return false;
      }
    }

    // All criteria were understood, so create a `MultiConditionSet`
    // representing them.
    std::set<url_matcher::URLMatcherCondition> page_matcher_conditions;
    MultiConditionSet mcs;
    mcs.entry_index = web_feed_entry_index;

    for (const auto& criteria : web_feed_matcher.criteria()) {
      const std::string* text = nullptr;
      const std::string* regex = nullptr;
      switch (criteria.match_case()) {
        case feedwire::webfeed::WebFeedMatcher::Criteria::MatchCase::
            kPartialMatchRegex:
          regex = &criteria.partial_match_regex();
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::MatchCase::kText:
          text = &criteria.text();
          break;
        default:
          return false;
      }

      switch (criteria.criteria_type()) {
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_MATCH:
          if (text) {
            page_matcher_conditions.insert(
                entry_set_->condition_factory_.CreateHostEqualsCondition(
                    *text));
            break;
          }
          DCHECK(regex);
          entry_set_->host_match_patterns_.emplace_back(
              std::move(*regex), next_condition_set_id++);
          ++mcs.condition_count;
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX:
          DCHECK(text);
          host_suffix_entries_.push_back({*text, next_condition_set_id++});
          ++mcs.condition_count;
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_PATH_MATCH:
          if (text) {
            page_matcher_conditions.insert(
                entry_set_->condition_factory_.CreatePathEqualsCondition(
                    *text));
            break;
          }
          DCHECK(regex);
          entry_set_->path_match_patterns_.emplace_back(
              *regex, next_condition_set_id++);
          ++mcs.condition_count;
          break;
        case feedwire::webfeed::WebFeedMatcher::Criteria::RSS_URL_MATCH: {
          url_matcher::URLMatcherCondition condition;
          if (text) {
            condition =
                entry_set_->condition_factory_.CreateURLEqualsCondition(*text);
          } else {
            DCHECK(regex);
            condition =
                entry_set_->condition_factory_.CreateURLMatchesCondition(
                    *regex);
          }
          rss_conditions_.push_back(
              base::MakeRefCounted<url_matcher::URLMatcherConditionSet>(
                  next_condition_set_id++,
                  std::set<url_matcher::URLMatcherCondition>(
                      {std::move(condition)})));
          ++mcs.condition_count;
        } break;
        default:
          DCHECK(false);
          break;
      }
    }

    if (!page_matcher_conditions.empty()) {
      page_conditions_.push_back(
          base::MakeRefCounted<url_matcher::URLMatcherConditionSet>(
              next_condition_set_id++, page_matcher_conditions));
      ++mcs.condition_count;
    }
    DCHECK_GE(mcs.condition_count, 0);
    entry_set_->condition_sets_.push_back(mcs);
    return true;
  }

  static std::vector<const base::MatcherStringPattern*> MakeVectorOfPointers(
      const std::vector<base::MatcherStringPattern>& patterns) {
    std::vector<const base::MatcherStringPattern*> result(patterns.size());
    for (size_t i = 0; i < patterns.size(); ++i) {
      result[i] = &patterns[i];
    }
    return result;
  }

  // The `EntrySet` being built.
  std::unique_ptr<EntrySet> entry_set_ = std::make_unique<EntrySet>();

  // Temporary state for building `entry_set_`.
  base::MatcherStringPattern::ID next_condition_set_id = kFirstConditionId;
  std::vector<scoped_refptr<url_matcher::URLMatcherConditionSet>>
      page_conditions_;
  std::vector<scoped_refptr<url_matcher::URLMatcherConditionSet>>
      rss_conditions_;
  std::vector<HostSuffixMatcher::Entry> host_suffix_entries_;
};

}  // namespace web_feed_index_internal

namespace {

using EntrySetBuilder = web_feed_index_internal::EntrySetBuilder;

}  // namespace

WebFeedIndex::WebFeedIndex() {
  Clear();
}

WebFeedIndex::~WebFeedIndex() = default;

void WebFeedIndex::Populate(
    const feedstore::RecommendedWebFeedIndex& recommended_feed_index) {
  int64_t update_time_millis = recommended_feed_index.update_time_millis();
  recommended_feeds_update_time_ =
      update_time_millis <= 0
          ? base::Time()
          : feedstore::FromTimestampMillis(update_time_millis);

  EntrySetBuilder builder;

  for (const feedstore::RecommendedWebFeedIndex::Entry& entry :
       recommended_feed_index.entries()) {
    builder.AddRecommended(entry);
  }

  recommended_ = std::move(builder).Build();
}

void WebFeedIndex::Populate(
    const feedstore::SubscribedWebFeeds& subscribed_feeds) {
  int64_t update_time_millis = subscribed_feeds.update_time_millis();
  subscribed_feeds_update_time_ =
      update_time_millis <= 0
          ? base::Time()
          : feedstore::FromTimestampMillis(update_time_millis);

  EntrySetBuilder builder;

  // TODO(crbug.com/40158714): Record UMA for subscribed and recommended lists.
  // Note that flat_map will keep only the first entry with a given key.
  for (const auto& info : subscribed_feeds.feeds()) {
    builder.AddSubscribed(info);
  }
  subscribed_ = std::move(builder).Build();
}

void WebFeedIndex::Clear() {
  recommended_ = std::make_unique<EntrySet>();
  subscribed_ = std::make_unique<EntrySet>();
  recommended_feeds_update_time_ = base::Time();
  subscribed_feeds_update_time_ = base::Time();
}

WebFeedIndex::Entry WebFeedIndex::FindWebFeed(
    const std::string& web_feed_id) const {
  for (const Entry& e : subscribed_->entries()) {
    if (e.web_feed_id == web_feed_id)
      return e;
  }
  for (const Entry& e : recommended_->entries()) {
    if (e.web_feed_id == web_feed_id)
      return e;
  }
  return {};
}

Entry WebFeedIndex::FindWebFeed(const WebFeedPageInformation& page_info) {
  Entry result = subscribed_->Search(page_info.url(), page_info.GetRssUrls());
  if (!result) {
    result = recommended_->Search(page_info.url(), page_info.GetRssUrls());
  }
  return result;
}

bool WebFeedIndex::IsRecommended(const std::string& web_feed_id) const {
  if (web_feed_id.empty())
    return false;
  for (const Entry& e : recommended_->entries()) {
    if (e.web_feed_id == web_feed_id)
      return true;
  }
  return false;
}

bool WebFeedIndex::HasSubscriptions() const {
  return !subscribed_->entries().empty();
}

int WebFeedIndex::SubscriptionCount() const {
  return subscribed_->entries().size();
}

int WebFeedIndex::RecommendedWebFeedCount() const {
  return recommended_->entries().size();
}

const std::vector<Entry>& WebFeedIndex::GetSubscribedEntries() const {
  return subscribed_->entries();
}

std::vector<WebFeedIndex::Entry> WebFeedIndex::GetRecommendedEntriesForTesting()
    const {
  return recommended_->entries();
}

std::vector<WebFeedIndex::Entry> WebFeedIndex::GetSubscribedEntriesForTesting()
    const {
  return subscribed_->entries();
}

void WebFeedIndex::DumpStateForDebugging(std::ostream& os) {
  os << "recommended: ";
  recommended_->DumpStateForDebugging(os);
  os << " updated " << recommended_feeds_update_time_;
  os << "\nsubscribed: ";
  subscribed_->DumpStateForDebugging(os);
  os << " updated " << subscribed_feeds_update_time_;
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
