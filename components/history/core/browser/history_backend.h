// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_backend_delegate.h"
#include "components/favicon/core/favicon_database.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/expire_history_backend.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/history/core/browser/visit_tracker.h"
#include "components/sync/service/sync_service.h"
#include "sql/init_status.h"
#include "url/origin.h"

class SkBitmap;

namespace base {
class SingleThreadTaskRunner;
}

namespace favicon {
class FaviconBackend;
}

namespace sql {
class Transaction;
}

namespace syncer {
class DataTypeControllerDelegate;
}

namespace history {
struct DownloadRow;
class HistoryBackendClient;
class HistoryBackendDBBaseTest;
class HistoryBackendObserver;
class HistoryBackend;
class TestHistoryBackend;
class HistoryBackendTest;
class HistoryDatabase;
struct HistoryDatabaseParams;
class HistoryDBTask;
class HistorySyncBridge;
class InMemoryHistoryBackend;
class URLDatabase;

// Returns a formatted version of `url` with the HTTP/HTTPS scheme, port,
// username/password, and any trivial subdomains (e.g., "www.", "m.") removed.
std::u16string FormatUrlForRedirectComparison(const GURL& url);

// Advances (if `day` >= 0) or backtracks (if `day` < 0) from `time` by
// abs(`day`) calendar days in local timezone and returns the midnight of the
// resulting day.
base::Time MidnightNDaysLater(base::Time time, int days);

// Keeps track of a queued HistoryDBTask. This class lives solely on the
// DB thread.
class QueuedHistoryDBTask {
 public:
  QueuedHistoryDBTask(
      std::unique_ptr<HistoryDBTask> task,
      scoped_refptr<base::SequencedTaskRunner> origin_loop,
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled);

  QueuedHistoryDBTask(const QueuedHistoryDBTask&) = delete;
  QueuedHistoryDBTask& operator=(const QueuedHistoryDBTask&) = delete;

  ~QueuedHistoryDBTask();

  bool is_canceled();
  bool Run(HistoryBackend* backend, HistoryDatabase* db);
  void DoneRun();

 private:
  std::unique_ptr<HistoryDBTask> task_;
  scoped_refptr<base::SequencedTaskRunner> origin_loop_;
  base::CancelableTaskTracker::IsCanceledCallback is_canceled_;
};

// *See the .cc file for more information on the design.*
//
// Internal history implementation which does most of the work of the history
// system. This runs on a background thread (to not block the browser when we
// do expensive operations) and is NOT threadsafe, so it must only be called
// from message handlers on the background thread. Invoking on another thread
// requires threadsafe refcounting.
//
// Most functions here are just the implementations of the corresponding
// functions in the history service. These functions are not documented
// here, see the history service for behavior.
class HistoryBackend : public base::RefCountedThreadSafe<HistoryBackend>,
                       public HistoryBackendForSync,
                       public HistoryBackendNotifier,
                       public favicon::FaviconBackendDelegate {
 public:
  // Interface implemented by the owner of the HistoryBackend object. Normally,
  // the history service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a history service object.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns whether the given URL can/should be added to the history.
    virtual bool CanAddURL(const GURL& url) const = 0;

    // Called when the database cannot be read correctly for some reason.
    // `diagnostics` contains information about the underlying database
    // which can help in identifying the cause of the profile error.
    virtual void NotifyProfileError(sql::InitStatus init_status,
                                    const std::string& diagnostics) = 0;

    // Sets the in-memory history backend. The in-memory backend is created by
    // the main backend. For non-unit tests, this happens on the background
    // thread. It is to be used on the main thread, so this would transfer
    // it to the history service. Unit tests can override this behavior.
    //
    // This function is NOT guaranteed to be called. If there is an error,
    // there may be no in-memory database.
    virtual void SetInMemoryBackend(
        std::unique_ptr<InMemoryHistoryBackend> backend) = 0;

    // Notify HistoryService that the favicons for the given page URLs (e.g.
    // http://www.google.com) and the given icon URL (e.g.
    // http://www.google.com/favicon.ico) have changed. HistoryService notifies
    // any registered callbacks. It is valid to call NotifyFaviconsChanged()
    // with non-empty `page_urls` and an empty `icon_url` and vice versa.
    virtual void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                       const GURL& icon_url) = 0;

    // Notify HistoryService that the user is visiting a URL. The event will
    // be forwarded to the HistoryServiceObservers in the correct thread.
    virtual void NotifyURLVisited(
        const URLRow& url_row,
        const VisitRow& visit_row,
        std::optional<int64_t> local_navigation_id) = 0;

    // Notify HistoryService that some URLs have been modified. The event will
    // be forwarded to the HistoryServiceObservers in the correct thread.
    virtual void NotifyURLsModified(const URLRows& changed_urls) = 0;

    // Notify HistoryService that some or all of the URLs have been deleted.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyDeletions(DeletionInfo deletion_info) = 0;

    // Notify HistoryService that partitioned visited links have been added.
    // This event will be forwarded to PartitionedVisitedLinkWriter, where the
    // corresponding visited links will be added to the in-memory hashtable.
    virtual void NotifyVisitedLinksAdded(const HistoryAddPageArgs& args) = 0;

    // Notify HistoryService that partitioned visited links have been deleted.
    // This event will be forwarded to the PartitionedVisitedLinkWriter, where
    // the corresponding visited links will be deleted from the in-memory
    // hashtable.
    virtual void NotifyVisitedLinksDeleted(
        const std::vector<DeletedVisitedLink>& links) = 0;

