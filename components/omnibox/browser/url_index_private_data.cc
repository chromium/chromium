// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_index_private_data.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/containers/stack.h"
#include "base/files/file_util.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
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
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace {

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using in_memory_url_index::InMemoryURLIndexCacheItem;

typedef in_memory_url_index::InMemoryURLIndexCacheItem_WordListItem
    WordListItem;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_WordMapItem_WordMapEntry
    WordMapEntry;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_WordMapItem WordMapItem;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_CharWordMapItem
    CharWordMapItem;
typedef in_memory_url_index::
    InMemoryURLIndexCacheItem_CharWordMapItem_CharWordMapEntry CharWordMapEntry;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_WordIDHistoryMapItem
    WordIDHistoryMapItem;
typedef in_memory_url_index::
    InMemoryURLIndexCacheItem_WordIDHistoryMapItem_WordIDHistoryMapEntry
        WordIDHistoryMapEntry;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_HistoryInfoMapItem
    HistoryInfoMapItem;
typedef in_memory_url_index::
    InMemoryURLIndexCacheItem_HistoryInfoMapItem_HistoryInfoMapEntry
        HistoryInfoMapEntry;
typedef in_memory_url_index::
    InMemoryURLIndexCacheItem_HistoryInfoMapItem_HistoryInfoMapEntry_VisitInfo
        HistoryInfoMapEntry_VisitInfo;
typedef in_memory_url_index::InMemoryURLIndexCacheItem_WordStartsMapItem
    WordStartsMapItem;
typedef in_memory_url_index::
    InMemoryURLIndexCacheItem_WordStartsMapItem_WordStartsMapEntry
        WordStartsMapEntry;

// Algorithm Functions ---------------------------------------------------------

// Comparison function for sorting search terms by descending length.
bool LengthGreater(const base::string16& string_a,
                   const base::string16& string_b) {
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

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  ~UpdateRecentVisitsFromHistoryDBTask() override;

  // The URLIndexPrivateData that gets updated after the historyDB
  // task returns.
  URLIndexPrivateData* private_data_;
  // The ID of the URL to get visits for and then update.
  history::URLID url_id_;
  // Whether fetching the recent visits for the URL succeeded.
  bool succeeded_;
  // The awaited data that's shown to private_data_ for it to copy and
  // store.
  history::VisitVector recent_visits_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecentVisitsFromHistoryDBTask);
};

UpdateRecentVisitsFromHistoryDBTask::UpdateRecentVisitsFromHistoryDBTask(
    URLIndexPrivateData* private_data,
    history::URLID url_id)
    : private_data_(private_data), url_id_(url_id), succeeded_(false) {
}

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

UpdateRecentVisitsFromHistoryDBTask::~UpdateRecentVisitsFromHistoryDBTask() {
}


// URLIndexPrivateData ---------------------------------------------------------

// static
constexpr size_t URLIndexPrivateData::kMaxVisitsToStoreInCache;

URLIndexPrivateData::URLIndexPrivateData()
    : restored_cache_version_(0),
      saved_cache_version_(kCurrentCacheFileVersion) {}

