// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/expire_history_backend.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/thumbnail_database.h"
#include "components/history/core/browser/visit_tracker.h"
#include "sql/init_status.h"

class SkBitmap;
class TestingProfile;

namespace base {
class SingleThreadTaskRunner;
}

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace history {
struct DownloadRow;
class HistoryBackendClient;
class HistoryBackendDBBaseTest;
class HistoryBackendObserver;
class HistoryBackend;
class HistoryBackendTest;
class HistoryDatabase;
struct HistoryDatabaseParams;
class HistoryDBTask;
class InMemoryHistoryBackend;
class HistoryBackendHelper;
class TypedURLSyncBridge;
class URLDatabase;

// The maximum number of bitmaps for a single icon URL which can be stored in
// the thumbnail database.
static const size_t kMaxFaviconBitmapsPerIconURL = 8;

// Returns a formatted version of |url| with the HTTP/HTTPS scheme, port,
// username/password, and any trivial subdomains (e.g., "www.", "m.") removed.
base::string16 FormatUrlForRedirectComparison(const GURL& url);

// Keeps track of a queued HistoryDBTask. This class lives solely on the
// DB thread.
class QueuedHistoryDBTask {
 public:
  QueuedHistoryDBTask(
      std::unique_ptr<HistoryDBTask> task,
      scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled);
  ~QueuedHistoryDBTask();

  bool is_canceled();
  bool Run(HistoryBackend* backend, HistoryDatabase* db);
  void DoneRun();

 private:
  std::unique_ptr<HistoryDBTask> task_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_loop_;
  base::CancelableTaskTracker::IsCanceledCallback is_canceled_;

  DISALLOW_COPY_AND_ASSIGN(QueuedHistoryDBTask);
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
                       public HistoryBackendNotifier {
 public:
  // Interface implemented by the owner of the HistoryBackend object. Normally,
  // the history service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a history service object.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the database cannot be read correctly for some reason.
    // |diagnostics| contains information about the underlying database
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
    // with non-empty |page_urls| and an empty |icon_url| and vice versa.
    virtual void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                       const GURL& icon_url) = 0;

    // Notify HistoryService that the user is visiting an URL. The event will
    // be forwarded to the HistoryServiceObservers in the correct thread.
    virtual void NotifyURLVisited(ui::PageTransition transition,
                                  const URLRow& row,
                                  const RedirectList& redirects,
                                  base::Time visit_time) = 0;

    // Notify HistoryService that some URLs have been modified. The event will
    // be forwarded to the HistoryServiceObservers in the correct thread.
    virtual void NotifyURLsModified(const URLRows& changed_urls) = 0;

    // Notify HistoryService that some or all of the URLs have been deleted.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyURLsDeleted(DeletionInfo deletion_info) = 0;

    // Notify HistoryService that some keyword has been searched using omnibox.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                                KeywordID keyword_id,
                                                const base::string16& term) = 0;

    // Notify HistoryService that keyword search term has been deleted.
    // The event will be forwarded to the HistoryServiceObservers in the correct
    // thread.
    virtual void NotifyKeywordSearchTermDeleted(URLID url_id) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  // Check if the transition should increment the typed_count of a visit.
  static bool IsTypedIncrement(ui::PageTransition transition);

  // Init must be called to complete object creation. This object can be
  // constructed on any thread, but all other functions including Init() must
  // be called on the history thread.
  //
  // |history_dir| is the directory where the history files will be placed.
  // See the definition of BroadcastNotificationsCallback above. This function
  // takes ownership of the callback pointer.
  //
  // |history_client| is used to determine bookmarked URLs when deleting and
  // may be null.
  //
  // This constructor is fast and does no I/O, so can be called at any time.
  HistoryBackend(std::unique_ptr<Delegate> delegate,
                 std::unique_ptr<HistoryBackendClient> backend_client,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // |force_fail| can be set during unittests to unconditionally fail to init.
  void Init(bool force_fail,
            const HistoryDatabaseParams& history_database_params);

  // Notification that the history system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

#if defined(OS_IOS)
  // Persists any in-flight state, without actually shutting down the history
  // system. This is intended for use when the application is backgrounded.
  void PersistState();
#endif

  void ClearCachedDataForContextID(ContextID context_id);

  // Clears all on-demand favicons from thumbnail database.
  void ClearAllOnDemandFavicons();

  // Gets the counts and last last time of URLs that belong to |origins| in the
  // history database. Origins that are not in the history database will be in
  // the map with a count and time of 0.
  // Returns an empty map if db_ is not initialized.
  OriginCountAndLastVisitMap GetCountsAndLastVisitForOrigins(
      const std::set<GURL>& origins) const;

  // Navigation ----------------------------------------------------------------

  // |request.time| must be unique with high probability.
  void AddPage(const HistoryAddPageArgs& request);
  virtual void SetPageTitle(const GURL& url, const base::string16& title);
  void AddPageNoVisitForBookmark(const GURL& url, const base::string16& title);
  void UpdateWithPageEndTime(ContextID context_id,
                             int nav_entry_id,
                             const GURL& url,
                             base::Time end_ts);

  // Querying ------------------------------------------------------------------

  // Run the |callback| on the History thread.
  // |callback| should handle the null database case.
  void ScheduleAutocomplete(
      base::OnceCallback<void(HistoryBackend*, URLDatabase*)> callback);

  QueryURLResult QueryURL(const GURL& url, bool want_visits);
  QueryResults QueryHistory(const base::string16& text_query,
                            const QueryOptions& options);

  // Computes the most recent URL(s) that the given canonical URL has
  // redirected to. There may be more than one redirect in a row, so this
  // function will fill the given array with the entire chain. If there are
  // no redirects for the most recent visit of the URL, or the URL is not
  // in history, the array will be empty.
  RedirectList QueryRedirectsFrom(const GURL& url);

  // Similar to above function except computes a chain of redirects to the
  // given URL. Stores the most recent list of redirects ending at |url| in the
  // given RedirectList. For example, if we have the redirect list A -> B -> C,
  // then calling this function with url=C would fill redirects with {B, A}.
  RedirectList QueryRedirectsTo(const GURL& url);

  VisibleVisitCountToHostResult GetVisibleVisitCountToHost(const GURL& url);

  // Request the |result_count| most visited URLs and the chain of
  // redirects leading to each of these URLs. |days_back| is the
  // number of days of history to use. Used by TopSites.
  MostVisitedURLList QueryMostVisitedURLs(int result_count, int days_back);

  // Statistics ----------------------------------------------------------------

  // Gets the number of URLs as seen in chrome://history within the time range
  // [|begin_time|, |end_time|). Each URL is counted only once per day. For
  // determination of the date, timestamps are converted to dates using local
  // time.
  HistoryCountResult GetHistoryCount(const base::Time& begin_time,
                                     const base::Time& end_time);

  // Returns the number of hosts visited in the last month.
  HistoryCountResult CountUniqueHostsVisitedLastMonth();

  // Gets the last time any webpage on the given host was visited within the
  // time range [|begin_time|, |end_time|). If the given host has not been
  // visited in the given time range, the result will have a null base::Time,
  // but still report success.
  HistoryLastVisitToHostResult GetLastVisitToHost(const GURL& host,
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

  // |page_urls| must not be empty.
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
                                   const base::string16& term);

  void DeleteAllSearchTermsForKeyword(KeywordID keyword_id);

  void DeleteKeywordSearchTermForURL(const GURL& url);

  void DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                    const base::string16& term);

  // Observers -----------------------------------------------------------------

  void AddObserver(HistoryBackendObserver* observer);
  void RemoveObserver(HistoryBackendObserver* observer);

  // Generic operations --------------------------------------------------------

  void ProcessDBTask(
      std::unique_ptr<HistoryDBTask> task,
      scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled);

  virtual bool GetAllTypedURLs(URLRows* urls);

  virtual bool GetVisitsForURL(URLID id, VisitVector* visits);

  // Fetches up to |max_visits| most recent visits for the passed URL.
  virtual bool GetMostRecentVisitsForURL(URLID id,
                                         int max_visits,
                                         VisitVector* visits);

  // For each element in |urls|, updates the pre-existing URLRow in the database
  // with the same ID; or ignores the element if no such row exists. Returns the
  // number of records successfully updated.
  virtual size_t UpdateURLs(const URLRows& urls);

  // While adding visits in batch, the source needs to be provided.
  virtual bool AddVisits(const GURL& url,
                         const std::vector<VisitInfo>& visits,
                         VisitSource visit_source);

  virtual bool RemoveVisits(const VisitVector& visits);

  // Returns the VisitSource associated with each one of the passed visits.
  // If there is no entry in the map for a given visit, that means the visit
  // was SOURCE_BROWSED. Returns false if there is no HistoryDatabase..
  bool GetVisitsSource(const VisitVector& visits, VisitSourceMap* sources);

  virtual bool GetURL(const GURL& url, URLRow* url_row);

  bool GetURLByID(URLID url_id, URLRow* url_row);

  // Returns the sync controller delegate for syncing typed urls. The returned
  // delegate is owned by |this| object.
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetTypedURLSyncControllerDelegate();

  // Deleting ------------------------------------------------------------------

  void DeleteURLs(const std::vector<GURL>& urls);

  void DeleteURL(const GURL& url);

  // Deletes all visits to urls until the corresponding timestamp.
  void DeleteURLsUntil(
      const std::vector<std::pair<GURL, base::Time>>& urls_and_timestamps);

  // Calls ExpireHistoryBackend::ExpireHistoryBetween and commits the change.
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time,
                            base::Time end_time,
                            bool user_initiated);

  // Finds the URLs visited at |times| and expires all their visits within
  // [|begin_time|, |end_time|). All times in |times| should be in
  // [|begin_time|, |end_time|). This is used when expiration request is from
  // server side, i.e. web history deletes, where only visit times (possibly
  // incomplete) are transmitted to protect user's privacy.
  void ExpireHistoryForTimes(const std::set<base::Time>& times,
                             base::Time begin_time,
                             base::Time end_time);

  // Calls ExpireHistoryBetween() once for each element in the vector.
  // The fields of |ExpireHistoryArgs| map directly to the arguments of
  // of ExpireHistoryBetween().
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

  // SupportsUserData ----------------------------------------------------------

  // The user data allows the clients to associate data with this object.
  // Multiple user data values can be stored under different keys.
  base::SupportsUserData::Data* GetUserData(const void* key) const;
  void SetUserData(const void* key,
                   std::unique_ptr<base::SupportsUserData::Data> data);

  // Testing -------------------------------------------------------------------

  // Sets the task to run and the message loop to run it on when this object
  // is destroyed. See HistoryService::SetOnBackendDestroyTask for a more
  // complete description.
  void SetOnBackendDestroyTask(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Closure& task);

  // Adds the given rows to the database if it doesn't exist. A visit will be
  // added for each given URL at the last visit time in the URLRow if the
  // passed visit type != SOURCE_SYNCED (the sync code manages visits itself).
  // Each visit will have the visit_source type set.
  void AddPagesWithDetails(const URLRows& info, VisitSource visit_source);