    // Notify HistoryService that some keyword has been searched using omnibox.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                                KeywordID keyword_id,
                                                const std::u16string& term) = 0;

    // Notify HistoryService that keyword search term has been deleted.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyKeywordSearchTermDeleted(URLID url_id) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  // Check if the transition should increment the typed_count of a visit.
  static bool IsTypedIncrement(ui::PageTransition transition);

  // The number of days old a history entry can be before it is considered "old"
  // and is deleted.
  static constexpr int kExpireDaysThreshold = 90;

  // Init must be called to complete object creation. This object can be
  // constructed on any thread, but all other functions including Init() must
  // be called on the history thread.
  //
  // `history_dir` is the directory where the history files will be placed.
  // See the definition of BroadcastNotificationsCallback above. This function
  // takes ownership of the callback pointer.
  //
  // `history_client` is used to determine bookmarked URLs when deleting and
  // may be null.
  //
  // This constructor is fast and does no I/O, so can be called at any time.
  HistoryBackend(std::unique_ptr<Delegate> delegate,
                 std::unique_ptr<HistoryBackendClient> backend_client,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);

  HistoryBackend(const HistoryBackend&) = delete;
  HistoryBackend& operator=(const HistoryBackend&) = delete;

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // `force_fail` can be set during unittests to unconditionally fail to init.
  void Init(bool force_fail,
            const HistoryDatabaseParams& history_database_params);

  // Notification that the history system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction, so it will
  // actually be deleted.
  void Closing();

#if BUILDFLAG(IS_IOS)
  // Persists any in-flight state, without actually shutting down the history
  // system. This is intended for use when the application is backgrounded.
  void PersistState();
