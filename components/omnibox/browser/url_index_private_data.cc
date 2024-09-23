// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_index_private_data.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/stack.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

GURL ClearUsernameAndPassword(const GURL& url) {
  GURL::Replacements r;
  r.ClearUsername();
  r.ClearPassword();
  return url.ReplaceComponents(r);
}

// Algorithm Functions ---------------------------------------------------------

// Comparison function for sorting search terms by descending length.
bool LengthGreater(const std::u16string& string_a,
                   const std::u16string& string_b) {
  return string_a.length() > string_b.length();
}

}  // namespace

// UpdateRecentVisitsFromHistoryDBTask -----------------------------------------

// HistoryDBTask used to update the recent visit data for a particular
// row from the history database.
class UpdateRecentVisitsFromHistoryDBTask : public history::HistoryDBTask {
 public:
  explicit UpdateRecentVisitsFromHistoryDBTask(
      URLIndexPrivateData* private_data,
      history::URLID url_id);
  UpdateRecentVisitsFromHistoryDBTask(
      const UpdateRecentVisitsFromHistoryDBTask&) = delete;
  UpdateRecentVisitsFromHistoryDBTask& operator=(
      const UpdateRecentVisitsFromHistoryDBTask&) = delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  ~UpdateRecentVisitsFromHistoryDBTask() override;

  // The URLIndexPrivateData that gets updated after the historyDB
  // task returns.
  raw_ptr<URLIndexPrivateData, AcrossTasksDanglingUntriaged> private_data_;
  // The ID of the URL to get visits for and then update.
  history::URLID url_id_;
  // Whether fetching the recent visits for the URL succeeded.
  bool succeeded_;
  // The awaited data that's shown to private_data_ for it to copy and
  // store.
  history::VisitVector recent_visits_;
};

UpdateRecentVisitsFromHistoryDBTask::UpdateRecentVisitsFromHistoryDBTask(
    URLIndexPrivateData* private_data,
    history::URLID url_id)
    : private_data_(private_data), url_id_(url_id), succeeded_(false) {}

bool UpdateRecentVisitsFromHistoryDBTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  succeeded_ = db->GetMostRecentVisitsForURL(
      url_id_, URLIndexPrivateData::kMaxVisitsToStoreInCache, &recent_visits_);
  if (!succeeded_)
    recent_visits_.clear();
  return true;  // Always claim to be done; do not retry failures.
}

void UpdateRecentVisitsFromHistoryDBTask::DoneRunOnMainThread() {
  if (succeeded_)
    private_data_->UpdateRecentVisits(url_id_, recent_visits_);
}

UpdateRecentVisitsFromHistoryDBTask::~UpdateRecentVisitsFromHistoryDBTask() =
    default;

// URLIndexPrivateData ---------------------------------------------------------

// static
constexpr size_t URLIndexPrivateData::kMaxVisitsToStoreInCache;

URLIndexPrivateData::URLIndexPrivateData() = default;