ScoredHistoryMatches URLIndexPrivateData::HistoryItemsForTerms(
    base::string16 original_search_string,
    size_t cursor_position,
    size_t max_matches,
    bookmarks::BookmarkModel* bookmark_model,
    TemplateURLService* template_url_service) {
  // This list will contain the original search string and any other string
  // transformations.
  String16Vector search_strings;
  search_strings.push_back(original_search_string);
  if ((cursor_position != base::string16::npos) &&
      (cursor_position < original_search_string.length()) &&
      (cursor_position > 0)) {
    // The original search_string broken at cursor position. This is one type of
    // transformation.  It's possible this transformation doesn't actually
    // break any words.  There's no harm in adding the transformation in this
    // case because the searching code below prevents running duplicate
    // searches.
    base::string16 transformed_search_string(original_search_string);
    transformed_search_string.insert(cursor_position, base::ASCIIToUTF16(" "));
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
  std::set<String16Vector> search_string_words;
  for (const base::string16& search_string : search_strings) {
    // The search string we receive may contain escaped characters. For reducing
    // the index we need individual, lower-cased words, ignoring escapings. For
    // the final filtering we need whitespace separated substrings possibly
    // containing escaped characters.
    base::string16 lower_raw_string(base::i18n::ToLower(search_string));
    // Have to convert to UTF-8 and back, because UnescapeURLComponent doesn't
    // support unescaping UTF-8 characters and converting them to UTF-16.
    base::string16 lower_unescaped_string =
        base::UTF8ToUTF16(net::UnescapeURLComponent(
            base::UTF16ToUTF8(lower_raw_string),
            net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
                net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));

    // Extract individual 'words' (as opposed to 'terms'; see comment in
    // HistoryIdsToScoredMatches()) from the search string. When the user types
    // "colspec=ID%20Mstone Release" we get four 'words': "colspec", "id",
    // "mstone" and "release".
    String16Vector lower_words(
        String16VectorFromString16(lower_unescaped_string, false, nullptr));
    if (lower_words.empty())
      continue;
    // If we've already searched for this list of words, don't do it again.
    if (search_string_words.find(lower_words) != search_string_words.end())
      continue;
    search_string_words.insert(lower_words);

    HistoryIDVector history_ids = HistoryIDsFromWords(lower_words);
    history_ids_were_trimmed |= TrimHistoryIdsPool(&history_ids);

    HistoryIdsToScoredMatches(std::move(history_ids), lower_raw_string,
                              template_url_service, bookmark_model,
                              &scored_items);
  }
  // Select and sort only the top |max_matches| results.
  if (scored_items.size() > max_matches) {
    std::partial_sort(scored_items.begin(), scored_items.begin() + max_matches,
                      scored_items.end(),
                      ScoredHistoryMatch::MatchScoreGreater);
    scored_items.resize(max_matches);
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
    base::EraseIf(
        search_term_cache_,
        [](const std::pair<base::string16, SearchTermCacheItem>& item) {
          return !item.second.used_;
        });
  }

  return scored_items;
}

bool URLIndexPrivateData::UpdateURL(
    history::HistoryService* history_service,
    const history::URLRow& row,
    const std::set<std::string>& scheme_whitelist,
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
    row_was_updated = RowQualifiesAsSignificant(new_row, base::Time()) &&
                      IndexRow(nullptr,
                               history_service,
                               new_row,
                               scheme_whitelist,
                               tracker);
  } else if (RowQualifiesAsSignificant(row, base::Time())) {
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
    // This indexed row no longer qualifies and will be de-indexed by
    // clearing all words associated with this row.
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

// Helper functor for DeleteURL.
class HistoryInfoMapItemHasURL {
 public:
  explicit HistoryInfoMapItemHasURL(const GURL& url): url_(url) {}

  bool operator()(const std::pair<const HistoryID, HistoryInfoMapValue>& item) {
    return item.second.url_row.url() == url_;
  }

 private:
  const GURL& url_;
};

bool URLIndexPrivateData::DeleteURL(const GURL& url) {
  // Find the matching entry in the history_info_map_.
  auto pos = std::find_if(history_info_map_.begin(), history_info_map_.end(),
                          HistoryInfoMapItemHasURL(url));
  if (pos == history_info_map_.end())
    return false;
  RemoveRowFromIndex(pos->second.url_row);
  search_term_cache_.clear();  // This invalidates the cache.
  return true;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RestoreFromFile(
    const base::FilePath& file_path) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  if (!base::PathExists(file_path))
    return nullptr;
  std::string data;

  // To reduce OOM crashes, set a common sense limit on the cache file size we
  // try to read. Most cache file sizes are under 1MB.
  constexpr size_t kHistoryProviderCacheSizeLimitBytes = 50 * 1000 * 1000;

  // If there is no cache file then simply give up. This will cause us to
  // attempt to rebuild from the history database.
  if (!base::ReadFileToStringWithMaxSize(file_path, &data,
                                         kHistoryProviderCacheSizeLimitBytes)) {
    return nullptr;
  }

  scoped_refptr<URLIndexPrivateData> restored_data(new URLIndexPrivateData);
  InMemoryURLIndexCacheItem index_cache;
  if (!index_cache.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse URLIndexPrivateData cache data read from "
                 << file_path.value();
    return restored_data;
  }

  if (!restored_data->RestorePrivateData(index_cache))
    return nullptr;

  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexRestoreCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryURLHistoryItems",
                          restored_data->history_id_word_map_.size());
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryURLCacheSize", data.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLWords",
                             restored_data->word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLChars",
                             restored_data->char_word_map_.size());
  if (restored_data->Empty())
    return nullptr;  // 'No data' is the same as a failed reload.
  return restored_data;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RebuildFromHistory(
    history::HistoryDatabase* history_db,
    const std::set<std::string>& scheme_whitelist) {
  if (!history_db)
    return nullptr;

  base::TimeTicks beginning_time = base::TimeTicks::Now();

  scoped_refptr<URLIndexPrivateData>
      rebuilt_data(new URLIndexPrivateData);
  history::URLDatabase::URLEnumerator history_enum;
  if (!history_db->InitURLEnumeratorForSignificant(&history_enum))
    return nullptr;

  rebuilt_data->last_time_rebuilt_from_history_ = base::Time::Now();

  // Limiting the number of URLs indexed degrades the quality of suggestions to
  // save memory. This limit is only applied for urls indexed at startup and
  // more urls can be indexed during the browsing session. The primary use case
  // is for Android devices where the session is typically short.
  const int max_urls_indexed =
      OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup();
  int num_urls_indexed = 0;
  for (history::URLRow row; history_enum.GetNextURL(&row);) {
    DCHECK(RowQualifiesAsSignificant(row, base::Time()));
    // Do not use >= to account for case of -1 for unlimited urls.
    if (num_urls_indexed++ == max_urls_indexed)
      break;
    rebuilt_data->IndexRow(
        history_db, nullptr, row, scheme_whitelist, nullptr);
  }

  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexingTime",
                      base::TimeTicks::Now() - beginning_time);
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryURLHistoryItems",
                          rebuilt_data->history_id_word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLWords",
                             rebuilt_data->word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLChars",
                             rebuilt_data->char_word_map_.size());
  return rebuilt_data;
}

