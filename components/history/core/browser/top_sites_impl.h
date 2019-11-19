// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/synchronization/lock.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_backend.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
}

namespace history {

class TopSitesImplTest;

// This class allows requests for most visited urls on any thread. All other
// methods must be invoked on the UI thread. All mutations to internal state
// happen on the UI thread and are scheduled to update the db using
// TopSitesBackend.
class TopSitesImpl : public TopSites, public HistoryServiceObserver {
 public:
  // Called to check whether an URL can be added to the history. Must be
  // callable multiple time and during the whole lifetime of TopSitesImpl.
  using CanAddURLToHistoryFn = base::Callback<bool(const GURL&)>;

  // How many top sites to store in the cache.
  static constexpr size_t kTopSitesNumber = 10;

  TopSitesImpl(PrefService* pref_service,
               HistoryService* history_service,
               const PrepopulatedPageList& prepopulated_pages,
               const CanAddURLToHistoryFn& can_add_url_to_history);

  // Initializes TopSitesImpl.
  void Init(const base::FilePath& db_name);

  // TopSites implementation.
  void GetMostVisitedURLs(const GetMostVisitedURLsCallback& callback) override;
  void SyncWithHistory() override;
  bool HasBlacklistedItems() const override;
  void AddBlacklistedURL(const GURL& url) override;
  void RemoveBlacklistedURL(const GURL& url) override;
  bool IsBlacklisted(const GURL& url) override;
  void ClearBlacklistedURLs() override;
  bool IsFull() override;
  PrepopulatedPageList GetPrepopulatedPages() override;
  bool loaded() const override;
  void OnNavigationCommitted(const GURL& url) override;

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // Register preferences used by TopSitesImpl.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  ~TopSitesImpl() override;

 private:
  // TODO(yiyaoliu): Remove the enums and related code when crbug/223430 is
  // fixed.
  // An enum representing different situations under which function SetTopSites
  // can be initiated.
  // This is needed because a histogram is used to record speed related metrics
  // when SetTopSites is initiated from OnGotMostVisitedURLs, which usually
  // happens early and might affect Chrome startup speed.
  enum CallLocation {
    // SetTopSites is called from function OnGotMostVisitedURLs.
    CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_URLS,
    // All other situations.
    CALL_LOCATION_FROM_OTHER_PLACES,
  };

  friend class TopSitesImplTest;
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisited);
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisitedWithForced);

  typedef base::Callback<void(const MostVisitedURLList&)> PendingCallback;

  typedef std::vector<PendingCallback> PendingCallbacks;

  // Starts to query most visited URLs from history database instantly. Also
  // cancels any pending queries requested in a delayed manner by canceling the
  // timer.
  void StartQueryForMostVisited();

  // Generates the diff of things that happened between "old" and "new."
  //
  // The URLs that are in "new" but not "old" will be have their index from
  // "new" placed in |added_urls|. The URLs that are in "old" but not "new" will
  // have their index from "old" placed in |deleted_urls|.
  //
  // URLs that appear in both lists but have different indices will have their
  // index from "new" placed in |moved_urls|.
  static void DiffMostVisited(const MostVisitedURLList& old_list,
                              const MostVisitedURLList& new_list,
                              TopSitesDelta* delta);

  // Adds prepopulated pages to TopSites. Returns true if any pages were added.
  bool AddPrepopulatedPages(MostVisitedURLList* urls) const;

  // Takes |urls|, produces it's copy in |out| after removing blacklisted URLs.
  // Also ensures we respect the maximum number TopSites URLs.
  MostVisitedURLList ApplyBlacklist(const MostVisitedURLList& urls);

  // Returns an MD5 hash of the URL. Hashing is required for blacklisted URLs.
  static std::string GetURLHash(const GURL& url);

  // Updates URLs in |cache_| and the db (in the background). The URLs in
  // |new_top_sites| replace those in |cache_|. All mutations to cache_ *must*
  // go through this. Should be called from the UI thread.
  void SetTopSites(MostVisitedURLList new_top_sites,
                   const CallLocation location);

  // Returns the number of most visited results to request from history. This
  // changes depending upon how many urls have been blacklisted. Should be
  // called from the UI thread.
  int num_results_to_request_from_history() const;

  // Invoked when transitioning to LOADED. Notifies any queued up callbacks.
  // Should be called from the UI thread.
  void MoveStateToLoaded();

  void ResetThreadSafeCache();

  // Schedules a timer to update top sites with a delay.
  // Does nothing if there is already a request queued.
  void ScheduleUpdateTimer();

  // Callback from TopSites with the list of top sites. Should be called from
  // the UI thread.
  void OnGotMostVisitedURLs(MostVisitedURLList sites);

  // Called when history service returns a list of top URLs.
  void OnTopSitesAvailableFromHistory(MostVisitedURLList data);

  // history::HistoryServiceObserver:
  void OnURLsDeleted(HistoryService* history_service,
                     const DeletionInfo& deletion_info) override;

  // Ensures that non thread-safe methods are called on the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<TopSitesBackend> backend_;

  // Lock used to access |thread_safe_cache_|.
  mutable base::Lock lock_;

  // The top sites data.
  MostVisitedURLList top_sites_;

  // Copy of the top sites data that may be accessed on any thread (assuming
  // you hold |lock_|). The data in |thread_safe_cache_| has blacklisted urls
  // applied (|top_sites_| does not).
  MostVisitedURLList thread_safe_cache_ GUARDED_BY(lock_);

  // Task tracker for history and backend requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Timer that asks history for the top sites. This is used to coalesce
  // requests that are generated in quick succession.
  base::OneShotTimer timer_;

  // The pending requests for the top sites list. Can only be non-empty at
  // startup. After we read the top sites from the DB, we'll always have a
  // cached list and be able to run callbacks immediately.
  PendingCallbacks pending_callbacks_ GUARDED_BY(lock_);

  // URL List of prepopulated page.
  const PrepopulatedPageList prepopulated_pages_;

  // PrefService holding the NTP URL blacklist dictionary. Must outlive
  // TopSitesImpl.
  PrefService* pref_service_;

  // HistoryService that TopSitesImpl can query. May be null, but if defined it
  // must outlive TopSitesImpl.
  HistoryService* history_service_;

  // Can URL be added to the history?
  CanAddURLToHistoryFn can_add_url_to_history_;

  // Are we loaded?
  bool loaded_;

  // Have the SetTopSites execution time related histograms been recorded?
  // The histogram should only be recorded once for each Chrome execution.
  static bool histogram_recorded_;

  ScopedObserver<HistoryService, HistoryServiceObserver>
      history_service_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(TopSitesImpl);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_