ScoredHistoryMatches URLIndexPrivateData::HistoryItemsForTerms(
    std::u16string original_search_string,
    size_t cursor_position,
    const std::string& host_filter,
    size_t max_matches,
    bookmarks::BookmarkModel* bookmark_model,
    TemplateURLService* template_url_service,
    OmniboxTriggeredFeatureService* triggered_feature_service) {
  // This list will contain the original search string and any other string
  // transformations.
  String16Vector search_strings;
  search_strings.push_back(original_search_string);
  if ((cursor_position != std::u16string::npos) &&
      (cursor_position < original_search_string.length()) &&
      (cursor_position > 0)) {
    // The original search_string broken at cursor position. This is one type of
    // transformation.  It's possible this transformation doesn't actually
    // break any words.  There's no harm in adding the transformation in this
    // case because the searching code below prevents running duplicate
    // searches.
    std::u16string transformed_search_string(original_search_string);
    transformed_search_string.insert(cursor_position, u" ");
    search_strings.push_back(transformed_search_string);
  }

  ScoredHistoryMatches scored_items;
  // Invalidate the term cache and return if we have indexed no words (probably
  // because we've not been initialized yet).
  if (word_list_.empty()) {
    search_term_cache_.clear();
    return scored_items;
  }
  // Reset used_ flags for search_term_cache_. We use a basic mark-and-sweep
  // approach.
  ResetSearchTermCache();

  bool history_ids_were_trimmed = false;
  // A set containing the list of words extracted from each search string,
  // used to prevent running duplicate searches.
  std::set<String16Vector> seen_search_words;
  for (const std::u16string& search_string : search_strings) {
    // The search string we receive may contain escaped characters. For reducing
    // the index we need individual, lower-cased words, ignoring escapings. For
    // the final filtering we need whitespace separated substrings possibly
    // containing escaped characters.
    std::u16string lower_raw_string(base::i18n::ToLower(search_string));
    // Have to convert to UTF-8 and back, because UnescapeURLComponent doesn't
    // support unescaping UTF-8 characters and converting them to UTF-16.
    std::u16string lower_unescaped_string =
        base::UTF8ToUTF16(base::UnescapeURLComponent(
            base::UTF16ToUTF8(lower_raw_string),
            base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
                base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));

    // Extract individual 'words' (as opposed to 'terms'; see the declaration
    // comment in `GetTermsAndWordStartsOffsets()`) from the search string. When
    // the user types "colspec=ID%20Mstone Release" we get four 'words':
    // "colspec", "id", "mstone" and "release".
    String16Vector lower_words(
        String16VectorFromString16(lower_unescaped_string, nullptr));
    if (lower_words.empty())
      continue;
    // If we've already searched for this list of words, don't do it again.
    if (seen_search_words.find(lower_words) != seen_search_words.end())
      continue;
    seen_search_words.insert(lower_words);

    HistoryIDVector history_ids = HistoryIDsFromWords(lower_words);
    history_ids_were_trimmed |= TrimHistoryIdsPool(&history_ids);

    HistoryIdsToScoredMatches(std::move(history_ids), lower_raw_string,
                              host_filter, template_url_service, bookmark_model,
                              &scored_items, triggered_feature_service);
  }
  // Select and sort only the top |max_matches| results.
  if (scored_items.size() > max_matches) {
    // Sort the top |max_matches| * 2 matches which is cheaper than sorting all
    // matches yet likely sufficient to contain |max_matches| unique matches
    // most of the time.
    auto first_pass_size = std::min(scored_items.size(), max_matches * 2);
    std::partial_sort(
        scored_items.begin(), scored_items.begin() + first_pass_size,
        scored_items.end(), ScoredHistoryMatch::MatchScoreGreater);

    // When ML scoring w/increased candidates is enabled, all candidates outside
    // of some light filtering should be passed to the controller to be
    // re-scored. Do not discard matches by resizing. These will have a zero
    // relevance score, so it's ok to not sort anything past `first_pass_size`.
    bool skip_resize =
        OmniboxFieldTrial::IsMlUrlScoringUnlimitedNumCandidatesEnabled();
    if (!skip_resize) {
      scored_items.resize(first_pass_size);
    }

    // Filter unique matches to maximize the use of the `max_matches` capacity.
    // It's possible this'll still end up with duplicates as having unique
    // URL IDs does not guarantee having unique `stripped_destination_url`.
    std::set<HistoryID> seen_history_ids;
    std::erase_if(scored_items, [&](const auto& scored_item) {
      HistoryID scored_item_id = scored_item.url_info.id();
      bool duplicate = seen_history_ids.count(scored_item_id);
      seen_history_ids.insert(scored_item_id);
      return duplicate;
    });
    if (!skip_resize && scored_items.size() > max_matches) {
      scored_items.resize(max_matches);
    }

  } else {
    std::sort(scored_items.begin(), scored_items.end(),
              ScoredHistoryMatch::MatchScoreGreater);
  }
  if (history_ids_were_trimmed) {
    // If we trim the results set we do not want to cache the results for next
    // time as the user's ultimately desired result could easily be eliminated
    // in the early rough filter.
    search_term_cache_.clear();
  } else {
    // Remove any stale SearchTermCacheItems.
    std::erase_if(
        search_term_cache_,
        [](const std::pair<std::u16string, SearchTermCacheItem>& item) {
          return !item.second.used_;
        });
  }

  return scored_items;
}

const std::vector<std::string>& URLIndexPrivateData::HighlyVisitedHosts()
    const {
  return highly_visited_hosts_;
}