// static
bool URLIndexPrivateData::WritePrivateDataToCacheFileTask(
    scoped_refptr<URLIndexPrivateData> private_data,
    const base::FilePath& file_path) {
  DCHECK(private_data);
  DCHECK(!file_path.empty());
  return private_data->SaveToFile(file_path);
}

scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::Duplicate() const {
  scoped_refptr<URLIndexPrivateData> data_copy = new URLIndexPrivateData;
  data_copy->last_time_rebuilt_from_history_ = last_time_rebuilt_from_history_;
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
  last_time_rebuilt_from_history_ = base::Time();
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

  return res;
}

URLIndexPrivateData::~URLIndexPrivateData() {}

HistoryIDVector URLIndexPrivateData::HistoryIDsFromWords(
    const String16Vector& unsorted_words) {
  // This histogram name reflects the historic name of this function.
  SCOPED_UMA_HISTOGRAM_TIMER("Omnibox.HistoryQuickHistoryIDSetFromWords");
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
      base::EraseIf(history_ids, base::IsNotIn<HistoryIDSet>(term_history_set));
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
    const base::string16& term) {
  if (term.empty())
    return HistoryIDSet();

  // TODO(mrossetti): Consider optimizing for very common terms such as
  // 'http[s]', 'www', 'com', etc. Or collect the top 100 more frequently
  // occuring words in the user's searches.

  size_t term_length = term.length();
  WordIDSet word_id_set;
  if (term_length > 1) {
    // See if this term or a prefix thereof is present in the cache.
    base::string16 term_lower = base::i18n::ToLower(term);
    auto best_prefix(search_term_cache_.end());
    for (auto cache_iter = search_term_cache_.begin();
         cache_iter != search_term_cache_.end(); ++cache_iter) {
      if (base::StartsWith(term_lower,
                           base::i18n::ToLower(cache_iter->first),
                           base::CompareCase::SENSITIVE) &&
          (best_prefix == search_term_cache_.end() ||
           cache_iter->first.length() > best_prefix->first.length()))
        best_prefix = cache_iter;
    }

    // If a prefix was found then determine the leftover characters to be used
    // for further refining the results from that prefix.
    Char16Set prefix_chars;
    base::string16 leftovers(term);
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
      return word_list_[word_id].find(term) == base::string16::npos;
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
    const base::string16& lower_raw_string,
    const TemplateURLService* template_url_service,
    bookmarks::BookmarkModel* bookmark_model,
    ScoredHistoryMatches* scored_items) const {
  if (history_ids.empty())
    return;

  // Break up the raw search string (complete with escaped URL elements) into
  // 'terms' (as opposed to 'words'; see comment in HistoryItemsForTerms()).
  // We only want to break up the search string on 'true' whitespace rather than
  // escaped whitespace.  For example, when the user types
  // "colspec=ID%20Mstone Release" we get two 'terms': "colspec=id%20mstone" and
  // "release".
  String16Vector lower_raw_terms =
      base::SplitString(lower_raw_string, base::kWhitespaceUTF16,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Don't score matches when there are no terms to score against.  (It's
  // possible that the word break iterater that extracts words to search for in
  // the database allows some whitespace "words" whereas SplitString excludes a
  // long list of whitespace.)  One could write a scoring function that gives a
  // reasonable order to matches when there are no terms (i.e., all the words
  // are some form of whitespace), but this is such a rare edge case that it's
  // not worth the time.
  if (lower_raw_terms.empty())
    return;

  WordStarts lower_terms_to_word_starts_offsets;
  CalculateWordStartsOffsets(lower_raw_terms,
                             &lower_terms_to_word_starts_offsets);

  // Filter bad matches and other matches we don't want to display.
  base::EraseIf(history_ids, [&](const HistoryID history_id) {
    return ShouldFilter(history_id, template_url_service);
  });

  // Score the matches.
  const size_t num_matches = history_ids.size();
  const base::Time now = base::Time::Now();

  for (HistoryID history_id : history_ids) {
    auto hist_pos = history_info_map_.find(history_id);
    const history::URLRow& hist_item = hist_pos->second.url_row;
    auto starts_pos = word_starts_map_.find(history_id);
    DCHECK(starts_pos != word_starts_map_.end());
    ScoredHistoryMatch new_scored_match(
        hist_item, hist_pos->second.visits, lower_raw_string, lower_raw_terms,
        lower_terms_to_word_starts_offsets, starts_pos->second,
        bookmark_model && bookmark_model->IsBookmarked(hist_item.url()),
        num_matches, now);
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
    TailoredWordBreakIterator iter(lower_terms[i],
                                   base::i18n::BreakIterator::BREAK_WORD);
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
    const std::set<std::string>& scheme_whitelist,
    base::CancelableTaskTracker* tracker) {
  const GURL& gurl(row.url());

  // Index only URLs with a whitelisted scheme.
  if (!URLSchemeIsWhitelisted(gurl, scheme_whitelist))
    return false;

  history::URLID row_id = row.id();
  // Strip out username and password before saving and indexing.
  base::string16 url(url_formatter::FormatUrl(
      gurl, url_formatter::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::NONE, nullptr, nullptr, nullptr));

  HistoryID history_id = static_cast<HistoryID>(row_id);
  DCHECK_LT(history_id, std::numeric_limits<HistoryID>::max());

  // Add the row for quick lookup in the history info store.
  history::URLRow new_row(GURL(url), row_id);
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
    if (history_db->GetMostRecentVisitsForURL(row_id,
                                              kMaxVisitsToStoreInCache,
                                              &recent_visits))
      UpdateRecentVisits(row_id, recent_visits);
  } else {
    DCHECK(tracker);
    DCHECK(history_service);
    ScheduleUpdateRecentVisits(history_service, row_id, tracker);
  }

  return true;
}

void URLIndexPrivateData::AddRowWordsToIndex(const history::URLRow& row,
                                             RowWordStarts* word_starts) {
  HistoryID history_id = static_cast<HistoryID>(row.id());
  // Split URL into individual, unique words then add in the title words.
  const GURL& gurl(row.url());
  const base::string16& url =
      bookmarks::CleanUpUrlForMatching(gurl, nullptr);
  String16Set url_words = String16SetFromString16(url,
      word_starts ? &word_starts->url_word_starts_ : nullptr);
  const base::string16& title = bookmarks::CleanUpTitleForMatching(row.title());
  String16Set title_words = String16SetFromString16(title,
      word_starts ? &word_starts->title_word_starts_ : nullptr);
  for (const auto& word :
       base::STLSetUnion<String16Set>(url_words, title_words))
    AddWordToIndex(word, history_id);

  search_term_cache_.clear();  // Invalidate the term cache.
}

void URLIndexPrivateData::AddWordToIndex(const base::string16& term,
                                         HistoryID history_id) {
  WordMap::iterator word_pos;
  bool is_new;
  std::tie(word_pos, is_new) = word_map_.insert(std::make_pair(term, WordID()));

  // Adding a new word (i.e. a word that is not already in the word index).
  if (is_new) {
    word_pos->second = AddNewWordToWordList(term);

    // For each character in the newly added word add the word to the character
    // index.
    for (base::char16 uni_char : Char16SetFromString16(term))
      char_word_map_[uni_char].insert(word_pos->second);
  }

  word_id_history_map_[word_pos->second].insert(history_id);
  history_id_word_map_[history_id].insert(word_pos->second);
}

WordID URLIndexPrivateData::AddNewWordToWordList(const base::string16& term) {
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
    DCHECK(word_id_history_map_iter != word_id_history_map_.end());

    word_id_history_map_iter->second.erase(history_id);
    if (!word_id_history_map_iter->second.empty())
      continue;

    // The word is no longer in use. Reconcile any changes to character usage.
    base::string16 word = word_list_[word_id];
    for (base::char16 uni_char : Char16SetFromString16(word)) {
      auto char_word_map_iter = char_word_map_.find(uni_char);
      char_word_map_iter->second.erase(word_id);
      if (char_word_map_iter->second.empty())
        char_word_map_.erase(char_word_map_iter);
    }

    // Complete the removal of references to the word.
    word_id_history_map_.erase(word_id_history_map_iter);
    word_map_.erase(word);
    word_list_[word_id] = base::string16();
    available_words_.push(word_id);
  }
}

void URLIndexPrivateData::ResetSearchTermCache() {
  for (auto& item : search_term_cache_)
    item.second.used_ = false;
}

bool URLIndexPrivateData::SaveToFile(const base::FilePath& file_path) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  InMemoryURLIndexCacheItem index_cache;
  SavePrivateData(&index_cache);
  std::string data;
  if (!index_cache.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize the InMemoryURLIndex cache.";
    return false;
  }

  int size = data.size();
  if (base::WriteFile(file_path, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << file_path.value();
    return false;
  }
  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexSaveCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  return true;
}

void URLIndexPrivateData::SavePrivateData(
    InMemoryURLIndexCacheItem* cache) const {
  DCHECK(cache);
  cache->set_last_rebuild_timestamp(
      last_time_rebuilt_from_history_.ToInternalValue());
  cache->set_version(saved_cache_version_);
  // history_item_count_ is no longer used but rather than change the protobuf
  // definition use a placeholder. This will go away with the switch to SQLite.
  cache->set_history_item_count(0);
  SaveWordList(cache);
  SaveWordMap(cache);
  SaveCharWordMap(cache);
  SaveWordIDHistoryMap(cache);
  SaveHistoryInfoMap(cache);
  SaveWordStartsMap(cache);
}

void URLIndexPrivateData::SaveWordList(InMemoryURLIndexCacheItem* cache) const {
  if (word_list_.empty())
    return;
  WordListItem* list_item = cache->mutable_word_list();
  list_item->set_word_count(word_list_.size());
  for (const base::string16& word : word_list_)
    list_item->add_word(base::UTF16ToUTF8(word));
}

void URLIndexPrivateData::SaveWordMap(InMemoryURLIndexCacheItem* cache) const {
  if (word_map_.empty())
    return;
  WordMapItem* map_item = cache->mutable_word_map();
  map_item->set_item_count(word_map_.size());
  for (const auto& elem : word_map_) {
    WordMapEntry* map_entry = map_item->add_word_map_entry();
    map_entry->set_word(base::UTF16ToUTF8(elem.first));
    map_entry->set_word_id(elem.second);
  }
}

void URLIndexPrivateData::SaveCharWordMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (char_word_map_.empty())
    return;
  CharWordMapItem* map_item = cache->mutable_char_word_map();
  map_item->set_item_count(char_word_map_.size());
  for (const auto& entry : char_word_map_) {
    CharWordMapEntry* map_entry = map_item->add_char_word_map_entry();
    map_entry->set_char_16(entry.first);
    const WordIDSet& word_id_set = entry.second;
    map_entry->set_item_count(word_id_set.size());
    for (WordID word_id : word_id_set)
      map_entry->add_word_id(word_id);
  }
}