#endif

  void ClearCachedDataForContextID(ContextID context_id);

  // Clears all on-demand favicons.
  void ClearAllOnDemandFavicons();

  // Gets the counts and last time of URLs that belong to `origins` in the
  // history database. Origins that are not in the history database will be in
  // the map with a count and time of 0.
  // Returns an empty map if db_ is not initialized.
  OriginCountAndLastVisitMap GetCountsAndLastVisitForOrigins(
      const std::set<GURL>& origins) const;

  // Navigation ----------------------------------------------------------------

  // `request.time` must be unique with high probability.
  void AddPage(const HistoryAddPageArgs& request);
  void SetPageTitle(const GURL& url, const std::u16string& title);
  void AddPageNoVisitForBookmark(const GURL& url, const std::u16string& title);
  void UpdateWithPageEndTime(ContextID context_id,
                             int nav_entry_id,
                             const GURL& url,
                             base::Time end_ts);
  void SetBrowsingTopicsAllowed(ContextID context_id,
                                int nav_entry_id,
                                const GURL& url);
  void SetPageLanguageForVisit(ContextID context_id,
                               int nav_entry_id,
                               const GURL& url,
                               const std::string& page_language);
  void SetPageLanguageForVisitByVisitID(VisitID visit_id,
                                        const std::string& page_language);
  void SetPasswordStateForVisit(
      ContextID context_id,
      int nav_entry_id,
      const GURL& url,
      VisitContentAnnotations::PasswordState password_state);
  void SetPasswordStateForVisitByVisitID(
      VisitID visit_id,
      VisitContentAnnotations::PasswordState password_state);
  void AddContentModelAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentModelAnnotations& model_annotations);
  void AddRelatedSearchesForVisit(
      VisitID visit_id,
      const std::vector<std::string>& related_searches);
  void AddSearchMetadataForVisit(VisitID visit_id,
                                 const GURL& search_normalized_url,
                                 const std::u16string& search_terms);
  void AddPageMetadataForVisit(VisitID visit_id,
                               const std::string& alternative_title);
  void SetHasUrlKeyedImageForVisit(VisitID visit_id, bool has_url_keyed_image);

  // Querying ------------------------------------------------------------------

  QueryURLResult QueryURL(const GURL& url, bool want_visits);
  std::vector<QueryURLResult> QueryURLs(const std::vector<GURL>& urls,
                                        bool want_visits);
  QueryResults QueryHistory(const std::u16string& text_query,
                            const QueryOptions& options);

  // Computes the most recent URL(s) that the given canonical URL has
  // redirected to. There may be more than one redirect in a row, so this
  // function will fill the given array with the entire chain. If there are
  // no redirects for the most recent visit of the URL, or the URL is not
  // in history, the array will be empty.
  RedirectList QueryRedirectsFrom(const GURL& url);

  // Similar to above function except computes a chain of redirects to the
  // given URL. Stores the most recent list of redirects ending at `url` in the
  // given RedirectList. For example, if we have the redirect list A -> B -> C,
  // then calling this function with url=C would fill redirects with {B, A}.
  RedirectList QueryRedirectsTo(const GURL& url);

  VisibleVisitCountToHostResult GetVisibleVisitCountToHost(const GURL& url);

  // Request the `result_count` most visited URLs and the chain of
  // redirects leading to each of these URLs. Used by TopSites.
  MostVisitedURLList QueryMostVisitedURLs(int result_count);

  // Request `result_count` of the most repeated queries for the given keyword.
  // Used by TopSites.
  KeywordSearchTermVisitList QueryMostRepeatedQueriesForKeyword(
      KeywordID keyword_id,
      size_t result_count);

  // Statistics ----------------------------------------------------------------

  // Gets the number of URLs as seen in chrome://history within the time range
  // [`begin_time`, `end_time`). Each URL is counted only once per day. For
  // determination of the date, timestamps are converted to dates using local
  // time.
  HistoryCountResult GetHistoryCount(const base::Time& begin_time,
                                     const base::Time& end_time);

  // Returns the number of hosts visited in the last month.
  HistoryCountResult CountUniqueHostsVisitedLastMonth();

  // Returns a collection of domain diversity metrics. Each metric is an
  // unsigned integer representing the number of unique domains (effective
  // top-level domain (eTLD) + 1, e.g. "foo.com", "bar.co.uk") visited within
  // the 1-day, 7-day or 28-day span that ends at a midnight in local timezone.
  //
  // For each of the most recent `number_of_days_to_report` midnights before
  // `report_time`(inclusive), this function computes a subset of
  // {1-day, 7-day, 28-day} metrics whose spanning periods all end on that
  // midnight. This subset of metrics to compute is specified by a bitmask
  // `metric_type_bitmask`, which takes a bitwise combination of
  // kEnableLast1DayMetric, kEnableLast7DayMetric and kEnableLast28DayMetric.
  //
  // All computed metrics are stored in DomainDiversityResults, which represents
  // a collection of DomainMetricSet's. Each DomainMetricSet contains up to 3
  // metrics ending at one unique midnight in the time range of
  // `number_of_days_to_report` days before `report_time`. The collection of
  // DomainMetricSet is sorted reverse chronologically by the ending midnight.
  //
  // For example, when `report_time` = 2019/11/01 00:01am, `number_of_days` = 3,
  // `metric_type_bitmask` = kEnableLast28DayMetric | kEnableLast1DayMetric,
  // DomainDiversityResults will hold 3 DomainMetricSets, each containing 2
  // metrics measuring domain visit counts spanning the following date ranges
  // (all dates are inclusive):
  // {{10/30, 10/3~10/30}, {10/29, 10/2~10/29}, {10/28, 10/1~10/28}}
  //
  // The return value is a pair of results, where the first member counts only
  // local visits, and the second counts both local and foreign (synced) visits.
  // TODO(crbug.com/40896778): Once the "V2" domain diversity metrics are
  // deprecated, return only a single result, the "local" one.
  std::pair<DomainDiversityResults, DomainDiversityResults> GetDomainDiversity(
      base::Time report_time,
      int number_of_days_to_report,
      DomainMetricBitmaskType metric_type_bitmask);

  // Gets unique domains (eTLD+1) visited within the time range
  // [`begin_time`, `end_time`) for local and synced visits sorted in
  // reverse-chronological order.
  DomainsVisitedResult GetUniqueDomainsVisited(base::Time begin_time,
                                               base::Time end_time);

  // Gets all the app IDs used in the database entries.
  GetAllAppIdsResult GetAllAppIds();

  // Gets the last time any webpage on the given host was visited within the
  // time range [`begin_time`, `end_time`). If the given host has not been
  // visited in the given time range, the result will have a null base::Time,
  // but still report success. Only queries http and https hosts.
  HistoryLastVisitResult GetLastVisitToHost(const std::string& host,
                                            base::Time begin_time,
                                            base::Time end_time);

  // Same as the above, but for the given origin instead of host.
  HistoryLastVisitResult GetLastVisitToOrigin(const url::Origin& origin,
                                              base::Time begin_time,
                                              base::Time end_time);

  // Gets the last time `url` was visited before `end_time`. If the given URL
  // has not been visited in the past, the result will have a null base::Time,
  // but still report success.
  HistoryLastVisitResult GetLastVisitToURL(const GURL& url,
                                           base::Time end_time);

  // Gets counts for total visits and days visited for pages matching `host`'s
  // scheme, port, and host. Counts only user-visible visits.
  DailyVisitsResult GetDailyVisitsToHost(const GURL& host,
                                         base::Time begin_time,
                                         base::Time end_time);

  // Favicon -------------------------------------------------------------------

  std::vector<favicon_base::FaviconRawBitmapResult> GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const std::vector<int>& desired_sizes);

  favicon_base::FaviconRawBitmapResult GetLargestFaviconForURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types_list,
      int minimum_size_in_pixels);

  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconsForURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      const std::vector<int>& desired_sizes,
      bool fallback_to_host);

  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconForID(
      favicon_base::FaviconID favicon_id,
      int desired_size);

  std::vector<GURL> GetFaviconURLsForURL(const GURL& page_url) override;

  std::vector<favicon_base::FaviconRawBitmapResult>
  UpdateFaviconMappingsAndFetch(const base::flat_set<GURL>& page_urls,
                                const GURL& icon_url,
                                favicon_base::IconType icon_type,
                                const std::vector<int>& desired_sizes);

  void DeleteFaviconMappings(const base::flat_set<GURL>& page_urls,
                             favicon_base::IconType icon_type);

  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size);

  // `page_urls` must not be empty.
  void SetFavicons(const base::flat_set<GURL>& page_urls,
                   favicon_base::IconType icon_type,
                   const GURL& icon_url,
                   const std::vector<SkBitmap>& bitmaps);

  void CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write);

  bool SetOnDemandFavicons(const GURL& page_url,
                           favicon_base::IconType icon_type,
                           const GURL& icon_url,
                           const std::vector<SkBitmap>& bitmaps);

  bool CanSetOnDemandFavicons(const GURL& page_url,
                              favicon_base::IconType icon_type);

  void SetFaviconsOutOfDateForPage(const GURL& page_url);

  void SetFaviconsOutOfDateBetween(base::Time begin, base::Time end);

  void TouchOnDemandFavicon(const GURL& icon_url);

  void SetImportedFavicons(
      const favicon_base::FaviconUsageDataList& favicon_usage);

  // Downloads -----------------------------------------------------------------

  uint32_t GetNextDownloadId();
  std::vector<DownloadRow> QueryDownloads();
  void UpdateDownload(const DownloadRow& data, bool should_commit_immediately);
  bool CreateDownload(const DownloadRow& history_info);
  void RemoveDownloads(const std::set<uint32_t>& ids);

  // Keyword search terms ------------------------------------------------------

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   KeywordID keyword_id,
                                   const std::u16string& term);

  void DeleteAllSearchTermsForKeyword(KeywordID keyword_id);

  void DeleteKeywordSearchTermForURL(const GURL& url);

  void DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                    const std::u16string& term);

  // Clusters ------------------------------------------------------------------
  // See `HistoryService`'s comments for details on these functions.

  void AddContextAnnotationsForVisit(
      VisitID visit_id,
      const VisitContextAnnotations& visit_context_annotations);

  void SetOnCloseContextAnnotationsForVisit(
      VisitID visit_id,
      const VisitContextAnnotations& visit_context_annotations);

  std::vector<AnnotatedVisit> GetAnnotatedVisits(
      const QueryOptions& options,
      bool compute_redirect_chain_start_properties,
      bool get_unclustered_visits_only,
      bool* limited_by_max_count = nullptr);

  // Utility method to Construct `AnnotatedVisit`s.
  std::vector<AnnotatedVisit> ToAnnotatedVisitsFromRows(
      const VisitVector& visit_rows,
      bool compute_redirect_chain_start_properties) override;

  // Like above, but will first construct `visit_rows` from each `VisitID`
  // before delegating to the `ToAnnotatedVisitsFromRows()` above.
  std::vector<AnnotatedVisit> ToAnnotatedVisitsFromIds(
      const std::vector<VisitID>& visit_ids,
      bool compute_redirect_chain_start_properties);

  // Utility method to construct `ClusterVisit`s. Since `duplicate_visits` isn't
  // always useful and requires extra SQL executions, it's only populated if
  // `include_duplicates` is true.
  std::vector<ClusterVisit> ToClusterVisits(
      const std::vector<VisitID>& visit_ids,
      bool include_duplicates = true);

  // Utility method to construct `DuplicateClusterVisit`s.
  std::vector<DuplicateClusterVisit> ToDuplicateClusterVisits(
      const std::vector<VisitID>& visit_ids);

  // Returns the time of the most recent clustered visits; i.e. the boundary
  // until which visits have been clustered. It's possible for there to be
  // unclustered visits older than this boundary, since synced visits older than
  // this boundary won't be clustered nor cause re-clustering.
  base::Time FindMostRecentClusteredTime();

  void ReplaceClusters(const std::vector<int64_t>& ids_to_delete,
                       const std::vector<Cluster>& clusters_to_add);

  int64_t ReserveNextClusterIdWithVisit(const ClusterVisit& cluster_visit);

  void AddVisitsToCluster(int64_t cluster_id,
                          const std::vector<ClusterVisit>& visits);

  // Adds `cluster_visit` to the local cluster with `originator_cache_guid` and
  // `originator_cluster_id`. If an existing cluster does not exist with those
  // synced details, a new one will be created.
  void AddVisitToSyncedCluster(const history::ClusterVisit& cluster_visit,
                               const std::string& originator_cache_guid,
                               int64_t originator_cluster_id) override;

  void UpdateClusterTriggerability(const std::vector<Cluster>& clusters);

  // Use `UpdateVisitsInteractionState` instead to preserve the visits' scores.
  void HideVisits(const std::vector<VisitID>& visit_ids);

  void UpdateClusterVisit(const history::ClusterVisit& cluster_visit);

  void UpdateVisitsInteractionState(
      const std::vector<VisitID>& visit_ids,
      const ClusterVisit::InteractionState interaction_state);

  std::vector<Cluster> GetMostRecentClusters(
      base::Time inclusive_min_time,
      base::Time exclusive_max_time,
      size_t max_clusters,
      size_t max_visits_soft_cap,
      bool include_keywords_and_duplicates = true);

  // Get a `Cluster`. Since `keyword_to_data_map` and `visits.duplicate_visits`
  // aren't always useful and require extra SQL executions, they're only
  // populated if `include_keywords_and_duplicates` is true.
  Cluster GetCluster(int64_t cluster_id,
                     bool include_keywords_and_duplicates = true);

  // Returns the ID of the cluster containing `visit_id`. Returns 0 if
  // `visit_id` is not in a cluster.
  // HistoryBackendForSync:
  int64_t GetClusterIdContainingVisit(VisitID visit_id) override;

  // Finds the 1st visit in the redirect chain containing `visit`.
  // Unlike `GetRedirectsToSpecificVisit()`, this only considers actual
  // redirects, not referrals.
  // May return an invalid `VisitRow` if there's something wrong with the DB.
  // The caller is responsible for identifying, by looking at `visit_id`, and
  // handling this case.
  VisitRow GetRedirectChainStart(VisitRow visit);

  // Returns the redirect chain ending in `visit`. (If `visit` is in the middle
  // of a redirect chain, returns the start of the chain until `visit`.)
  // Unlike `GetRedirectsToSpecificVisit()`, this only considers actual
  // redirects, not referrals.
  // May return an empty vector if there's something wrong with the DB.
  VisitVector GetRedirectChain(VisitRow visit) override;

  // Observers -----------------------------------------------------------------

  void AddObserver(HistoryBackendObserver* observer) override;
  void RemoveObserver(HistoryBackendObserver* observer) override;

  // Generic operations --------------------------------------------------------

  // Sets the device information for all syncing devices.
  void SetSyncDeviceInfo(SyncDeviceInfoMap sync_device_info);

  // Sets the local device Originator Cache GUID.
  void SetLocalDeviceOriginatorCacheGuid(
      std::string local_device_originator_cache_guid);

  // Notifies the history backend that it should consider adding foreign
  // visits to local segments data.
  void SetCanAddForeignVisitsToSegments(bool add_foreign_visits);

  void ProcessDBTask(
      std::unique_ptr<HistoryDBTask> task,
      scoped_refptr<base::SequencedTaskRunner> origin_loop,
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled);

  // Run the `callback` with `this` and its database. Expected to be called from
  // the history thread. `callback` should handle the null database case.
  // Similar in purpose to above `ProcessDBTask` but this simply runs
  // immediately instead of possibly being queued. It's essentially just an
  // access mechanism.
  void RunDBTask(
      base::OnceCallback<void(HistoryBackend*, URLDatabase*)> callback);

  bool CanAddURL(const GURL& url) const override;

  bool GetAllTypedURLs(URLRows* urls);

  // TODO(manukh): It's confusing to have 5 methods for fetching URLs' visits
  //   and continuously adding more methods for each use case. Maybe we can
  //   satisfy all the callsites with just a few generic methods. E.g.:
  //   - GetMostRecentVisitsForUrlId(URLID id, int max_visits)
  //   - GetMostRecentVisitsForGurl(GURL url, int max_visits)
  //   - GetMostRecentVisitsForEachGurl(std::vector<GURL> urls, int max_visits)

  // TODO(manukh): DEPRECATED (see above comment)
  bool GetVisitsForURL(URLID id, VisitVector* visits);

  // TODO(manukh): Rename to `GetMostRecentVisitsForEachGurl`.
  std::map<GURL, VisitRow> GetMostRecentVisitForEachURL(
      const std::vector<GURL>& urls);

  // TODO(manukh): DEPRECATED (see above comment)
  bool GetMostRecentVisitForURL(URLID id, VisitRow* visit_row) override;

  // Fetches up to `max_visits` most recent visits for the passed URL.
  // TODO(manukh): Rename to `GetMostRecentVisitsForUrlId`.
  bool GetMostRecentVisitsForURL(URLID id, int max_visits, VisitVector* visits);

  QueryURLResult GetMostRecentVisitsForGurl(GURL url, int max_visits);

  // Searches for a visit with the given `originator_visit_id` coming from
  // another device (identified by `originator_cache_guid`). If found, returns
  // true and writes the visit into `visit_row`; otherwise returns false.
  bool GetForeignVisit(const std::string& originator_cache_guid,
                       VisitID originator_visit_id,
                       VisitRow* visit_row) override;

  // Adds a visit coming from another device. The visit's ID must be 0 (unset),
  // and its originator_cache_guid must be populated.
  VisitID AddSyncedVisit(
      const GURL& url,
      const std::u16string& title,
      bool hidden,
      const VisitRow& visit,
      const std::optional<VisitContextAnnotations>& context_annotations,
      const std::optional<VisitContentAnnotations>& content_annotations)
      override;

  // Updates a visit coming from another device (typically to update its
  // duration). The visit must be the end of a redirect chain (only chain ends
  // have the visit duration populated), and the visit's ID must be 0 (unset),
  // because for incoming remote visits, the local visit ID isn't know. The
  // visit will be identified via its timestamp and originator_cache_guid
  // instead. Returns the local VisitID of the updated visit, or 0 if no
  // matching visit was found.
  VisitID UpdateSyncedVisit(
      const GURL& url,
      const std::u16string& title,
      bool hidden,
      const VisitRow& visit,
      const std::optional<VisitContextAnnotations>& context_annotations,
      const std::optional<VisitContentAnnotations>& content_annotations)
      override;

  // Updates the `referring_visit` and `opener_visit` fields for the visit with
  // the given `visit_id`. Used by Sync to re-map originator IDs to local IDs.
  bool UpdateVisitReferrerOpenerIDs(VisitID visit_id,
                                    VisitID referrer_id,
                                    VisitID opener_id) override;

  // Starts the asynchronous process of deleting all foreign visits, i.e. those
  // with a non-empty `originator_cache_guid`. (This is called when History Sync
  // is disabled.)
  // Even though the process is async, only visits that already exist at the
  // time this is called will be deleted. Visits added afterwards will *not* be
  // deleted.
  // This method also resets the `is_known_to_sync` bit for all visits, local
  // and foreign.
  void DeleteAllForeignVisitsAndResetIsKnownToSync() override;

  // Removes `visits` from local state. `deletion_reason` specifies the reason
  // for why the removal action was initiated.
  bool RemoveVisits(const VisitVector& visits,
                    DeletionInfo::Reason deletion_reason);

  // Returns the `VisitSource` associated with each one of the passed visits.
  // If there is no entry in the map for a given visit, that means the visit
  // was `SOURCE_BROWSED`. Returns false if there is no `HistoryDatabase`.
  bool GetVisitsSource(const VisitVector& visits, VisitSourceMap* sources);

  // Like `GetVisitsSource`, but for a single visit.
  bool GetVisitSource(const VisitID visit_id, VisitSource* source);

  bool GetURL(const GURL& url, URLRow* url_row);

  bool GetURLByID(URLID url_id, URLRow* url_row) override;

  bool GetVisitByID(VisitID visit_id, VisitRow* visit_row) override;

  // Returns the visit matching a given timestamp. In case of redirects (where
  // multiple visits can have the same timestamp), returns the last visit in the
  // redirect chain.
  bool GetLastVisitByTime(base::Time visit_time, VisitRow* visit_row) override;

  // Returns the sync controller delegate for syncing history. The returned
  // delegate is owned by `this` object.
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetHistorySyncControllerDelegate();

  // Sends the SyncService's TransportState `state` to the HistorySyncBridge.
  void SetSyncTransportState(syncer::SyncService::TransportState state);

  // Deleting ------------------------------------------------------------------

  void DeleteURLs(const std::vector<GURL>& urls);

  void DeleteURL(const GURL& url);

  // Deletes all visits to urls until the corresponding timestamp.
  void DeleteURLsUntil(
      const std::vector<std::pair<GURL, base::Time>>& urls_and_timestamps);

  // Calls ExpireHistoryBackend::ExpireHistoryBetween and commits the change.
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            std::optional<std::string> restrict_app_id,
                            base::Time begin_time,
                            base::Time end_time,
                            bool user_initiated);

  // Finds the URLs visited at `times` and expires all their visits within
  // [`begin_time`, `end_time`). All times in `times` should be in
  // [`begin_time`, `end_time`). This is used when expiration request is from
  // server side, i.e. web history deletes, where only visit times (possibly
  // incomplete) are transmitted to protect user's privacy.
  void ExpireHistoryForTimes(const std::set<base::Time>& times,
                             base::Time begin_time,
                             base::Time end_time);

  // Calls ExpireHistoryBetween() once for each element in the vector.
  // The fields of `ExpireHistoryArgs` map directly to the arguments of
  // ExpireHistoryBetween().
  void ExpireHistory(const std::vector<ExpireHistoryArgs>& expire_list);

  // Expires all visits before and including the given time, updating the URLs
  // accordingly.
  void ExpireHistoryBeforeForTesting(base::Time end_time);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked. If there are no visits
  // for the specified url, it is deleted.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Callbacks To Kill Database When It Gets Corrupted -------------------------

  // Called by the database to report errors.  Schedules one call to
  // KillHistoryDatabase() in case of corruption.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Raze the history database. It will be recreated in a future run. Hopefully
  // things go better then. Continue running but without reading or storing any
  // state into the HistoryBackend databases. Close all of the databases managed
  // HistoryBackend as there are no provisions for accessing the other databases
  // managed by HistoryBackend when the history database cannot be accessed.
  void KillHistoryDatabase();

  // Testing -------------------------------------------------------------------

  // Sets the task to run and the message loop to run it on when this object
  // is destroyed. See HistoryService::SetOnBackendDestroyTask for a more
  // complete description.
  void SetOnBackendDestroyTask(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure task);

  // Adds the given rows to the database if it doesn't exist. A visit will be
  // added for each given URL at the last visit time in the URLRow if the
  // passed visit type != SOURCE_SYNCED (the sync code manages visits itself).
  // Each visit will have the visit_source type set.
  void AddPagesWithDetails(const URLRows& info, VisitSource visit_source);