bool URLIndexPrivateData::UpdateURL(
    history::HistoryService* history_service,
    const history::URLRow& row,
    const std::set<std::string>& scheme_allowlist,
    base::CancelableTaskTracker* tracker) {
  // The row may or may not already be in our index. If it is not already
  // indexed and it qualifies then it gets indexed. If it is already
  // indexed and still qualifies then it gets updated, otherwise it
  // is deleted from the index.
  bool row_was_updated = false;
  history::URLID row_id = row.id();
  auto row_pos = history_info_map_.find(row_id);
  if (row_pos == history_info_map_.end()) {
    // This new row should be indexed if it qualifies.
    history::URLRow new_row(row);
    new_row.set_id(row_id);
    row_was_updated =
        RowQualifiesAsSignificant(new_row, base::Time()) &&
        IndexRow(nullptr, history_service, new_row, scheme_allowlist, tracker);
  } else if (RowQualifiesAsSignificant(row, base::Time())) {
    // TODO(manukh): If we decide to launch `kDomainSuggestions`, `host_visits_`
    //   should be incremented here.
    // This indexed row still qualifies and will be re-indexed.
    // The url won't have changed but the title, visit count, etc.
    // might have changed.
    history::URLRow& row_to_update = row_pos->second.url_row;
    bool title_updated = row_to_update.title() != row.title();
    if (row_to_update.visit_count() != row.visit_count() ||
        row_to_update.typed_count() != row.typed_count() ||
        row_to_update.last_visit() != row.last_visit() || title_updated) {
      row_to_update.set_visit_count(row.visit_count());
      row_to_update.set_typed_count(row.typed_count());
      row_to_update.set_last_visit(row.last_visit());
      // If something appears to have changed, update the recent visits
      // information.
      ScheduleUpdateRecentVisits(history_service, row_id, tracker);
      // While the URL is guaranteed to remain stable, the title may have
      // changed. If so, then update the index with the changed words.
      if (title_updated) {
        // Clear all words associated with this row and re-index both the
        // URL and title.
        RemoveRowWordsFromIndex(row_to_update);
        row_to_update.set_title(row.title());
        RowWordStarts word_starts;
        AddRowWordsToIndex(row_to_update, &word_starts);
        word_starts_map_[row_id] = std::move(word_starts);
      }
      row_was_updated = true;
    }
  } else {
    // This indexed row no longer qualifies and will be de-indexed by clearing
    // all words associated with this row.
    // TODO(manukh): If we decide to launch `kDomainSuggestions`, `host_visits_`
    //  should be decremented here, and if it falls below the threshold, the URL
    //  removed from `highly_visited_hosts_`.
    RemoveRowFromIndex(row);
    row_was_updated = true;
  }
  if (row_was_updated)
    search_term_cache_.clear();  // This invalidates the cache.
  return row_was_updated;
}

void URLIndexPrivateData::UpdateRecentVisits(
    history::URLID url_id,
    const history::VisitVector& recent_visits) {
  auto row_pos = history_info_map_.find(url_id);
  if (row_pos != history_info_map_.end()) {
    VisitInfoVector* visits = &row_pos->second.visits;
    visits->clear();
    const size_t size =
        std::min(recent_visits.size(), kMaxVisitsToStoreInCache);
    visits->reserve(size);
    for (size_t i = 0; i < size; i++) {
      // Copy from the history::VisitVector the only fields visits needs.
      visits->push_back(std::make_pair(recent_visits[i].visit_time,
                                       recent_visits[i].transition));
    }
  }
  // Else: Oddly, the URL doesn't seem to exist in the private index.
  // Ignore this update.  This can happen if, for instance, the user
  // removes the URL from URLIndexPrivateData before the historyDB call
  // returns.
}

void URLIndexPrivateData::ScheduleUpdateRecentVisits(
    history::HistoryService* history_service,
    history::URLID url_id,
    base::CancelableTaskTracker* tracker) {
  history_service->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new UpdateRecentVisitsFromHistoryDBTask(this, url_id)),
      tracker);
}