void URLIndexPrivateData::SaveWordIDHistoryMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (word_id_history_map_.empty())
    return;
  WordIDHistoryMapItem* map_item = cache->mutable_word_id_history_map();
  map_item->set_item_count(word_id_history_map_.size());
  for (const auto& entry : word_id_history_map_) {
    WordIDHistoryMapEntry* map_entry =
        map_item->add_word_id_history_map_entry();
    map_entry->set_word_id(entry.first);
    const HistoryIDSet& history_id_set = entry.second;
    map_entry->set_item_count(history_id_set.size());
    for (HistoryID history_id : history_id_set)
      map_entry->add_history_id(history_id);
  }
}

void URLIndexPrivateData::SaveHistoryInfoMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (history_info_map_.empty())
    return;
  HistoryInfoMapItem* map_item = cache->mutable_history_info_map();
  map_item->set_item_count(history_info_map_.size());
  for (const auto& entry : history_info_map_) {
    HistoryInfoMapEntry* map_entry = map_item->add_history_info_map_entry();
    map_entry->set_history_id(entry.first);
    const history::URLRow& url_row = entry.second.url_row;
    // Note: We only save information that contributes to the index so there
    // is no need to save search_term_cache_ (not persistent).
    map_entry->set_visit_count(url_row.visit_count());
    map_entry->set_typed_count(url_row.typed_count());
    map_entry->set_last_visit(url_row.last_visit().ToInternalValue());
    map_entry->set_url(url_row.url().spec());
    map_entry->set_title(base::UTF16ToUTF8(url_row.title()));
    for (const auto& visit : entry.second.visits) {
      HistoryInfoMapEntry_VisitInfo* visit_info = map_entry->add_visits();
      visit_info->set_visit_time(visit.first.ToInternalValue());
      visit_info->set_transition_type(visit.second);
    }
  }
}

