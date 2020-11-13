// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_URL_INDEX_PRIVATE_DATA_H_
#define COMPONENTS_OMNIBOX_BROWSER_URL_INDEX_PRIVATE_DATA_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/containers/stack.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/in_memory_url_index_cache.pb.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/scored_history_match.h"

class HistoryQuickProviderTest;
class TemplateURLService;

namespace bookmarks {
class BookmarkModel;
}

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {
class HistoryDatabase;
class InMemoryURLIndex;
}

// Current version of the cache file.
static const int kCurrentCacheFileVersion = 5;

// A structure private to InMemoryURLIndex describing its internal data and
// providing for restoring, rebuilding and updating that internal data. As
// this class is for exclusive use by the InMemoryURLIndex class there should
// be no calls from any other class.
//
// All public member functions are called on the main thread unless otherwise
// annotated.
class URLIndexPrivateData
    : public base::RefCountedThreadSafe<URLIndexPrivateData> {
 public:
  // The maximum number of recent visits stored.  Public so that
  // ScoredHistoryMatch can enuse that this number is greater than the number
  // of visits it wants to use for scoring.
  static constexpr size_t kMaxVisitsToStoreInCache = 10;

  URLIndexPrivateData();

  // Given a base::string16 in |term_string|, scans the history index and
  // returns a vector with all scored, matching history items. The
  // |term_string| is broken down into individual terms (words), each of which
  // must occur in the candidate history item's URL or page title for the item
  // to qualify; however, the terms do not necessarily have to be adjacent. We
  // also allow breaking |term_string| at |cursor_position| (if
  // set). Once we have a set of candidates, they are filtered to ensure
  // that all |term_string| terms, as separated by whitespace and the
  // cursor (if set), occur within the candidate's URL or page title.
  // Scores are then calculated on no more than |kItemsToScoreLimit|
  // candidates, as the scoring of such a large number of candidates may
  // cause perceptible typing response delays in the omnibox. This is
  // likely to occur for short omnibox terms such as 'h' and 'w' which
  // will be found in nearly all history candidates. Results are sorted by
  // descending score. The full results set (i.e. beyond the
  // |kItemsToScoreLimit| limit) will be retained and used for subsequent calls
  // to this function. In total, |max_matches| of items will be returned in the
  // |ScoredHistoryMatches| vector.
  ScoredHistoryMatches HistoryItemsForTerms(
      base::string16 term_string,
      size_t cursor_position,
      size_t max_matches,
      bookmarks::BookmarkModel* bookmark_model,
      TemplateURLService* template_url_service);

  // Adds the history item in |row| to the index if it does not already already
  // exist and it meets the minimum 'quick' criteria. If the row already exists
  // in the index then the index will be updated if the row still meets the
  // criteria, otherwise the row will be removed from the index. Returns true
  // if the index was actually updated. |scheme_whitelist| is used to filter
  // non-qualifying schemes. |history_service| is used to schedule an update to
  // the recent visits component of this URL's entry in the index.
  bool UpdateURL(history::HistoryService* history_service,
                 const history::URLRow& row,
                 const std::set<std::string>& scheme_whitelist,
                 base::CancelableTaskTracker* tracker);

  // Updates the entry for |url_id| in the index, replacing its
  // recent visits information with |recent_visits|.  If |url_id|
  // is not in the index, does nothing.
  void UpdateRecentVisits(history::URLID url_id,
                          const history::VisitVector& recent_visits);

  // Using |history_service| schedules an update (using the historyDB
  // thread) for the recent visits information for |url_id|.  Unless
  // something unexpectedly goes wrong, UdpateRecentVisits() should
  // eventually be called from a callback.
  void ScheduleUpdateRecentVisits(history::HistoryService* history_service,
                                  history::URLID url_id,
                                  base::CancelableTaskTracker* tracker);

  // Deletes index data for the history item with the given |url|.
  // The item may not have actually been indexed, which is the case if it did
  // not previously meet minimum 'quick' criteria. Returns true if the index
  // was actually updated.
  bool DeleteURL(const GURL& url);

  // Constructs a new object by restoring its contents from the cache file
  // at |path|. Returns the new URLIndexPrivateData which on success will
  // contain the restored data but upon failure will be empty.
  // This function should be run on the the file thread.
  static scoped_refptr<URLIndexPrivateData> RestoreFromFile(
      const base::FilePath& path);

  // Constructs a new object by rebuilding its contents from the history
  // database in |history_db|. Returns the new URLIndexPrivateData which on
  // success will contain the rebuilt data but upon failure will be empty.
  static scoped_refptr<URLIndexPrivateData> RebuildFromHistory(
      history::HistoryDatabase* history_db,
      const std::set<std::string>& scheme_whitelist);

  // Writes |private_data| as a cache file to |file_path| and returns success.
  static bool WritePrivateDataToCacheFileTask(
      scoped_refptr<URLIndexPrivateData> private_data,
      const base::FilePath& file_path);

  // Creates a copy of ourself.
  scoped_refptr<URLIndexPrivateData> Duplicate() const;

  // Returns true if there is no data in the index.
  bool Empty() const;

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns true if |row| is indexed.
  bool IsUrlRowIndexed(const history::URLRow& row) const;

  // Returns true if a visit with |transition| should not be returned.
  static bool ShouldExcludeBecauseOfCctTransition(
      ui::PageTransition transition);

  // Returns true |visits| should be excluded (not shown).
  static bool ShouldExcludeBecauseOfCctVisits(
      const history::VisitVector& visits);

 private:
  friend class base::RefCountedThreadSafe<URLIndexPrivateData>;
  ~URLIndexPrivateData();

  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CalculateWordStartsOffsets);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest,
                           CalculateWordStartsOffsetsUnderscore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, HugeResultSet);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, ReadVisitsFromHistory);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, RebuildFromHistoryIfCacheOld);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Scoring);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TrimHistoryIds);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TypedCharacterCaching);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, WhitelistedURLs);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // Support caching of term results so that we can optimize searches which
  // build upon a previous search. Each entry in this map represents one
  // search term from the most recent search. For example, if the user had
  // typed "google blog trans" and then typed an additional 'l' (at the end,
  // of course) then there would be four items in the cache: 'blog', 'google',
  // 'trans', and 'transl'. All would be marked as being in use except for the
  // 'trans' item; its cached data would have been used when optimizing the
  // construction of the search results candidates for 'transl' but then would
  // no longer needed.
  //
  // Items stored in the search term cache. If a search term exactly matches one
  // in the cache then we can quickly supply the proper |history_id_set_| (and
  // marking the cache item as being |used_|. If we find a prefix for a search
  // term in the cache (which is very likely to occur as the user types each
  // term into the omnibox) then we can short-circuit the index search for those
  // characters in the prefix by returning the |word_id_set|. In that case we do
  // not mark the item as being |used_|.
  struct SearchTermCacheItem {
    SearchTermCacheItem(const WordIDSet& word_id_set,
                        const HistoryIDSet& history_id_set);
    // Creates a cache item for a term which has no results.
    SearchTermCacheItem();
    SearchTermCacheItem(const SearchTermCacheItem& other);

    ~SearchTermCacheItem();

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

    WordIDSet word_id_set_;
    HistoryIDSet history_id_set_;
    bool used_;  // True if this item has been used for the current term search.
  };
  typedef std::map<base::string16, SearchTermCacheItem> SearchTermCacheMap;

  // A helper predicate class used to filter excess history items when the
  // candidate results set is too large.
  class HistoryItemFactorGreater {
   public:
    explicit HistoryItemFactorGreater(const HistoryInfoMap& history_info_map);
    ~HistoryItemFactorGreater();

    bool operator()(const HistoryID h1, const HistoryID h2);

   private:
    const HistoryInfoMap& history_info_map_;
  };

  // URL History indexing support functions.

  // Composes a vector of history item IDs by intersecting the set for each word
  // in |unsorted_words|.
  HistoryIDVector HistoryIDsFromWords(const String16Vector& unsorted_words);

  // Trims the candidate pool in advance of doing proper substring searching, to
  // cap the cost of such searching. Discards the least-relevant items (based on
  // visit stats), which are least likely to score highly in the end.  To
  // minimize the risk of discarding a valuable URL, the candidate pool is still
  // left two orders of magnitude larger than the final number of results
  // returned from the HQP. Returns whether anything was trimmed.
  bool TrimHistoryIdsPool(HistoryIDVector* history_ids) const;

  // Helper function to HistoryIDSetFromWords which composes a set of history
  // ids for the given term given in |term|.
  HistoryIDSet HistoryIDsForTerm(const base::string16& term);

  // Given a set of Char16s, finds words containing those characters.
  WordIDSet WordIDSetForTermChars(const Char16Set& term_chars);

  // Helper function for HistoryItemsForTerms().  Fills in |scored_items| from
  // the matches listed in |history_ids|.
  void HistoryIdsToScoredMatches(HistoryIDVector history_ids,
                                 const base::string16& lower_raw_string,
                                 const TemplateURLService* template_url_service,
                                 bookmarks::BookmarkModel* bookmark_model,
                                 ScoredHistoryMatches* scored_items) const;

  // Fills in |terms_to_word_starts_offsets| according to where the word starts
  // in each term.  For example, in the term "-foo" the word starts at offset 1.
  static void CalculateWordStartsOffsets(
      const String16Vector& terms,
      WordStarts* terms_to_word_starts_offsets);

  // Indexes one URL history item as described by |row|. Returns true if the
  // row was actually indexed. |scheme_whitelist| is used to filter
  // non-qualifying schemes.  If |history_db| is not NULL then this function
  // uses the history database synchronously to get the URL's recent visits
  // information.  This mode should/ only be used on the historyDB thread.
  // If |history_db| is NULL, then this function uses |history_service| to
  // schedule a task on the historyDB thread to fetch and update the recent
  // visits information.
  bool IndexRow(history::HistoryDatabase* history_db,
                history::HistoryService* history_service,
                const history::URLRow& row,
                const std::set<std::string>& scheme_whitelist,
                base::CancelableTaskTracker* tracker);

  // Parses and indexes the words in the URL and page title of |row| and
  // calculate the word starts in each, saving the starts in |word_starts|.
  void AddRowWordsToIndex(const history::URLRow& row,
                          RowWordStarts* word_starts);

  // Given a single word in |uni_word|, adds a reference for the containing
  // history item identified by |history_id| to the index.
  void AddWordToIndex(const base::string16& uni_word, HistoryID history_id);

  // Adds a new entry to |word_list_|. Uses previously freed positions if
  // available.
  WordID AddNewWordToWordList(const base::string16& term);

  // Removes |row| and all associated words and characters from the index.
  void RemoveRowFromIndex(const history::URLRow& row);

  // Removes all words and characters associated with |row| from the index.
  void RemoveRowWordsFromIndex(const history::URLRow& row);

  // Clears |used_| for each item in the search term cache.
  void ResetSearchTermCache();

  // Caches the index private data and writes the cache file to the profile
  // directory.  Called by WritePrivateDataToCacheFileTask.
  bool SaveToFile(const base::FilePath& file_path);

  // Encode a data structure into the protobuf |cache|.
  void SavePrivateData(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordList(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordMap(in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveCharWordMap(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordIDHistoryMap(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveHistoryInfoMap(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordStartsMap(
      in_memory_url_index::InMemoryURLIndexCacheItem* cache) const;

  // Decode a data structure from the protobuf |cache|. Return false if there
  // is any kind of failure.
  bool RestorePrivateData(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordList(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordMap(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreCharWordMap(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordIDHistoryMap(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreHistoryInfoMap(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordStartsMap(
      const in_memory_url_index::InMemoryURLIndexCacheItem& cache);

  // Determines if |gurl| has a whitelisted scheme and returns true if so.
  static bool URLSchemeIsWhitelisted(const GURL& gurl,
                                     const std::set<std::string>& whitelist);

  // Returns true if the URL associated with |history_id| is missing, malformed,
  // or otherwise should not be displayed.  (Results from the default search
  // provider fall into this category.)
  bool ShouldFilter(const HistoryID history_id,
                    const TemplateURLService* template_url_service) const;

  // Cache of search terms.
  SearchTermCacheMap search_term_cache_;

  // Start of data members that are cached -------------------------------------

  // The version of the cache file most recently used to restore this instance
  // of the private data. If the private data was rebuilt from the history
  // database this will be 0.
  int restored_cache_version_;

  // The last time the data was rebuilt from the history database.
  base::Time last_time_rebuilt_from_history_;

  // A list of all of indexed words. The index of a word in this list is the
  // ID of the word in the word_map_. It reduces the memory overhead by
  // replacing a potentially long and repeated string with a simple index.
  String16Vector word_list_;

  // A list of available words slots in |word_list_|. An available word slot
  // is the index of a unused word in word_list_ vector, also referred to as
  // a WordID. As URL visits are added or modified new words may be added to
  // the index, in which case any available words are used, if any, and then
  // words are added to the end of the word_list_. When URL visits are
  // modified or deleted old words may be removed from the index, in which
  // case the slots for those words are added to available_words_ for reuse
  // by future URL updates.
  base::stack<WordID> available_words_;

  // A one-to-one mapping from the a word string to its slot number (i.e.
  // WordID) in the |word_list_|.
  WordMap word_map_;

  // A one-to-many mapping from a single character to all WordIDs of words
  // containing that character.
  CharWordIDMap char_word_map_;

  // A one-to-many mapping from a WordID to all HistoryIDs (the row_id as
  // used in the history database) of history items in which the word occurs.
  WordIDHistoryMap word_id_history_map_;

  // A one-to-many mapping from a HistoryID to all WordIDs of words that occur
  // in the URL and/or page title of the history item referenced by that
  // HistoryID.
  HistoryIDWordMap history_id_word_map_;

  // A one-to-one mapping from HistoryID to the history item data governing
  // index inclusion and relevance scoring.
  HistoryInfoMap history_info_map_;

  // A one-to-one mapping from HistoryID to the word starts detected in each
  // item's URL and page title.
  WordStartsMap word_starts_map_;

  // End of data members that are cached ---------------------------------------

  // For unit testing only. Specifies the version of the cache file to be saved.
  // Used only for testing upgrading of an older version of the cache upon
  // restore.
  int saved_cache_version_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_URL_INDEX_PRIVATE_DATA_H_