bool URLIndexPrivateData::DeleteURL(const GURL& url) {
  // Find the matching entry in the history_info_map_.
  auto pos = base::ranges::find(
      history_info_map_, url,
      [](const std::pair<const HistoryID, HistoryInfoMapValue>& item) {
        return item.second.url_row.url();
      });
  if (pos == history_info_map_.end())
    return false;
  RemoveRowFromIndex(pos->second.url_row);
  search_term_cache_.clear();  // This invalidates the cache.
  return true;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RebuildFromHistory(
    history::HistoryDatabase* history_db,
    const std::set<std::string>& scheme_allowlist) {
  if (!history_db)
    return nullptr;

  history::URLDatabase::URLEnumerator history_enum;
  if (!history_db->InitURLEnumeratorForSignificant(&history_enum))
    return nullptr;

  scoped_refptr<URLIndexPrivateData> rebuilt_data(new URLIndexPrivateData);

  // Limiting the number of URLs indexed degrades the quality of suggestions to
  // save memory. This limit is only applied for urls indexed at startup and
  // more urls can be indexed during the browsing session. The primary use case
  // is for Android devices where the session is typically short, and low-memory
  // machines in general (Desktop or Mobile).
  const int max_urls_indexed =
      OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup();
  int num_urls_indexed = 0;
  for (history::URLRow row; history_enum.GetNextURL(&row);) {
    CHECK(row.url().is_valid());
    // Do not use >= to account for case of -1 for unlimited urls.
    if (rebuilt_data->IndexRow(history_db, nullptr, row, scheme_allowlist,
                               nullptr) &&
        num_urls_indexed++ == max_urls_indexed) {
      break;
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryURLHistoryItems",
                          rebuilt_data->history_id_word_map_.size());
  // TODO(manukh): Add histograms if we decide to experiment with
  //  `kDomainSuggestions`.

  return rebuilt_data;
}

scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::Duplicate() const {
  scoped_refptr<URLIndexPrivateData> data_copy = new URLIndexPrivateData;
  data_copy->word_list_ = word_list_;
  data_copy->available_words_ = available_words_;
  data_copy->word_map_ = word_map_;
  data_copy->char_word_map_ = char_word_map_;
  data_copy->word_id_history_map_ = word_id_history_map_;
  data_copy->history_id_word_map_ = history_id_word_map_;
  data_copy->history_info_map_ = history_info_map_;
  data_copy->word_starts_map_ = word_starts_map_;
  return data_copy;
  // Not copied:
  //    search_term_cache_
}

bool URLIndexPrivateData::Empty() const {
  return history_info_map_.empty();
}

void URLIndexPrivateData::Clear() {
  word_list_.clear();
  available_words_ = base::stack<WordID>();
  word_map_.clear();
  char_word_map_.clear();
  word_id_history_map_.clear();
  history_id_word_map_.clear();
  history_info_map_.clear();
  word_starts_map_.clear();
}

size_t URLIndexPrivateData::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(search_term_cache_);
  res += base::trace_event::EstimateMemoryUsage(word_list_);
  res += base::trace_event::EstimateMemoryUsage(available_words_);
  res += base::trace_event::EstimateMemoryUsage(word_map_);
  res += base::trace_event::EstimateMemoryUsage(char_word_map_);
  res += base::trace_event::EstimateMemoryUsage(word_id_history_map_);
  res += base::trace_event::EstimateMemoryUsage(history_id_word_map_);
  res += base::trace_event::EstimateMemoryUsage(history_info_map_);
  res += base::trace_event::EstimateMemoryUsage(word_starts_map_);
  res += base::trace_event::EstimateMemoryUsage(host_visits_);
  res += base::trace_event::EstimateMemoryUsage(highly_visited_hosts_);

  return res;
}

// Note that when running Chrome normally this destructor isn't called during
// shutdown because these objects are intentionally leaked. See
// InMemoryURLIndex::Shutdown for details.
URLIndexPrivateData::~URLIndexPrivateData() = default;

HistoryIDVector URLIndexPrivateData::HistoryIDsFromWords(
    const String16Vector& unsorted_words) {
  // Break the terms down into individual terms (words), get the candidate
  // set for each term, and intersect each to get a final candidate list.
  // Note that a single 'term' from the user's perspective might be
  // a string like "http://www.somewebsite.com" which, from our perspective,
  // is four words: 'http', 'www', 'somewebsite', and 'com'.
  HistoryIDVector history_ids;
  String16Vector words(unsorted_words);
  // Sort the words into the longest first as such are likely to narrow down
  // the results quicker. Also, single character words are the most expensive
  // to process so save them for last.
  std::sort(words.begin(), words.end(), LengthGreater);

  // TODO(dyaroshev): write a generic algorithm(crbug.com/696167).
  for (auto iter = words.begin(); iter != words.end(); ++iter) {
    HistoryIDSet term_history_set = HistoryIDsForTerm(*iter);
    if (term_history_set.empty())
      return HistoryIDVector();

    if (iter == words.begin()) {
      history_ids = {term_history_set.begin(), term_history_set.end()};
    } else {
      // set-intersection
      std::erase_if(history_ids, base::IsNotIn<HistoryIDSet>(term_history_set));
    }
  }
  return history_ids;
}

bool URLIndexPrivateData::TrimHistoryIdsPool(
    HistoryIDVector* history_ids) const {
  constexpr size_t kItemsToScoreLimit = 500;
  if (history_ids->size() <= kItemsToScoreLimit)
    return false;

  // Trim down the set by sorting by typed-count, visit-count, and last
  // visit.
  auto new_end = history_ids->begin() + kItemsToScoreLimit;
  HistoryItemFactorGreater item_factor_functor(history_info_map_);

  std::nth_element(history_ids->begin(), new_end, history_ids->end(),
                   item_factor_functor);
  history_ids->erase(new_end, history_ids->end());

  return true;
}

HistoryIDSet URLIndexPrivateData::HistoryIDsForTerm(
    const std::u16string& term) {
  if (term.empty())
    return HistoryIDSet();

  // TODO(mrossetti): Consider optimizing for very common terms such as
  //  'http[s]', 'www', 'com', etc. Or collect the top 100 more frequently
  //  occurring words in the user's searches.

  size_t term_length = term.length();
  WordIDSet word_id_set;
  if (term_length > 1) {
    // See if this term or a prefix thereof is present in the cache.
    std::u16string term_lower = base::i18n::ToLower(term);
    auto best_prefix(search_term_cache_.end());
    for (auto cache_iter = search_term_cache_.begin();
         cache_iter != search_term_cache_.end(); ++cache_iter) {
      if (base::StartsWith(term_lower, base::i18n::ToLower(cache_iter->first),
                           base::CompareCase::SENSITIVE) &&
          (best_prefix == search_term_cache_.end() ||
           cache_iter->first.length() > best_prefix->first.length()))
        best_prefix = cache_iter;
    }

    // If a prefix was found then determine the leftover characters to be used
    // for further refining the results from that prefix.
    Char16Set prefix_chars;
    std::u16string leftovers(term);
    if (best_prefix != search_term_cache_.end()) {
      // If the prefix is an exact match for the term then grab the cached
      // results and we're done.
      size_t prefix_length = best_prefix->first.length();
      if (prefix_length == term_length) {
        best_prefix->second.used_ = true;
        return best_prefix->second.history_id_set_;
      }

      // Otherwise we have a handy starting point.
      // If there are no history results for this prefix then we can bail early
      // as there will be no history results for the full term.
      if (best_prefix->second.history_id_set_.empty()) {
        search_term_cache_[term] = SearchTermCacheItem();
        return HistoryIDSet();
      }
      word_id_set = best_prefix->second.word_id_set_;
      prefix_chars = Char16SetFromString16(best_prefix->first);
      leftovers = term.substr(prefix_length);
    }

    // Filter for each remaining, unique character in the term.
    Char16Set leftover_chars = Char16SetFromString16(leftovers);
    Char16Set unique_chars =
        base::STLSetDifference<Char16Set>(leftover_chars, prefix_chars);

    // Reduce the word set with any leftover, unprocessed characters.
    if (!unique_chars.empty()) {
      WordIDSet leftover_set(WordIDSetForTermChars(unique_chars));
      // We might come up empty on the leftovers.
      if (leftover_set.empty()) {
        search_term_cache_[term] = SearchTermCacheItem();
        return HistoryIDSet();
      }
      // Or there may not have been a prefix from which to start.
      if (prefix_chars.empty()) {
        word_id_set = std::move(leftover_set);
      } else {
        // set-intersection
        base::EraseIf(word_id_set, base::IsNotIn<WordIDSet>(leftover_set));
      }
    }

    // We must filter the word list because the resulting word set surely
    // contains words which do not have the search term as a proper subset.
    base::EraseIf(word_id_set, [this, &term](WordID word_id) {
      return word_list_[word_id].find(term) == std::u16string::npos;
    });
  } else {
    word_id_set = WordIDSetForTermChars(Char16SetFromString16(term));
  }

  // If any words resulted then we can compose a set of history IDs by unioning
  // the sets from each word.
  // We use |buffer| because it's more efficient to collect everything and then
  // construct a flat_set than to insert elements one by one.
  HistoryIDVector buffer;
  for (WordID word_id : word_id_set) {
    auto word_iter = word_id_history_map_.find(word_id);
    if (word_iter != word_id_history_map_.end()) {
      HistoryIDSet& word_history_id_set(word_iter->second);
      buffer.insert(buffer.end(), word_history_id_set.begin(),
                    word_history_id_set.end());
    }
  }
  HistoryIDSet history_id_set(buffer.begin(), buffer.end());

  // Record a new cache entry for this word if the term is longer than
  // a single character.
  if (term_length > 1)
    search_term_cache_[term] = SearchTermCacheItem(word_id_set, history_id_set);

  return history_id_set;
}

WordIDSet URLIndexPrivateData::WordIDSetForTermChars(
    const Char16Set& term_chars) {
  // TODO(dyaroshev): write a generic algorithm(crbug.com/696167).

  WordIDSet word_id_set;
  for (auto c_iter = term_chars.begin(); c_iter != term_chars.end(); ++c_iter) {
    auto char_iter = char_word_map_.find(*c_iter);
    // A character was not found so there are no matching results: bail.
    if (char_iter == char_word_map_.end())
      return WordIDSet();

    const WordIDSet& char_word_id_set(char_iter->second);
    // It is possible for there to no longer be any words associated with
    // a particular character. Give up in that case.
    if (char_word_id_set.empty())
      return WordIDSet();

    if (c_iter == term_chars.begin()) {
      word_id_set = char_word_id_set;
    } else {
      // set-intersection
      base::EraseIf(word_id_set, base::IsNotIn<WordIDSet>(char_word_id_set));
    }
  }
  return word_id_set;
}

void URLIndexPrivateData::HistoryIdsToScoredMatches(
    HistoryIDVector history_ids,
    const std::u16string& lower_raw_string,
    const std::string& host_filter,
    const TemplateURLService* template_url_service,
    bookmarks::BookmarkModel* bookmark_model,
    ScoredHistoryMatches* scored_items,
    OmniboxTriggeredFeatureService* triggered_feature_service) const {
  if (history_ids.empty())
    return;

  auto [lower_raw_terms, lower_terms_to_word_starts_offsets] =
      GetTermsAndWordStartsOffsets(lower_raw_string);

  // Don't score matches when there are no terms to score against.  (It's
  // possible that the word break iterater that extracts words to search for in
  // the database allows some whitespace "words" whereas SplitString excludes a
  // long list of whitespace.)  One could write a scoring function that gives a
  // reasonable order to matches when there are no terms (i.e., all the words
  // are some form of whitespace), but this is such a rare edge case that it's
  // not worth the time.
  if (lower_raw_terms.empty()) {
    return;
  }

  // Filter bad matches and other matches we don't want to display.
  std::erase_if(history_ids, [&](const HistoryID history_id) {
    return ShouldExclude(history_id, host_filter, template_url_service);
  });

  // Score the matches.
  const base::Time now = base::Time::Now();

  // `ScoredHistoryMatch` will score suggestions higher when there are fewer
  // matches. However, since HQP doesn't dedupe suggestions, this can be
  // problematic when there are multiple duplicate matches. Try counting the
  // unique hosts in the matches instead.
  size_t num_unique_hosts;
  std::set<std::string> unique_hosts = {};
  for (const auto& history_id : history_ids) {
    DCHECK(history_info_map_.count(history_id));
    unique_hosts.insert(
        history_info_map_.find(history_id)->second.url_row.url().host());
    // `ScoredHistoryMatch` assigns the same specificity to suggestions for
    // counts 4 or larger.
    // TODO(manukh) Should share `kMaxUniqueHosts` with `ScoredHistoryMatch`,
    //  but doing so is complicated as it's derived from parsing the default
    //  string value for the finch param `kHQPNumMatchesScoresRule`.
    constexpr size_t kMaxUniqueHosts = 4;
    if (unique_hosts.size() >= kMaxUniqueHosts)
      break;
  }
  num_unique_hosts = unique_hosts.size();

  for (HistoryID history_id : history_ids) {
    auto hist_pos = history_info_map_.find(history_id);
    const history::URLRow& hist_item = hist_pos->second.url_row;
    auto starts_pos = word_starts_map_.find(history_id);
    CHECK(starts_pos != word_starts_map_.end(), base::NotFatalUntil::M130);

    bool is_highly_visited_host =
        !host_filter.empty() ||
        base::ranges::find(HighlyVisitedHosts(), hist_item.url().host()) !=
            HighlyVisitedHosts().end();
    ScoredHistoryMatch new_scored_match(
        hist_item, hist_pos->second.visits, lower_raw_string, lower_raw_terms,
        lower_terms_to_word_starts_offsets, starts_pos->second,
        bookmark_model && bookmark_model->IsBookmarked(hist_item.url()),
        num_unique_hosts, is_highly_visited_host, now);

    if (new_scored_match.raw_score_before_domain_boosting <
        new_scored_match.raw_score_after_domain_boosting) {
      triggered_feature_service->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_DOMAIN_SUGGESTIONS);
    }

    // Filter new matches that ended up scoring 0. (These are usually matches
    // which didn't match the user's raw terms.)
    if (new_scored_match.raw_score > 0)
      scored_items->push_back(std::move(new_scored_match));
  }
}