void URLIndexPrivateData::SaveWordStartsMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (word_starts_map_.empty())
    return;
  // For unit testing: Enable saving of the cache as an earlier version to
  // allow testing of cache file upgrading in ReadFromFile().
  // TODO(mrossetti): Instead of intruding on production code with this kind of
  // test harness, save a copy of an older version cache with known results.
  // Implement this when switching the caching over to SQLite.
  if (saved_cache_version_ < 1)
    return;

  WordStartsMapItem* map_item = cache->mutable_word_starts_map();
  map_item->set_item_count(word_starts_map_.size());
  for (const auto& entry : word_starts_map_) {
    WordStartsMapEntry* map_entry = map_item->add_word_starts_map_entry();
    map_entry->set_history_id(entry.first);
    const RowWordStarts& word_starts = entry.second;
    for (auto url_word_start : word_starts.url_word_starts_)
      map_entry->add_url_word_starts(url_word_start);
    for (auto title_word_start : word_starts.title_word_starts_)
      map_entry->add_title_word_starts(title_word_start);
  }
}

bool URLIndexPrivateData::RestorePrivateData(
    const InMemoryURLIndexCacheItem& cache) {
  last_time_rebuilt_from_history_ =
      base::Time::FromInternalValue(cache.last_rebuild_timestamp());
  const base::TimeDelta rebuilt_ago =
      base::Time::Now() - last_time_rebuilt_from_history_;
  if ((rebuilt_ago > base::TimeDelta::FromDays(7)) ||
      (rebuilt_ago < base::TimeDelta::FromDays(-1))) {
    // Cache is more than a week old or, somehow, from some time in the future.
    // It's probably a good time to rebuild the index from history to
    // allow synced entries to now appear, expired entries to disappear, etc.
    // Allow one day in the future to make the cache not rebuild on simple
    // system clock changes such as time zone changes.
    return false;
  }
  if (cache.has_version()) {
    if (cache.version() < kCurrentCacheFileVersion) {
      // Don't try to restore an old format cache file.  (This will cause
      // the InMemoryURLIndex to schedule rebuilding the URLIndexPrivateData
      // from history.)
      return false;
    }
    restored_cache_version_ = cache.version();
  }
  return RestoreWordList(cache) && RestoreWordMap(cache) &&
      RestoreCharWordMap(cache) && RestoreWordIDHistoryMap(cache) &&
      RestoreHistoryInfoMap(cache) && RestoreWordStartsMap(cache);
}