#if defined(UNIT_TEST)
  HistoryDatabase* db() const { return db_.get(); }

  ExpireHistoryBackend* expire_backend() { return &expirer_; }
#endif

  void SetTypedURLSyncBridgeForTest(std::unique_ptr<TypedURLSyncBridge> bridge);

  // Returns true if the passed visit time is already expired (used by the sync
  // code to avoid syncing visits that would immediately be expired).
  virtual bool IsExpiredVisitTime(const base::Time& time);

  base::Time GetFirstRecordedTimeForTest() { return first_recorded_time_; }

 protected:
  ~HistoryBackend() override;

 private:
  friend class base::RefCountedThreadSafe<HistoryBackend>;
  friend class HistoryBackendTest;
  friend class HistoryBackendDBBaseTest;  // So the unit tests can poke our
                                          // innards.
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAll);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAllURLPreviouslyDeleted);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAllThenAddData);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPagesWithDetails);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, UpdateURLs);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, ImportedFaviconsTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, URLsNoLongerBookmarked);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, StripUsernamePasswordTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteThumbnailsDatabaseTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitBackForward);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitRedirectBackForward);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitNotLastVisit);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           AddPageVisitFiresNotificationWithCorrectDetails);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageArgsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetMostRecentVisits);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsTransitions);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageAndRedirects);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageAndRedirectsWithFragment);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           RecentRedirectsForClientRedirects);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageDuplicates);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFaviconsDeleteBitmaps);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFaviconsReplaceBitmapData);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconsSameFaviconURLForTwoPages);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFaviconsWithTwoPageURLs);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetOnDemandFaviconsForEmptyDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetOnDemandFaviconsForPageInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetOnDemandFaviconsForIconInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchNoChange);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconPageURLNotInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconPageURLInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconMaxFaviconsPerPage);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           MergeFaviconIconURLMappedToDifferentPageURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           MergeFaviconMaxFaviconBitmapsPerIconURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           MergeIdenticalFaviconDoesNotChangeLastUpdatedTime);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           FaviconChangedNotificationNewFavicon);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           FaviconChangedNotificationBitmapDataChanged);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           FaviconChangedNotificationIconMappingChanged);
  FRIEND_TEST_ALL_PREFIXES(
      HistoryBackendTest,
      FaviconChangedNotificationIconMappingAndBitmapDataChanged);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           FaviconChangedNotificationsMergeCopy);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, NoFaviconChangedNotifications);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchMultipleIconTypes);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, CloneFaviconMappingsForPages);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBEmpty);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           GetFaviconsFromDBNoFaviconBitmaps);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           GetFaviconsFromDBSelectClosestMatch);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBIconType);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBExpired);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBFallbackToHost);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchNoDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, QueryFilteredURLs);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts_ElidePortAndScheme);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts_ElideWWW);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts_OnlyLast30Days);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts_MaxNumHosts);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, TopHosts_IgnoreUnusualURLs);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetCountsAndLastVisitForOrigins);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, UpdateVisitDuration);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, ExpireHistoryForTimes);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteFTSIndexDatabases);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTypedUrlTest,
                           ProcessUserChangeRemove);
  friend class ::TestingProfile;

  // Returns the name of the Favicons database. This is the new name
  // of the Thumbnails database.
  base::FilePath GetFaviconsFileName() const;

  class URLQuerier;
  friend class URLQuerier;

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
  // This does not schedule database commits, it is intended to be used as a
  // subroutine for AddPage only. It also assumes the database is valid.
  std::pair<URLID, VisitID> AddPageVisit(
      const GURL& url,
      base::Time time,
      VisitID referring_visit,
      ui::PageTransition transition,
      bool hidden,
      VisitSource visit_source,
      bool should_increment_typed_count,
      base::Optional<base::string16> title = base::nullopt);

  // Returns a redirect chain in |redirects| for the VisitID
  // |cur_visit|. |cur_visit| is assumed to be valid. Assumes that
  // this HistoryBackend object has been Init()ed successfully.
  void GetRedirectsFromSpecificVisit(VisitID cur_visit,
                                     RedirectList* redirects);

  // Similar to the above function except returns a redirect list ending
  // at |cur_visit|.
  void GetRedirectsToSpecificVisit(VisitID cur_visit, RedirectList* redirects);

  // Updates the visit_duration information in visits table.
  void UpdateVisitDuration(VisitID visit_id, const base::Time end_ts);

  // Returns whether |url| is on an untyped intranet host.
  bool IsUntypedIntranetHost(const GURL& url);

  // Querying ------------------------------------------------------------------

  // Backends for QueryHistory. *Basic() handles queries that are not
  // text search queries and can just be given directly to the history DB.
  // The *Text() version performs a brute force query of the history DB to
  // search for results which match the given text query.
  // Both functions assume QueryHistory already checked the DB for validity.
  void QueryHistoryBasic(const QueryOptions& options, QueryResults* result);
  void QueryHistoryText(const base::string16& text_query,
                        const QueryOptions& options,
                        QueryResults* result);

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

  // Segments ------------------------------------------------------------------

  // Walks back a segment chain to find the last visit with a non null segment
  // id and returns it. If there is none found, returns 0.
  SegmentID GetLastSegmentID(VisitID from_visit);

  // Update the segment information. This is called internally when a page is
  // added. Return the segment id of the segment that has been updated.
  SegmentID UpdateSegments(const GURL& url,
                           VisitID from_visit,
                           VisitID visit_id,
                           ui::PageTransition transition_type,
                           const base::Time ts);

  // Favicons ------------------------------------------------------------------

  // If |bitmaps_are_expired| is true, the icon for |icon_url| will be modified
  // only if it's not present in the database. In that case, it will be
  // initially set as expired. Returns whether the new bitmaps were actually
  // written. |page_urls| must not be empty.
  bool SetFaviconsImpl(const base::flat_set<GURL>& page_urls,
                       favicon_base::IconType icon_type,
                       const GURL& icon_url,
                       const std::vector<SkBitmap>& bitmaps,
                       FaviconBitmapType type);

  // Used by both UpdateFaviconMappingsAndFetch() and GetFavicon().
  // If there is a favicon stored in the database for |icon_url|, a mapping is
  // added to the database from each element in |page_urls| (and all redirects)
  // to |icon_url|.
  void UpdateFaviconMappingsAndFetchImpl(
      const base::flat_set<GURL>& page_urls,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>* results);

  // Set the favicon bitmaps of |type| for |icon_id|.
  // For each entry in |bitmaps|, if a favicon bitmap already exists at the
  // entry's pixel size, replace the favicon bitmap's data with the entry's
  // bitmap data. Otherwise add a new favicon bitmap.
  // Any favicon bitmaps already mapped to |icon_id| whose pixel size does not
  // match the pixel size of one of |bitmaps| is deleted.
  // For bitmap type FaviconBitmapType::ON_DEMAND, this is legal to call only
  // for a newly created |icon_id| (that has no bitmaps yet).
  // Returns true if any of the bitmap data at |icon_id| is changed as a result
  // of calling this method.
  bool SetFaviconBitmaps(favicon_base::FaviconID icon_id,
                         const std::vector<SkBitmap>& bitmaps,
                         FaviconBitmapType type);

  // Returns true if the bitmap data at |bitmap_id| equals |new_bitmap_data|.
  bool IsFaviconBitmapDataEqual(
      FaviconBitmapID bitmap_id,
      const scoped_refptr<base::RefCountedMemory>& new_bitmap_data);

  // Returns true if there are favicons for |page_url| and one of the types in
  // |icon_types|.
  // |favicon_bitmap_results| is set to the favicon bitmaps whose edge sizes
  // most closely match |desired_sizes|. If |desired_sizes| has a '0' entry, the
  // largest favicon bitmap with one of the icon types in |icon_types| is
  // returned. If |icon_types| contains multiple icon types and there are
  // several matched icon types in the database, results will only be returned
  // for a single icon type in the priority of kTouchPrecomposedIcon,
  // kTouchIcon, and kFavicon. If |fallback_to_host| is true, the host of
  // |page_url| will be used to search the favicon database if an exact match
  // cannot be found. See the comment for GetFaviconResultsForBestMatch() for
  // more details on how |favicon_bitmap_results| is constructed.
  bool GetFaviconsFromDB(const GURL& page_url,
                         const favicon_base::IconTypeSet& icon_types,
                         const std::vector<int>& desired_sizes,
                         bool fallback_to_host,
                         std::vector<favicon_base::FaviconRawBitmapResult>*
                             favicon_bitmap_results);

  // Returns the favicon bitmaps whose edge sizes most closely match
  // |desired_sizes| in |favicon_bitmap_results|. If |desired_sizes| has a '0'
  // entry, only the largest favicon bitmap is returned. Goodness is computed
  // via SelectFaviconFrameIndices(). It is computed on a per FaviconID basis,
  // thus all |favicon_bitmap_results| are guaranteed to be for the same
  // FaviconID. |favicon_bitmap_results| will have at most one entry for each
  // desired edge size. There will be fewer entries if the same favicon bitmap
  // is the best result for multiple edge sizes.
  // Returns true if there were no errors.
  bool GetFaviconBitmapResultsForBestMatch(
      const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>*
          favicon_bitmap_results);

  // Maps the favicon ID |icon_id| to |page_url| (and all redirects) for
  // |icon_type|. |icon_id| == 0 deletes previously existing mappings.
  // Returns true if the mappings for the page or any of its redirects were
  // changed.
  bool SetFaviconMappingsForPageAndRedirects(const GURL& page_url,
                                             favicon_base::IconType icon_type,
                                             favicon_base::FaviconID icon_id);

  // Maps the favicon ID |icon_id| to URLs in |page_urls| for |icon_type|.
  // |icon_id| == 0 deletes previously existing mappings.
  // Returns page URLs among |page_urls| whose mappings were changed (might be
  // empty).
  std::vector<GURL> SetFaviconMappingsForPages(
      const base::flat_set<GURL>& page_urls,
      favicon_base::IconType icon_type,
      favicon_base::FaviconID icon_id);

  // Maps the favicon ID |icon_ids| to |page_url| for |icon_type|.
  // |icon_id| == 0 deletes previously existing mappings.
  // Returns true if the function changed at least one of |page_url|'s mappings.
  bool SetFaviconMappingsForPage(const GURL& page_url,
                                 favicon_base::IconType icon_type,
                                 favicon_base::FaviconID icon_id);

  // Returns all the page URLs in the redirect chain for |page_url|. If there
  // are no known redirects for |page_url|, returns a vector with |page_url|.
  RedirectList GetCachedRecentRedirects(const GURL& page_url);

  // Send notification that the favicon has changed for |page_url| and all its
  // redirects. This should be called if the mapping between the page URL
  // (e.g. http://www.google.com) and the icon URL (e.g.
  // http://www.google.com/favicon.ico) has changed.
  void SendFaviconChangedNotificationForPageAndRedirects(const GURL& page_url);

  // Send notification that the bitmap data for the favicon at |icon_url| has
  // changed. Sending this notification is important because the favicon at
  // |icon_url| may be mapped to hundreds of page URLs.
  void SendFaviconChangedNotificationForIconURL(const GURL& icon_url);

  // Generic stuff -------------------------------------------------------------

  // Processes the next scheduled HistoryDBTask, scheduling this method
  // to be invoked again if there are more tasks that need to run.
  void ProcessDBTaskImpl();

  // HistoryBackendNotifier:
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override;
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override;
  void NotifyURLsModified(const URLRows& changed_urls,
                          bool is_from_expiration) override;
  void NotifyURLsDeleted(DeletionInfo deletion_info) override;

  // Deleting all history ------------------------------------------------------

  // Deletes all history. This is a special case of deleting that is separated
  // from our normal dependency-following method for performance reasons. The
  // logic lives here instead of ExpireHistoryBackend since it will cause
  // re-initialization of some databases (e.g. Thumbnails) that could fail.
  // When these databases are not valid, our pointers must be null, so we need
  // to handle this type of operation to keep the pointers in sync.
  void DeleteAllHistory();

  // Given a vector of all URLs that we will keep, removes all thumbnails
  // referenced by any URL, and also all favicons that aren't used by those
  // URLs.
  bool ClearAllThumbnailHistory(const std::vector<GURL>& kept_urls);

  // Deletes all information in the history database, except for the supplied
  // set of URLs in the URL table (these should correspond to the bookmarked
  // URLs).
  //
  // The IDs of the URLs may change.
  bool ClearAllMainHistory(const URLRows& kept_urls);

  // Deletes the FTS index database files, which are no longer used.
  void DeleteFTSIndexDatabases();

  // Data ----------------------------------------------------------------------

  // Delegate. See the class definition above for more information. This will
  // be null before Init is called and after Cleanup, but is guaranteed
  // non-null in between.
  std::unique_ptr<Delegate> delegate_;

  // Directory where database files will be stored, empty until Init is called.
  base::FilePath history_dir_;

  // The history/thumbnail databases. Either may be null if the database could
  // not be opened, all users must first check for null and return immediately
  // if it is. The thumbnail DB may be null when the history one isn't, but not
  // vice-versa.
  std::unique_ptr<HistoryDatabase> db_;
  bool scheduled_kill_db_;  // Database is being killed due to error.
  std::unique_ptr<ThumbnailDatabase> thumbnail_db_;

  // Manages expiration between the various databases.
  ExpireHistoryBackend expirer_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // !IsCancelled() to see if there is a commit scheduled in the future (note
  // that CancelableClosure starts cancelled with the default constructor), and
  // we can use Cancel() to cancel the scheduled commit. There can be only one
  // scheduled commit at a time (see ScheduleCommit).
  base::CancelableClosure scheduled_commit_;

  // Maps recent redirect destination pages to the chain of redirects that
  // brought us to there. Pages that did not have redirects or were not the
  // final redirect in a chain will not be in this list, as well as pages that
  // redirected "too long" ago (as determined by ExpireOldRedirects above).
  // It is used to set titles & favicons for redirects to that of the
  // destination.
  //
  // As with AddPage, the last item in the redirect chain will be the
  // destination of the redirect (i.e., the key into recent_redirects_);
  typedef base::MRUCache<GURL, RedirectList> RedirectCache;
  RedirectCache recent_redirects_;

  // Timestamp of the first entry in our database.
  base::Time first_recorded_time_;

  // When set, this is the task that should be invoked on destruction.
  scoped_refptr<base::SingleThreadTaskRunner> backend_destroy_task_runner_;
  base::Closure backend_destroy_task_;

  // Tracks page transition types.
  VisitTracker tracker_;

  // A boolean variable to track whether we have already purged obsolete segment
  // data.
  bool segment_queried_;

  // List of QueuedHistoryDBTasks to run;
  std::list<std::unique_ptr<QueuedHistoryDBTask>> queued_history_db_tasks_;

  // Used to determine if a URL is bookmarked; may be null.
  std::unique_ptr<HistoryBackendClient> backend_client_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to allow embedder code to stash random data by key. Those object will
  // be deleted before closing the databases (hence the member variable instead
  // of inheritance from base::SupportsUserData).
  std::unique_ptr<HistoryBackendHelper> supports_user_data_helper_;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Contains diagnostic information about the sql database that is non-empty
  // when a catastrophic error occurs.
  std::string db_diagnostics_;

  // List of observers
  base::ObserverList<HistoryBackendObserver>::Unchecked observers_;

  // Used to manage syncing of the typed urls datatype. It will be null before
  // HistoryBackend::Init is called. Defined after observers_ because
  // it unregisters itself as observer during destruction.
  std::unique_ptr<TypedURLSyncBridge> typed_url_sync_bridge_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackend);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_H_
