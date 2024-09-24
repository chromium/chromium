// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/browser/web_history_service_observer.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "url/gurl.h"

FORWARD_DECLARE_TEST(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions);

namespace history {

class BrowsingHistoryDriver;
class QueryResults;
struct QueryOptions;

// Interacts with HistoryService, WebHistoryService, and SyncService to query
// history and provide results to the associated BrowsingHistoryDriver.
class BrowsingHistoryService : public HistoryServiceObserver,
                               public WebHistoryServiceObserver,
                               public syncer::SyncServiceObserver {
 public:
  // Represents a history entry to be shown to the user, representing either
  // a local or remote visit. A single entry can represent multiple visits,
  // since only the most recent visit on a particular day is shown.
  struct HistoryEntry {
    // Values indicating whether an entry represents only local visits, only
    // remote visits, or a mixture of both.
    enum EntryType {
      EMPTY_ENTRY = 0,
      LOCAL_ENTRY,
      REMOTE_ENTRY,
      COMBINED_ENTRY
    };

    HistoryEntry(EntryType type,
                 const GURL& url,
                 const std::u16string& title,
                 base::Time time,
                 const std::string& client_id,
                 bool is_search_result,
                 const std::u16string& snippet,
                 bool blocked_visit,
                 const GURL& remote_icon_url_for_uma,
                 int visit_count,
                 int typed_count,
                 std::optional<std::string> app_id);
    HistoryEntry();
    HistoryEntry(const HistoryEntry& other);
    virtual ~HistoryEntry();

    // Comparison function for sorting HistoryEntries from newest to oldest.
    static bool SortByTimeDescending(const HistoryEntry& entry1,
                                     const HistoryEntry& entry2);

    // The type of visits this entry represents: local, remote, or both.
    EntryType entry_type;

    GURL url;

    std::u16string title;  // Title of the entry. May be empty.

    // The time of the entry. Usually this will be the time of the most recent
    // visit to `url` on a particular day as defined in the local timezone.
    base::Time time;

    // The sync ID of the client on which the most recent visit occurred.
    std::string client_id;

    // Timestamps of all local or remote visits the same URL on the same day.
    // TODO(skym): These should probably be converted to base::Time.
    std::set<int64_t> all_timestamps;

    // If true, this entry is a search result.
    bool is_search_result;

    // The entry's search snippet, if this entry is a search result.
    std::u16string snippet;

    // Whether this entry was blocked when it was attempted.
    bool blocked_visit;

    // Optional parameter used to plumb footprints associated icon url.
    GURL remote_icon_url_for_uma;

    // Total number of times this URL has been visited.
    int visit_count = 0;

    // Number of times this URL has been manually entered in the URL bar.
    int typed_count = 0;

    // ID of the app this entry was generated for. Set to a non-null value
    // on Android only.
    std::optional<std::string> app_id;
  };

  // Contains information about a completed history query.
  struct QueryResultsInfo {
    ~QueryResultsInfo();

    // The query search text.
    std::u16string search_text;

    // Whether this query reached the end of all results, or if there are more
    // history entries that can be fetched through paging.
    bool reached_beginning = false;

    // Whether the last call to Web History timed out.
    bool sync_timed_out = false;

    // Whether the last call to Web History returned successfully with a message
    // body. During continuation queries we are not guaranteed to always make a
    // call to WebHistory, and this value could reflect the state from previous
    // queries.
    bool has_synced_results = false;
  };

  BrowsingHistoryService(BrowsingHistoryDriver* driver,
                         HistoryService* local_history,
                         syncer::SyncService* sync_service);

  BrowsingHistoryService(const BrowsingHistoryService&) = delete;
  BrowsingHistoryService& operator=(const BrowsingHistoryService&) = delete;

  ~BrowsingHistoryService() override;

  // Start a new query with the given parameters.
  virtual void QueryHistory(const std::u16string& search_text,
                            const QueryOptions& options);

  // Fetch all the app IDs used in the database.
  void GetAllAppIds();

  // Callback invoked when the app ID fetching task is completed.
  void OnGetAllAppIds(GetAllAppIdsResult result);

  // Gets a version of the last time any webpage on the given host was visited,
  // by using the min("last navigation time", x minutes ago) as the upper bound
  // of the GetLastVisitToHost query. This is done in order to provide the user
  // with a more useful sneak peak into their navigation history, by excluding
  // the site(s) they were just on.
  void GetLastVisitToHostBeforeRecentNavigations(
      const std::string& host_name,
      base::OnceCallback<void(base::Time)> callback);

  // Removes `items` from history.
  // TODO(tommycli): Update this API to take only URLs and timestamps, because
  // callers only have that information, and only that information is used by
  // the actual implementation.
  void RemoveVisits(const std::vector<HistoryEntry>& items);

  // SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 protected:
  // Constructor that allows specifying more dependencies for unit tests.
  BrowsingHistoryService(BrowsingHistoryDriver* driver,
                         HistoryService* local_history,
                         syncer::SyncService* sync_service,
                         std::unique_ptr<base::OneShotTimer> web_history_timer);
  // Should be used only for tests when mocking the service.
  BrowsingHistoryService();

 private:
  FRIEND_TEST_ALL_PREFIXES(::BrowsingHistoryHandlerTest,
                           ObservingWebHistoryDeletions);

  // Used to hold and track query state between asynchronous calls.
  struct QueryHistoryState;

  // Moves results from `state` into `results`, merging both remote and local
  // results together and maintaining reverse chronological order. Any results
  // with the same URL will be merged together for each day. Often holds back
  // some results in `state` from one of the two sources to ensure that they're
  // always returned to the driver in correct order. This function also updates
  // the end times in `state` for both sources that the next query should be
  // made against.
  static void MergeDuplicateResults(QueryHistoryState* state,
                                    std::vector<HistoryEntry>* results);

  // Core implementation of history querying.
  void QueryHistoryInternal(scoped_refptr<QueryHistoryState> state);

  // Callback from the history system when a history query has completed.
  void QueryComplete(scoped_refptr<QueryHistoryState> state,
                     QueryResults results);

  // Callback from the history system when the last visit query has completed.
  // May need to do a second query based on the results.
  void OnLastVisitBeforeRecentNavigationsComplete(
      const std::string& host_name,
      base::Time query_start_time,
      base::OnceCallback<void(base::Time)> callback,
      HistoryLastVisitResult result);

  // Callback from the history system when the last visit query has completed
  // the second time.
  void OnLastVisitBeforeRecentNavigationsComplete2(
      base::OnceCallback<void(base::Time)> callback,
      HistoryLastVisitResult result);

  // Combines the query results from the local history database and the history
  // server, and sends the combined results to the
  // BrowsingHistoryDriver.
  void ReturnResultsToDriver(scoped_refptr<QueryHistoryState> state);

  // Callback from `web_history_timer_` when a response from web history has
  // not been received in time.
  void WebHistoryTimeout(scoped_refptr<QueryHistoryState> state);

  // Callback from the WebHistoryService when a query has completed.
  void WebHistoryQueryComplete(
      scoped_refptr<QueryHistoryState> state,
      base::Time start_time,
      WebHistoryService::Request* request,
      base::optional_ref<const base::Value::Dict> results_dict);

  // Callback telling us whether other forms of browsing history were found
  // on the history server.
  void OtherFormsOfBrowsingHistoryQueryComplete(
      bool found_other_forms_of_browsing_history);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // Callback from history server when visits were deleted.
  void RemoveWebHistoryComplete(bool success);

  // HistoryServiceObserver implementation.
  void OnHistoryDeletions(HistoryService* history_service,
                          const DeletionInfo& deletion_info) override;

  // WebHistoryServiceObserver implementation.
  void OnWebHistoryDeleted() override;

  // Tracker for search requests to the history service.
  base::CancelableTaskTracker query_task_tracker_;

  // The currently-executing request for synced history results.
  // Deleting the request will cancel it.
  std::unique_ptr<WebHistoryService::Request> web_history_request_;

  // True if there is a pending delete requests to the web service.
  bool has_pending_delete_request_ = false;

  // Tracker for delete requests to the history service.
  base::CancelableTaskTracker delete_task_tracker_;

  // Timer used to implement a timeout on a Web History response.
  std::unique_ptr<base::OneShotTimer> web_history_timer_;

  // HistoryService (local history) observer.
  base::ScopedObservation<HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  // WebHistoryService (synced history) observer.
  base::ScopedObservation<WebHistoryService, WebHistoryServiceObserver>
      web_history_service_observation_{this};

  // SyncService observer listens to late initialization of history sync.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  // Whether the last call to Web History returned synced results.
  bool has_synced_results_ = false;

  // Whether there are other forms of browsing history on the history server.
  bool has_other_forms_of_browsing_history_ = false;

  raw_ptr<BrowsingHistoryDriver> driver_;

  raw_ptr<HistoryService> local_history_;

  raw_ptr<syncer::SyncService> sync_service_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<BrowsingHistoryService> weak_factory_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_