// static
void URLIndexPrivateData::CalculateWordStartsOffsets(
    const String16Vector& lower_terms,
    WordStarts* lower_terms_to_word_starts_offsets) {
  // Calculate offsets for each term.  For instance, the offset for
  // ".net" should be 1, indicating that the actual word-part of the term
  // starts at offset 1.
  lower_terms_to_word_starts_offsets->resize(lower_terms.size(), 0u);
  for (size_t i = 0; i < lower_terms.size(); ++i) {
    TailoredWordBreakIterator iter(lower_terms[i]);
    // If the iterator doesn't work, assume an offset of 0.
    if (!iter.Init())
      continue;
    // Find the first word start. If the iterator didn't find a word break,
    // set an offset of term size. For example, the offset for "://" should be
    // 3, indicating that the word-part is missing.
    while (iter.Advance() && !iter.IsWord()) {
    }

    (*lower_terms_to_word_starts_offsets)[i] = iter.prev();
  }
}

bool URLIndexPrivateData::IndexRow(
    history::HistoryDatabase* history_db,
    history::HistoryService* history_service,
    const history::URLRow& row,
    const std::set<std::string>& scheme_allowlist,
    base::CancelableTaskTracker* tracker) {
  const GURL& gurl(row.url());

  // Index only URLs with an allowlisted scheme.
  if (!URLSchemeIsAllowlisted(gurl, scheme_allowlist))
    return false;

  const history::URLID row_id = row.id();
  // Strip out username and password before saving and indexing.
  const GURL new_url = ClearUsernameAndPassword(gurl);

  HistoryID history_id = static_cast<HistoryID>(row_id);
  DCHECK_LT(history_id, std::numeric_limits<HistoryID>::max());

  // Add the row for quick lookup in the history info store.
  history::URLRow new_row(new_url, row_id);
  new_row.set_visit_count(row.visit_count());
  new_row.set_typed_count(row.typed_count());
  new_row.set_last_visit(row.last_visit());
  new_row.set_title(row.title());

  // Index the words contained in the URL and title of the row.
  RowWordStarts word_starts;
  AddRowWordsToIndex(new_row, &word_starts);
  word_starts_map_[history_id] = std::move(word_starts);

  history_info_map_[history_id].url_row = std::move(new_row);

  // Update the recent visits information or schedule the update
  // as appropriate.
  if (history_db) {
    // We'd like to check that we're on the history DB thread.
    // However, unittest code actually calls this on the UI thread.
    // So we don't do any thread checks.
    history::VisitVector recent_visits;
    if (history_db->GetMostRecentVisitsForURL(row_id, kMaxVisitsToStoreInCache,
                                              &recent_visits))
      UpdateRecentVisits(row_id, recent_visits);
  } else if (history_service) {
    DCHECK(tracker);
    ScheduleUpdateRecentVisits(history_service, row_id, tracker);
  }

  // Increment `host_visits_` for and possibly add the host to
  // `highly_visited_hosts`.
  static const bool domain_suggestions_enabled =
      base::FeatureList::IsEnabled(omnibox::kDomainSuggestions);
  if (domain_suggestions_enabled) {
    auto& host_info = host_visits_[gurl.host()];
    const bool was_highly_visited = host_info.IsHighlyVisited();
    host_info.AddUrl(row);
    // If the host was already added to `highly_visited_hosts_`, no need to
    // re-add it.
    if (!was_highly_visited && host_info.IsHighlyVisited())
      highly_visited_hosts_.push_back(gurl.host());
  }

  return true;
}