bool URLIndexPrivateData::RestoreWordList(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_list())
    return false;
  const WordListItem& list_item(cache.word_list());
  uint32_t expected_item_count = list_item.word_count();
  uint32_t actual_item_count = list_item.word_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<std::string>& words = list_item.word();
  word_list_.reserve(words.size());
  std::transform(
      words.begin(), words.end(), std::back_inserter(word_list_),
      [](const std::string& word) { return base::UTF8ToUTF16(word); });
  return true;
}

bool URLIndexPrivateData::RestoreWordMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_map())
    return false;
  const WordMapItem& list_item = cache.word_map();
  uint32_t expected_item_count = list_item.item_count();
  uint32_t actual_item_count = list_item.word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  for (const auto& entry : list_item.word_map_entry())
    word_map_[base::UTF8ToUTF16(entry.word())] = entry.word_id();

  return true;
}

bool URLIndexPrivateData::RestoreCharWordMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_char_word_map())
    return false;
  const CharWordMapItem& list_item(cache.char_word_map());
  uint32_t expected_item_count = list_item.item_count();
  uint32_t actual_item_count = list_item.char_word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;

  for (const auto& entry : list_item.char_word_map_entry()) {
    expected_item_count = entry.item_count();
    actual_item_count = entry.word_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    base::char16 uni_char = static_cast<base::char16>(entry.char_16());
    const RepeatedField<int32_t>& word_ids = entry.word_id();
    char_word_map_[uni_char] = WordIDSet(word_ids.begin(), word_ids.end());
  }
  return true;
}