#if defined(UNIT_TEST)
  HistoryDatabase* db() const { return db_.get(); }

  ExpireHistoryBackend* expire_backend() { return &expirer_; }
#endif

  // Returns true if the passed visit time is already expired (used by the sync
  // code to avoid syncing visits that would immediately be expired).
  bool IsExpiredVisitTime(const base::Time& time) const override;

  base::Time GetFirstRecordedTimeForTest() { return first_recorded_time_; }

  static int GetForeignVisitsToDeletePerBatchForTest();

  sql::Database& GetDBForTesting();

 protected:
  ~HistoryBackend() override;

 private:
  friend class base::RefCountedThreadSafe<HistoryBackend>;
  friend class TestHistoryBackend;
  friend class HistoryBackendTest;
  friend class HistoryBackendDBBaseTest;
  FRIEND_TEST_ALL_PREFIXES(OrderingHistoryServiceTest, EnsureCorrectOrder);

  // Returns the name of the Favicons database.
  base::FilePath GetFaviconsFileName() const;

  // Does the work of Init.
  void InitImpl(const HistoryDatabaseParams& history_database_params);

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Closes all databases managed by HistoryBackend. Commits any pending
  // transactions.
  void CloseAllDatabases();

  // Adds a single visit to the database, updating the URL information such
  // as visit and typed count. The visit ID of the added visit and the URL ID
  // of the associated URL (whether added or not) is returned. Both values will
  // be 0 on failure.
  //
  // If the caller wants to add this visit to the VisitedLinkDatabase, it needs
  // to provide values for the `top_level_url`, `frame_url`, `is_ephemeral`
  // parameters. `top_level_url` is a GURL representing the top-level frame that
  // this navigation originated from. `frame_url` is GURL representing the
  // immediate frame that this navigation originated from. For example, if a
  // link to `c.com` is clicked in an iframe `b.com` that is embedded in
  // `a.com`, the `top_level_url` is `a.com` and the `frame_url` is `b.com` (and
  // the `url` is `c.com`). `is_ephemeral` represents whether our navigation
  // came from a credentialless iframe (which is an ephemeral context). When
  // true, we want to avoid adding the visit into the VisitedLinkDatabase. This
  // does not schedule database commits, it is intended to be used as a
  // subroutine for AddPage only. It also assumes the database is valid.
  // Note that |app_is| is used for mobile only; |nullopt| on other platforms.
  std::pair<URLID, VisitID> AddPageVisit(
      const GURL& url,
      base::Time time,
      VisitID referring_visit,
      const GURL& external_referrer_url,
      ui::PageTransition transition,
      bool hidden,
      VisitSource visit_source,
      bool should_increment_typed_count,
      VisitID opener_visit,
      bool consider_for_ntp_most_visited,
      std::optional<int64_t> local_navigation_id = std::nullopt,
      std::optional<std::u16string> title = std::nullopt,
      std::optional<GURL> top_level_url = std::nullopt,
      std::optional<GURL> frame_url = std::nullopt,
      std::optional<std::string> app_id = std::nullopt,
      std::optional<base::TimeDelta> visit_duration = std::nullopt,
      std::optional<std::string> originator_cache_guid = std::nullopt,
      std::optional<VisitID> originator_visit_id = std::nullopt,
      std::optional<VisitID> originator_referring_visit = std::nullopt,
      std::optional<VisitID> originator_opener_visit = std::nullopt,
      bool is_known_to_sync = false,
      bool is_ephemeral = false);

  // Returns a redirect-or-referral chain in `redirects` for the VisitID
  // `cur_visit`. `cur_visit` is assumed to be valid. Assumes that
  // this HistoryBackend object has been Init()ed successfully.
  void GetRedirectsFromSpecificVisit(VisitID cur_visit,
                                     RedirectList* redirects);

  // Similar to the above function except returns a redirect-or-referral list
  // ending at `cur_visit`. E.g. if visit A redirected to visit B, which
  // referred to visit C, then the return list for visit E would include all 3
  // visits.
  void GetRedirectsToSpecificVisit(VisitID cur_visit, RedirectList* redirects);

  // Updates the visit_duration information in visits table.
  void UpdateVisitDuration(VisitID visit_id, const base::Time end_ts);

  // Flags this visit's `is_known_to_sync` true.
  void MarkVisitAsKnownToSync(VisitID visit_id) override;

  // Returns whether `url` is on an untyped intranet host.
  bool IsUntypedIntranetHost(const GURL& url);

  // Querying ------------------------------------------------------------------

  // Backends for QueryHistory. *Basic() handles queries that are not
  // text search queries and can just be given directly to the history DB.
  // The *Text() version performs a brute force query of the history DB to
  // search for results which match the given text query.
  // Both functions assume QueryHistory already checked the DB for validity.
  void QueryHistoryBasic(const QueryOptions& options, QueryResults* result);
  void QueryHistoryText(const std::u16string& text_query,
                        const QueryOptions& options,
                        QueryResults* result);

  // Performs a brute force search over the database to find any host names that
  // match the `host_name` string. Returns any matches.
  URLRows GetMatchesForHost(const std::u16string& host_name);

  // Committing ----------------------------------------------------------------

  // We always keep a transaction open on the history database so that multiple
  // transactions can be batched. Periodically, these are flushed (use
  // ScheduleCommit). This function does the commit to write any new changes to
  // disk and opens a new transaction. This will be called automatically by
  // ScheduleCommit, or it can be called explicitly if a caller really wants
  // to write something to disk.
  void Commit();

  // Schedules a commit to happen in the future. We do this so that many
  // operations over a period of time will be batched together. If there is
  // already a commit scheduled for the future, this will do nothing.
  void ScheduleCommit();

  // Cancels the scheduled commit, if any. If there is no scheduled commit,
  // does nothing.
  void CancelScheduledCommit();

  // Begins the singleton transaction and checks all invariants. Caller MUST
  // make sure `singleton_transaction_` is nullptr beforehand. If this succeeds,
  // `singleton_transaction_` will be defined after this call.
  void BeginSingletonTransaction();

  // If the singleton transaction exists, commits it and checks all invariants.
  // Does nothing if `singleton_transaction_` is nullptr. Caller is responsible
  // for starting a new singleton transaction.
  void CommitSingletonTransactionIfItExists();

  // Segments ------------------------------------------------------------------

  // Walks back a segment chain to find the last visit with a non null segment
  // id and returns it. If there is none found, returns 0.
  SegmentID GetLastSegmentID(VisitID from_visit);

  // Assign segment information for a new visit. This is called internally when
  // a page is added. Return the segment id of the segment that has been
  // assigned to `visit_id`.
  SegmentID AssignSegmentForNewVisit(const GURL& url,
                                     VisitID from_visit,
                                     VisitID visit_id,
                                     ui::PageTransition transition_type,
                                     const base::Time ts);

  // Calculates the segment ID given a URL, visit ID, and page transition
  // type(s). If necessary, this method will create a new segment and return its
  // ID. Returns 0 if no segment ID can be calculated, or a new segment cannot
  // be created.
  SegmentID CalculateSegmentID(const GURL& url,
                               VisitID from_visit,
                               ui::PageTransition transition_type);

  // Detects if `visit_row`'s segment has changed. If so, updates
  // `visit_row`'s `segment_id`, and ensures segment visits are not double
  // counted across the existing and new segments.
  void UpdateSegmentForExistingForeignVisit(VisitRow& visit_row);

  // Favicons ------------------------------------------------------------------

  // Returns all the page URLs in the redirect chain for `page_url`. If there
  // are no known redirects for `page_url`, returns a vector with `page_url`.
  RedirectList GetCachedRecentRedirects(const GURL& page_url);

  // Send notification that the favicon has changed for `page_url` and all its
  // redirects. This should be called if the mapping between the page URL
  // (e.g. http://www.google.com) and the icon URL (e.g.
  // http://www.google.com/favicon.ico) has changed.
  void SendFaviconChangedNotificationForPageAndRedirects(const GURL& page_url);

  // Send notification that the bitmap data for the favicon at `icon_url` has
  // changed. Sending this notification is important because the favicon at
  // `icon_url` may be mapped to hundreds of page URLs.
  void SendFaviconChangedNotificationForIconURL(const GURL& icon_url);

  // Generic stuff -------------------------------------------------------------

  // Processes the next scheduled HistoryDBTask, scheduling this method
  // to be invoked again if there are more tasks that need to run.
  void ProcessDBTaskImpl();

  // HistoryBackendNotifier:
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override;
  void NotifyURLVisited(const URLRow& url_row,
                        const VisitRow& visit_row,
                        std::optional<int64_t> local_navigation_id) override;
  void NotifyURLsModified(const URLRows& changed_urls,
                          bool is_from_expiration) override;
  void NotifyDeletions(DeletionInfo deletion_info) override;
  void NotifyVisitUpdated(const VisitRow& visit,
                          VisitUpdateReason reason) override;
  void NotifyVisitsDeleted(const std::vector<DeletedVisit>& visits) override;

  // Deleting all history ------------------------------------------------------

  // Deletes all history. This is a special case of deleting that is separated
  // from our normal dependency-following method for performance reasons. The
  // logic lives here instead of ExpireHistoryBackend since it will cause
  // re-initialization of some databases (e.g. favicons) that could fail.
  // When these databases are not valid, our pointers must be null, so we need
  // to handle this type of operation to keep the pointers in sync.
  void DeleteAllHistory();

  // Given a vector of all URLs that we will keep, removes all favicons that
  // aren't used by those URLs.
  bool ClearAllFaviconHistory(const std::vector<GURL>& kept_urls);

  // Deletes all information in the history database, except for the supplied
  // set of URLs in the URL table (these should correspond to the bookmarked
  // URLs).
  //
  // The IDs of the URLs may change.
  bool ClearAllMainHistory(const URLRows& kept_urls);

  // Deletes the FTS index database files, which are no longer used.
  void DeleteFTSIndexDatabases();

  // favicon::FaviconBackendDelegate
  std::vector<GURL> GetCachedRecentRedirectsForPage(
      const GURL& page_url) override;

  bool ProcessSetFaviconsResult(const favicon::SetFaviconsResult& result,
                                const GURL& icon_url);

  // Implementation of DeleteAllForeignVisits(): Since there may be many (1000s)
  // of foreign visits, the deletion is implemented in multiple small batches to
  // keep the memory overhead manageable. This method schedules a HistoryDBTask
  // that will perform the deletion in multiple steps.
  void StartDeletingForeignVisits();

  // Data ----------------------------------------------------------------------

  // Delegate. See the class definition above for more information. This will
  // be null before Init is called and after Cleanup, but is guaranteed
  // non-null in between.
  std::unique_ptr<Delegate> delegate_;

  // Directory where database files will be stored, empty until Init is called.
  base::FilePath history_dir_;

  // The history/favicon databases. Either may be null if the database could
  // not be opened, all users must first check for null and return immediately
  // if it is. The favicon DB may be null when the history one isn't, but not
  // vice-versa.
  std::unique_ptr<HistoryDatabase> db_;

  // The singleton long-running transaction used to batch together History for
  // optimization purposes. There can only ever be one, because transaction
  // nesting doesn't actually exist, and leads to unexpected bugs. This is
  // nullptr if the transaction didn't successfully begin.
  std::unique_ptr<sql::Transaction> singleton_transaction_;

  bool scheduled_kill_db_ = false;  // Database is being killed due to error.
  std::unique_ptr<favicon::FaviconBackend> favicon_backend_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // !IsCancelled() to see if there is a commit scheduled in the future (note
  // that CancelableOnceClosure starts cancelled with the default constructor),
  // and we can use Cancel() to cancel the scheduled commit. There can be only
  // one scheduled commit at a time (see ScheduleCommit).
  base::CancelableOnceClosure scheduled_commit_;

  // Maps recent redirect destination pages to the chain of redirects that
  // brought us to there. Pages that did not have redirects or were not the
  // final redirect in a chain will not be in this list, as well as pages that
  // redirected "too long" ago (as determined by ExpireOldRedirects above).
  // It is used to set titles & favicons for redirects to that of the
  // destination.
  //
  // As with AddPage, the last item in the redirect chain will be the
  // destination of the redirect (i.e., the key into recent_redirects_);
  typedef base::LRUCache<GURL, RedirectList> RedirectCache;
  RedirectCache recent_redirects_;

  // Timestamp of the first entry in our database.
  base::Time first_recorded_time_;

  // When set, this is the task that should be invoked on destruction.
  scoped_refptr<base::SingleThreadTaskRunner> backend_destroy_task_runner_;
  base::OnceClosure backend_destroy_task_;

  // Tracks page transition types.
  VisitTracker tracker_;

  // List of QueuedHistoryDBTasks to run.
  std::list<std::unique_ptr<QueuedHistoryDBTask>> queued_history_db_tasks_;
  // A single task, taken out of the above list, that has already been posted to
  // the `task_runner_`. Stored so that it can be canceled at shutdown.
  base::CancelableOnceClosure posted_history_db_task_;

  // Used to determine if a URL is bookmarked; may be null.
  std::unique_ptr<HistoryBackendClient> backend_client_;

  // Manages expiration between the various databases.
  ExpireHistoryBackend expirer_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Contains diagnostic information about the sql database that is non-empty
  // when a catastrophic error occurs.
  std::string diagnostics_string_;
  sql::DatabaseDiagnostics diagnostics_;

  // List of observers
  base::ObserverList<HistoryBackendObserver>::Unchecked observers_;

  // Used to manage syncing of the history datatype. It will be null before
  // HistoryBackend::Init() is called. Defined after `observers_` because
  // it unregisters itself as observer during destruction.
  std::unique_ptr<HistorySyncBridge> history_sync_bridge_;

  // Contains device information for all syncing devices.
  SyncDeviceInfoMap sync_device_info_;

  // Contains the local device Originator Cache GUID, a unique, sync-specific
  // identifier for the local device.
  std::string local_device_originator_cache_guid_;

  // Whether segments data should include foreign history.
  bool can_add_foreign_visits_to_segments_ = false;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_