void URLIndexPrivateData::AddRowWordsToIndex(const history::URLRow& row,
                                             RowWordStarts* word_starts) {
  HistoryID history_id = static_cast<HistoryID>(row.id());
  // Split URL into individual, unique words then add in the title words.
  const GURL& gurl(row.url());
  DCHECK(gurl.is_valid());
  const std::u16string& url = bookmarks::CleanUpUrlForMatching(gurl, nullptr);
  String16Set url_words = String16SetFromString16(
      url, word_starts ? &word_starts->url_word_starts_ : nullptr);
  const std::u16string& title = bookmarks::CleanUpTitleForMatching(row.title());
  String16Set title_words = String16SetFromString16(
      title, word_starts ? &word_starts->title_word_starts_ : nullptr);
  for (const auto& word :
       base::STLSetUnion<String16Set>(url_words, title_words))
    AddWordToIndex(word, history_id);

  search_term_cache_.clear();  // Invalidate the term cache.
}

void URLIndexPrivateData::AddWordToIndex(const std::u16string& term,
                                         HistoryID history_id) {
  auto [word_pos, is_new] = word_map_.insert(std::make_pair(term, WordID()));

  // Adding a new word (i.e. a word that is not already in the word index).
  if (is_new) {
    word_pos->second = AddNewWordToWordList(term);

    // For each character in the newly added word add the word to the character
    // index.
    for (char16_t uni_char : Char16SetFromString16(term))
      char_word_map_[uni_char].insert(word_pos->second);
  }

  word_id_history_map_[word_pos->second].insert(history_id);
  history_id_word_map_[history_id].insert(word_pos->second);
}