bool URLIndexPrivateData::RestoreWordIDHistoryMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_id_history_map())
    return false;
  const WordIDHistoryMapItem& list_item(cache.word_id_history_map());
  uint32_t expected_item_count = list_item.item_count();
  uint32_t actual_item_count = list_item.word_id_history_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  for (const auto& entry : list_item.word_id_history_map_entry()) {
    expected_item_count = entry.item_count();
    actual_item_count = entry.history_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    WordID word_id = entry.word_id();
    const RepeatedField<int64_t>& history_ids = entry.history_id();
    word_id_history_map_[word_id] =
        HistoryIDSet(history_ids.begin(), history_ids.end());
    for (HistoryID history_id : history_ids)
      history_id_word_map_[history_id].insert(word_id);
  }
  return true;
}

bool URLIndexPrivateData::RestoreHistoryInfoMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_history_info_map())
    return false;
  const HistoryInfoMapItem& list_item(cache.history_info_map());
  uint32_t expected_item_count = list_item.item_count();
  uint32_t actual_item_count = list_item.history_info_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;

  for (const auto& entry : list_item.history_info_map_entry()) {
    HistoryID history_id = entry.history_id();
    history::URLRow url_row(GURL(entry.url()), history_id);
    url_row.set_visit_count(entry.visit_count());
    url_row.set_typed_count(entry.typed_count());
    url_row.set_last_visit(base::Time::FromInternalValue(entry.last_visit()));
    if (entry.has_title())
      url_row.set_title(base::UTF8ToUTF16(entry.title()));
    history_info_map_[history_id].url_row = std::move(url_row);

    // Restore visits list.
    VisitInfoVector visits;
    visits.reserve(entry.visits_size());
    for (const auto& entry_visit : entry.visits()) {
      visits.emplace_back(
          base::Time::FromInternalValue(entry_visit.visit_time()),
          ui::PageTransitionFromInt(entry_visit.transition_type()));
    }
    history_info_map_[history_id].visits = std::move(visits);
  }
  return true;
}

