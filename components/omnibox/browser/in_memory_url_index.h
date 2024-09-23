// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_H_
#define COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_H_

#include <stddef.h>

#include <functional>
#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "components/search_engines/template_url_service.h"

class FakeAutocompleteProviderClient;
class HistoryQuickProviderTest;
class OmniboxTriggeredFeatureService;

namespace base {
class SequencedTaskRunner;
}

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryDatabase;
class HQPPerfTestOnePopularURL;
}  // namespace history

class URLIndexPrivateData;

typedef std::set<std::string> SchemeSet;

// The URL history source.
// Holds portions of the URL database in memory in an indexed form.  Used to
// quickly look up matching URLs for a given query string.  Used by
// the HistoryQuickProvider for inline autocomplete and to provide URL
// matches to the omnibox.
//
// Note about multi-byte codepoints and the data structures in the
// InMemoryURLIndex class: One will quickly notice that no effort is made to
// insure that multi-byte character boundaries are detected when indexing the
// words and characters in the URL history database except when converting
// URL strings to lowercase. Multi-byte-edness makes no difference when
// indexing or when searching the index as the final filtering of results
// is dependent on the comparison of a string of bytes, not individual
// characters. While the lookup of those bytes during a search in the
// |char_word_map_| could serve up words in which the individual char16
// occurs as a portion of a composite character the next filtering step
// will eliminate such words except in the case where a single character
// is being searched on and which character occurs as the second char16 of a
// multi-char16 instance.
class InMemoryURLIndex : public KeyedService,
                         public history::HistoryServiceObserver,
                         public base::trace_event::MemoryDumpProvider {
 public:
  // `history_service` may be null during unit testing.
  InMemoryURLIndex(bookmarks::BookmarkModel* bookmark_model,
                   history::HistoryService* history_service,
                   TemplateURLService* template_url_service,
                   const base::FilePath& history_dir,
                   const SchemeSet& client_schemes_to_allowlist);
  ~InMemoryURLIndex() override;
  InMemoryURLIndex(const InMemoryURLIndex&) = delete;
  InMemoryURLIndex& operator=(const InMemoryURLIndex&) = delete;

  // Opens and prepares the index of historical URL visits. Rebuilds the index
  // from History.
  void Init();

  // Scans the history index and returns a vector with all scored, matching
  // history items. This entry point simply forwards the call to
  // `URLIndexPrivateData`. For a complete description of this function refer to
  // that class. If `cursor_position` is `npos`, the function doesn't do
  // anything special with the cursor; this is equivalent to the cursor being at
  // the end. If `host_filter` is not empty, filters matches by host. In total,
  // `max_matches` of items will be returned.
  ScoredHistoryMatches HistoryItemsForTerms(
      const std::u16string& term_string,
      size_t cursor_position,
      const std::string& host_filter,
      size_t max_matches,
      OmniboxTriggeredFeatureService* triggered_feature_service);

  // Returns URL hosts that have been visited more than a threshold.
  const std::vector<std::string>& HighlyVisitedHosts() const;

  // Deletes the index entry, if any, for the given |url|.
  void DeleteURL(const GURL& url);

  // Indicates that the index restoration is complete.
  bool restored() const { return restored_; }

 private:
  friend class ::FakeAutocompleteProviderClient;
  friend class ::HistoryQuickProviderTest;
  friend class history::HQPPerfTestOnePopularURL;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, ExpireRow);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // HistoryDBTask used to rebuild our private data from the history database.
  class RebuildPrivateDataFromHistoryDBTask : public history::HistoryDBTask {
   public:
    explicit RebuildPrivateDataFromHistoryDBTask(
        InMemoryURLIndex* index,
        const SchemeSet& scheme_allowlist);
    RebuildPrivateDataFromHistoryDBTask(
        const RebuildPrivateDataFromHistoryDBTask&) = delete;
    RebuildPrivateDataFromHistoryDBTask& operator=(
        const RebuildPrivateDataFromHistoryDBTask&) = delete;
    ~RebuildPrivateDataFromHistoryDBTask() override;

    bool RunOnDBThread(history::HistoryBackend* backend,
                       history::HistoryDatabase* db) override;
    void DoneRunOnMainThread() override;

   private:
    raw_ptr<InMemoryURLIndex, AcrossTasksDanglingUntriaged>
        index_;                   // Call back to this index at completion.
    SchemeSet scheme_allowlist_;  // Schemes to be indexed.
    bool succeeded_ = false;      // Indicates if the rebuild was successful.
    scoped_refptr<URLIndexPrivateData> data_;  // The rebuilt private data.
  };

  // Clears the in-memory cache entirely. Called when History is cleared.
  void ClearPrivateData();

  // Schedules a history task to rebuild our private data from the history
  // database.
  void ScheduleRebuildFromHistory();

  // Callback used by RebuildPrivateDataFromHistoryDBTask to signal completion
  // or rebuilding our private data from the history database. |succeeded|
  // will be true if the rebuild was successful. |data| will point to a new
  // instanceof the private data just rebuilt.
  void DoneRebuildingPrivateDataFromHistoryDB(
      bool succeeded,
      scoped_refptr<URLIndexPrivateData> private_data);

  // KeyedService:
  // Signals that any outstanding initialization should be canceled.
  void Shutdown() override;

  // HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // MemoryDumpProvider:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  // Returns a pointer to our private data. For unit testing only.
  URLIndexPrivateData* private_data() { return private_data_.get(); }

  // Returns a pointer to our private data cancelable request tracker. For
  // unit testing only.
  base::CancelableTaskTracker* private_data_tracker() {
    return &private_data_tracker_;
  }

  // Returns the set of allowlisted schemes. For unit testing only.
  const SchemeSet& scheme_allowlist() { return scheme_allowlist_; }

  // The BookmarkModel; may be null when testing.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // The HistoryService; may be null when testing.
  raw_ptr<history::HistoryService> history_service_;

  // The TemplateURLService; may be null when testing.  Used to identify URLs
  // that are from the default search provider.
  raw_ptr<TemplateURLService> template_url_service_;

  // Only URLs with a allowlisted scheme are indexed.
  SchemeSet scheme_allowlist_;

  // The index's durable private data.
  scoped_refptr<URLIndexPrivateData> private_data_;

  // Task runner used for operations which require disk access.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::CancelableTaskTracker private_data_tracker_;

  // Set to true once the shutdown process has begun.
  bool shutdown_ = false;

  // Set to true once the index restoration is complete.
  bool restored_ = false;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::ThreadChecker thread_checker_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_H_