WordID URLIndexPrivateData::AddNewWordToWordList(const std::u16string& term) {
  WordID word_id = word_list_.size();
  if (available_words_.empty()) {
    word_list_.push_back(term);
    return word_id;
  }

  word_id = available_words_.top();
  available_words_.pop();
  word_list_[word_id] = term;
  return word_id;
}

void URLIndexPrivateData::RemoveRowFromIndex(const history::URLRow& row) {
  RemoveRowWordsFromIndex(row);
  HistoryID history_id = static_cast<HistoryID>(row.id());
  history_info_map_.erase(history_id);
  word_starts_map_.erase(history_id);
}

void URLIndexPrivateData::RemoveRowWordsFromIndex(const history::URLRow& row) {
  // Remove the entries in history_id_word_map_ and word_id_history_map_ for
  // this row.
  HistoryID history_id = static_cast<HistoryID>(row.id());
  WordIDSet word_id_set = history_id_word_map_[history_id];
  history_id_word_map_.erase(history_id);

  // Reconcile any changes to word usage.
  for (WordID word_id : word_id_set) {
    auto word_id_history_map_iter = word_id_history_map_.find(word_id);
    CHECK(word_id_history_map_iter != word_id_history_map_.end(),
          base::NotFatalUntil::M130);

    word_id_history_map_iter->second.erase(history_id);
    if (!word_id_history_map_iter->second.empty())
      continue;

    // The word is no longer in use. Reconcile any changes to character usage.
    std::u16string word = word_list_[word_id];
    for (char16_t uni_char : Char16SetFromString16(word)) {
      auto char_word_map_iter = char_word_map_.find(uni_char);
      char_word_map_iter->second.erase(word_id);
      if (char_word_map_iter->second.empty())
        char_word_map_.erase(char_word_map_iter);
    }

    // Complete the removal of references to the word.
    word_id_history_map_.erase(word_id_history_map_iter);
    word_map_.erase(word);
    word_list_[word_id] = std::u16string();
    available_words_.push(word_id);
  }
}