bool URLIndexPrivateData::RestoreWordStartsMap(
    const InMemoryURLIndexCacheItem& cache) {
  // Note that this function must be called after RestoreHistoryInfoMap() has
  // been run as the word starts may have to be recalculated from the urls and
  // page titles.
  if (cache.has_word_starts_map()) {
    const WordStartsMapItem& list_item(cache.word_starts_map());
    uint32_t expected_item_count = list_item.item_count();
    uint32_t actual_item_count = list_item.word_starts_map_entry_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    for (const auto& entry : list_item.word_starts_map_entry()) {
      HistoryID history_id = entry.history_id();
      RowWordStarts word_starts;
      // Restore the URL word starts.
      const RepeatedField<int32_t>& url_starts = entry.url_word_starts();
      word_starts.url_word_starts_ = {url_starts.begin(), url_starts.end()};

      // Restore the page title word starts.
      const RepeatedField<int32_t>& title_starts = entry.title_word_starts();
      word_starts.title_word_starts_ = {title_starts.begin(),
                                        title_starts.end()};

      word_starts_map_[history_id] = std::move(word_starts);
    }
  } else {
    // Since the cache did not contain any word starts we must rebuild then from
    // the URL and page titles.
    for (const auto& entry : history_info_map_) {
      RowWordStarts word_starts;
      const history::URLRow& row = entry.second.url_row;
      const base::string16& url =
          bookmarks::CleanUpUrlForMatching(row.url(), nullptr);
      String16VectorFromString16(url, false, &word_starts.url_word_starts_);
      const base::string16& title =
          bookmarks::CleanUpTitleForMatching(row.title());
      String16VectorFromString16(title, false, &word_starts.title_word_starts_);
      word_starts_map_[entry.first] = std::move(word_starts);
    }
  }
  return true;
}

// static
bool URLIndexPrivateData::URLSchemeIsWhitelisted(
    const GURL& gurl,
    const std::set<std::string>& whitelist) {
  return whitelist.find(gurl.scheme()) != whitelist.end();
}

bool URLIndexPrivateData::ShouldFilter(
    const HistoryID history_id,
    const TemplateURLService* template_url_service) const {
  auto hist_pos = history_info_map_.find(history_id);
  if (hist_pos == history_info_map_.end())
    return true;

  GURL url = hist_pos->second.url_row.url();
  if (!url.is_valid())  // Possible in case of profile corruption.
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
    : word_id_set_(word_id_set), history_id_set_(history_id_set), used_(true) {
}

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem() : used_(true) {
}

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem(
    const SearchTermCacheItem& other) = default;

size_t URLIndexPrivateData::SearchTermCacheItem::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(word_id_set_) +
         base::trace_event::EstimateMemoryUsage(history_id_set_);
}

URLIndexPrivateData::SearchTermCacheItem::~SearchTermCacheItem() {
}

// URLIndexPrivateData::HistoryItemFactorGreater -------------------------------

URLIndexPrivateData::HistoryItemFactorGreater::HistoryItemFactorGreater(
    const HistoryInfoMap& history_info_map)
    : history_info_map_(history_info_map) {
}

URLIndexPrivateData::HistoryItemFactorGreater::~HistoryItemFactorGreater() {
}

bool URLIndexPrivateData::HistoryItemFactorGreater::operator()(
    const HistoryID h1,
    const HistoryID h2) {
  auto entry1(history_info_map_.find(h1));
  if (entry1 == history_info_map_.end())
    return false;
  auto entry2(history_info_map_.find(h2));
  if (entry2 == history_info_map_.end())
    return true;
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