void URLIndexPrivateData::ResetSearchTermCache() {
  for (auto& item : search_term_cache_)
    item.second.used_ = false;
}

// static
bool URLIndexPrivateData::URLSchemeIsAllowlisted(
    const GURL& gurl,
    const std::set<std::string>& allowlist) {
  return allowlist.find(gurl.scheme()) != allowlist.end();
}

bool URLIndexPrivateData::ShouldExclude(
    const HistoryID history_id,
    const std::string& host_filter,
    const TemplateURLService* template_url_service) const {
  auto hist_pos = history_info_map_.find(history_id);
  if (hist_pos == history_info_map_.end())
    return true;

  GURL url = hist_pos->second.url_row.url();
  if (!url.is_valid())  // Possible in case of profile corruption.
    return true;

  if (!host_filter.empty() && url.host() != host_filter)
    return true;

  // Skip results corresponding to queries from the default search engine.
  // These are low-quality, difficult-to-understand matches for users.
  // SearchProvider should surface past queries in a better way.
  const TemplateURL* default_search_engine =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  return default_search_engine &&
         default_search_engine->IsSearchURL(
             url, template_url_service->search_terms_data());
}

// SearchTermCacheItem ---------------------------------------------------------

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem(
    const WordIDSet& word_id_set,
    const HistoryIDSet& history_id_set)
    : word_id_set_(word_id_set), history_id_set_(history_id_set), used_(true) {}

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem() : used_(true) {}

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem(
    const SearchTermCacheItem& other) = default;

size_t URLIndexPrivateData::SearchTermCacheItem::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(word_id_set_) +
         base::trace_event::EstimateMemoryUsage(history_id_set_);
}

// static
std::pair<String16Vector, WordStarts>
URLIndexPrivateData::GetTermsAndWordStartsOffsets(
    const std::u16string& lower_raw_string) {
  String16Vector lower_raw_terms =
      base::SplitString(lower_raw_string, base::kWhitespaceUTF16,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lower_raw_terms.empty()) {
    return {{}, {}};
  }

  WordStarts lower_terms_to_word_starts_offsets;
  CalculateWordStartsOffsets(lower_raw_terms,
                             &lower_terms_to_word_starts_offsets);
  return {lower_raw_terms, lower_terms_to_word_starts_offsets};
}

URLIndexPrivateData::SearchTermCacheItem::~SearchTermCacheItem() = default;

// URLIndexPrivateData::HistoryItemFactorGreater -------------------------------

URLIndexPrivateData::HistoryItemFactorGreater::HistoryItemFactorGreater(
    const HistoryInfoMap& history_info_map)
    : history_info_map_(history_info_map) {}

URLIndexPrivateData::HistoryItemFactorGreater::~HistoryItemFactorGreater() =
    default;

bool URLIndexPrivateData::HistoryItemFactorGreater::operator()(
    const HistoryID h1,
    const HistoryID h2) {
  auto entry1(history_info_map_->find(h1));
  if (entry1 == history_info_map_->end()) {
    return false;
  }
  auto entry2(history_info_map_->find(h2));
  if (entry2 == history_info_map_->end()) {
    return true;
  }
  const history::URLRow& r1(entry1->second.url_row);
  const history::URLRow& r2(entry2->second.url_row);
  // First cut: typed count, visit count, recency.
  // TODO(mrossetti): This is too simplistic. Consider an approach which ranks
  // recently visited (within the last 12/24 hours) as highly important. Get
  // input from mpearson.
  if (r1.typed_count() != r2.typed_count())
    return (r1.typed_count() > r2.typed_count());
  if (r1.visit_count() != r2.visit_count())
    return (r1.visit_count() > r2.visit_count());
  return (r1.last_visit() > r2.last_visit());
}

// HostInfo --------------------------------------------------------------------

bool URLIndexPrivateData::HostInfo::IsHighlyVisited() const {
  static const int visited_urls_threshold =
      OmniboxFieldTrial::kDomainSuggestionsTypedUrlsThreshold.Get();
  static const int typed_visit_threshold =
      OmniboxFieldTrial::kDomainSuggestionsTypedVisitThreshold.Get();

  return typed_urls_ >= visited_urls_threshold &&
         typed_visits_ >= typed_visit_threshold;
}

void URLIndexPrivateData::HostInfo::AddUrl(const history::URLRow& row) {
  static const int visited_urls_offset =
      OmniboxFieldTrial::kDomainSuggestionsTypedUrlsOffset.Get();
  static const int typed_visit_offset =
      OmniboxFieldTrial::kDomainSuggestionsTypedVisitOffset.Get();
  static const int typed_visit_cap_per_visit =
      OmniboxFieldTrial::kDomainSuggestionsTypedVisitCapPerVisit.Get();

  if (row.typed_count() >= visited_urls_offset)
    typed_urls_++;

  typed_visits_ += std::clamp(row.typed_count() - typed_visit_offset, 0,
                              typed_visit_cap_per_visit);
}
