// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"
#include "components/no_state_prefetch/browser/prerender_config.h"
#include "components/no_state_prefetch/browser/prerender_histograms.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class TickClock;
}  // namespace base

namespace chrome_browser_net {
enum class NetworkPredictionStatus;
}

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace prerender {

namespace test_utils {
class PrerenderInProcessBrowserTest;
}

class NoStatePrefetchHandle;
class NoStatePrefetchHistory;

// Observer interface for NoStatePrefetchManager events.
class NoStatePrefetchManagerObserver {
 public:
  virtual ~NoStatePrefetchManagerObserver();

  // Called from the UI thread.
  virtual void OnFirstContentfulPaint() = 0;
};

// NoStatePrefetchManager is responsible for initiating and keeping prerendered
// views of web pages. All methods must be called on the UI thread unless
// indicated otherwise.
class NoStatePrefetchManager : public content::RenderProcessHostObserver,
                               public KeyedService {
 public:
  // One or more of these flags must be passed to ClearData() to specify just
  // what data to clear.  See function declaration for more information.
  enum ClearFlags {
    CLEAR_PRERENDER_CONTENTS = 0x1 << 0,
    CLEAR_PRERENDER_HISTORY = 0x1 << 1,
    CLEAR_MAX = 0x1 << 2
  };

  // Owned by a BrowserContext object for the lifetime of the browser_context.
  NoStatePrefetchManager(
      content::BrowserContext* browser_context,
      std::unique_ptr<NoStatePrefetchManagerDelegate> delegate);

  NoStatePrefetchManager(const NoStatePrefetchManager&) = delete;
  NoStatePrefetchManager& operator=(const NoStatePrefetchManager&) = delete;

  ~NoStatePrefetchManager() override;

  // From KeyedService:
  void Shutdown() override;

  // Entry points for adding prerenders.

  // Starts a prefetch for |url| if valid. |process_id| and |route_id| identify
  // the RenderView that the prefetch request came from. If |size| is empty, a
  // default from the PrerenderConfig is used. Returns a NoStatePrefetchHandle
  // if the URL was added, NULL if it was not. If the launching RenderView is
  // itself prefetching, the prefetch is added as a pending prefetch.
  std::unique_ptr<NoStatePrefetchHandle> StartPrefetchingFromLinkRelPrerender(
      int process_id,
      int route_id,
      const GURL& url,
      blink::mojom::PrerenderTriggerType trigger_type,
      const content::Referrer& referrer,
      const url::Origin& initiator_origin,
      const gfx::Size& size);

  // Adds a NoStatePrefetch that only allows for same origin requests (i.e.,
  // requests that only redirect to the same origin).
  std::unique_ptr<NoStatePrefetchHandle> AddSameOriginSpeculation(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size,
      const url::Origin& initiator_origin);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // Destroys all pending prerenders using FinalStatus.  Also deletes them as
  // well as any swapped out WebContents queued for destruction.
  // Used both on destruction, and when clearing the browsing history.
  void DestroyAllContents(FinalStatus final_status);

  // Moves a NoStatePrefetchContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  virtual void MoveEntryToPendingDelete(NoStatePrefetchContents* entry,
                                        FinalStatus final_status);

  // Query the list of current prefetches to see if the given web contents is
  // prefetching a page.
  bool IsWebContentsPrefetching(const content::WebContents* web_contents) const;

  // Returns the NoStatePrefetchContents object for the given web_contents,
  // otherwise returns NULL. Note that the NoStatePrefetchContents may have been
  // Destroy()ed, but not yet deleted.
  NoStatePrefetchContents* GetNoStatePrefetchContents(
      const content::WebContents* web_contents) const;

  // Returns the NoStatePrefetchContents object for a given child_id, route_id
  // pair, otherwise returns NULL. Note that the NoStatePrefetchContents may
  // have been Destroy()ed, but not yet deleted.
  virtual NoStatePrefetchContents* GetNoStatePrefetchContentsForRoute(
      int child_id,
      int route_id) const;

  // Returns a list of all WebContents being NoStatePrefetched.
  std::vector<content::WebContents*>
  GetAllNoStatePrefetchingContentsForTesting() const;

  // Checks whether |url| has been recently navigated to.
  bool HasRecentlyBeenNavigatedTo(Origin origin, const GURL& url);

  // Returns a `base::Value::Dict` object containing the active pages being
  // prerendered, and a history of pages which were prerendered.
  base::Value::Dict CopyAsDict() const;

  // Clears the data indicated by which bits of clear_flags are set.
  //
  // If the CLEAR_PRERENDER_CONTENTS bit is set, all active prerenders are
  // cancelled and then deleted, and any WebContents queued for destruction are
  // destroyed as well.
  //
  // If the CLEAR_PRERENDER_HISTORY bit is set, the prerender history is
  // cleared, including any entries newly created by destroying them in
  // response to the CLEAR_PRERENDER_CONTENTS flag.
  //
  // Intended to be used when clearing the cache or history.
  void ClearData(int clear_flags);

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin, FinalStatus final_status) const;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  // Return current time and ticks with ability to mock the clock out for
  // testing.
  base::Time GetCurrentTime() const;
  base::TimeTicks GetCurrentTimeTicks() const;
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void AddObserver(std::unique_ptr<NoStatePrefetchManagerObserver> observer);

  // Registers a new ProcessHost performing a prerender. Called by
  // NoStatePrefetchContents.
  void AddPrerenderProcessHost(content::RenderProcessHost* process_host);

  // Returns whether or not |process_host| may be reused for new navigations
  // from a prerendering perspective. Currently, if Prerender Cookie Stores are
  // enabled, prerenders must be in their own processes that may not be shared.
  bool MayReuseProcessHost(content::RenderProcessHost* process_host);

  // content::RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Cleans up the expired prefetches and then returns true if |url| was
  // no-state prefetched recently. If so, |prefetch_age|, |final_status| and
  // |origin| are set based on the no-state prefetch information if they are
  // non-null.
  bool GetPrefetchInformation(const GURL& url,
                              base::TimeDelta* prefetch_age,
                              FinalStatus* final_status,
                              Origin* origin);

  void SetNoStatePrefetchContentsFactoryForTest(
      NoStatePrefetchContents::Factory* no_state_prefetch_contents_factory);

  base::WeakPtr<NoStatePrefetchManager> AsWeakPtr();

  // Clears the list of recently prefetched URLs. Allows, for example, to reuse
  // the same URL in tests, without running into FINAL_STATUS_DUPLICATE.
  void ClearPrefetchInformationForTesting();

  // Starts a prefetch for |url| from |initiator_origin|. The |origin| specifies
  // how the prefetch was started. Returns a NoStatePrefetchHandle or nullptr.
  // Only for testing.
  std::unique_ptr<NoStatePrefetchHandle>
  StartPrefetchingWithPreconnectFallbackForTesting(
      Origin origin,
      const GURL& url,
      const std::optional<url::Origin>& initiator_origin);

 protected:
  class NoStatePrefetchData {
   public:
    struct OrderByExpiryTime;

    NoStatePrefetchData(NoStatePrefetchManager* manager,
                        std::unique_ptr<NoStatePrefetchContents> contents,
                        base::TimeTicks expiry_time);

    NoStatePrefetchData(const NoStatePrefetchData&) = delete;
    NoStatePrefetchData& operator=(const NoStatePrefetchData&) = delete;

    ~NoStatePrefetchData();

    // A new NoStatePrefetchHandle has been created for this
    // NoStatePrefetchData.
    void OnHandleCreated(NoStatePrefetchHandle* handle);

    // The launcher associated with a handle is navigating away from the context
    // that launched this prefetch. If the prerender is active, it may stay
    // alive briefly though, in case we we going through a redirect chain that
    // will eventually land at it.
    void OnHandleNavigatedAway(NoStatePrefetchHandle* handle);

    // The launcher associated with a handle has taken explicit action to cancel
    // this prefetch. We may well destroy the prerender in this case if no other
    // handles continue to track it.
    void OnHandleCanceled(NoStatePrefetchHandle* handle);

    NoStatePrefetchContents* contents() { return contents_.get(); }

    std::unique_ptr<NoStatePrefetchContents> ReleaseContents();

    int handle_count() const { return handle_count_; }

    base::TimeTicks expiry_time() const { return expiry_time_; }
    void set_expiry_time(base::TimeTicks expiry_time) {
      expiry_time_ = expiry_time;
    }

    base::WeakPtr<NoStatePrefetchData> AsWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }

   private:
    const raw_ptr<NoStatePrefetchManager> manager_;
    std::unique_ptr<NoStatePrefetchContents> contents_;

    // The number of distinct NoStatePrefetchHandles created for |this|,
    // including ones that have called
    // NoStatePrefetchData::OnHandleNavigatedAway(), but not counting the ones
    // that have called NoStatePrefetchData::OnHandleCanceled(). For pending
    // prefetches, this will always be 1, since the NoStatePrefetchManager only
    // merges handles of running prefetches.
    int handle_count_ = 0;

    // After this time, this prefetch is no longer fresh, and should be removed.
    base::TimeTicks expiry_time_;

    base::WeakPtrFactory<NoStatePrefetchData> weak_factory_{this};
  };

  // Called by a NoStatePrefetchData to signal that the launcher has navigated
  // away from the context that launched the prefetch. A user may have clicked
  // a link in a page containing a <link rel=prerender> element, or the user
  // might have committed an omnibox navigation. This is used to possibly
  // shorten the TTL of the page for NoStatePrefetch.
  void SourceNavigatedAway(NoStatePrefetchData* prefetch_data);

  // Same as base::SysInfo::IsLowEndDevice(), overridden in tests.
  virtual bool IsLowEndDevice() const;

  // Whether network prediction is enabled for prefetch origin, |origin|.
  bool IsPredictionEnabled(Origin origin);

 private:
  friend class test_utils::PrerenderInProcessBrowserTest;
  friend class NoStatePrefetchContents;
  friend class NoStatePrefetchHandle;
  friend class UnitTestNoStatePrefetchManager;

  class OnCloseWebContentsDeleter;
  struct NavigationRecord;
  using NoStatePrefetchDataVector =
      std::vector<std::unique_ptr<NoStatePrefetchData>>;

  // Time interval before a new prefetch is allowed.
  static const int kMinTimeBetweenPrefetchesMs = 500;

  // Time window for which we record old navigations, in milliseconds.
  static const int kNavigationRecordWindowMs = 5000;

  // Starts a prefetch for |url| from |referrer|. The |origin| specifies how the
  // prefetch was started. If |bounds| is empty, then
  // NoStatePrefetchContents::StartPrerendering will instead use a default from
  // PrerenderConfig. Returns a NoStatePrefetchHandle or NULL.
  // PreloadingAttempt helps us to log various metrics associated with
  // particular NoStatePrefetch attempt.
  // TODO(crbug.com/40238653): Remove nullptr as default parameter once NSP is
  // integrated with all different predictors.
  std::unique_ptr<NoStatePrefetchHandle> StartPrefetchingWithPreconnectFallback(
      Origin origin,
      const GURL& url,
      const content::Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace,
      base::WeakPtr<content::PreloadingAttempt> attempt = nullptr);

  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  // Deletes stale and cancelled prerendered NoStatePrefetchContents, as well as
  // WebContents that have been replaced by prerendered WebContents.
  // Also identifies and kills NoStatePrefetchContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  base::TimeTicks GetExpiryTimeForNewPrerender(Origin origin) const;
  base::TimeTicks GetExpiryTimeForNavigatedAwayPrerender() const;

  void DeleteOldEntries();

  void DeleteToDeletePrerenders();

  // Virtual so unit tests can override this.
  virtual std::unique_ptr<NoStatePrefetchContents>
  CreateNoStatePrefetchContents(
      const GURL& url,
      const content::Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      Origin origin);

  // Insures the |active_prefetches_| are sorted by increasing expiry time. Call
  // after every mutation of |active_prefetches_| that can possibly make it
  // unsorted (e.g. an insert, or changing an expiry time).
  void SortActivePrefetches();

  // Finds the active NoStatePrefetchData object for a running prefetch matching
  // |url| and |session_storage_namespace|.
  NoStatePrefetchData* FindNoStatePrefetchData(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace);

  // Given the |no_state_prefetch_contents|, find the iterator in
  // |active_prefetches_| corresponding to the given prefetch.
  NoStatePrefetchDataVector::iterator FindIteratorForNoStatePrefetchContents(
      NoStatePrefetchContents* no_state_prefetch_contents);

  bool DoesRateLimitAllowPrefetch(Origin origin) const;

  // Deletes old WebContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old WebContents,
  // so cannot immediately be deleted.
  void DeleteOldWebContents();

  // Called when NoStatePrefetchContents gets destroyed. Attaches the
  // |final_status| to the most recent prefetch matching the |url|.
  void SetPrefetchFinalStatusForUrl(const GURL& url, FinalStatus final_status);

  // Called when a prefetch has been used. Prefetches avoid cache revalidation
  // only once.
  void OnPrefetchUsed(const GURL& url);

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations(std::vector<NavigationRecord>* navigations,
                             base::TimeDelta max_age);

  // Arrange for the given WebContents to be deleted asap. Delete |deleter| as
  // well.
  void ScheduleDeleteOldWebContents(std::unique_ptr<content::WebContents> tab,
                                    OnCloseWebContentsDeleter* deleter);

  // Adds to the history list.
  void AddToHistory(NoStatePrefetchContents* contents);

  // Returns a new `base::Value::List` representing the pages currently being
  // prerendered.
  base::Value::List GetActivePrerenders() const;

  // Records the final status a prerender in the case that a
  // NoStatePrefetchContents was never created, adds a NoStatePrefetchHistory
  // entry, and may also initiate a preconnect to |url|.
  void SkipNoStatePrefetchContentsAndMaybePreconnect(
      const GURL& url,
      Origin origin,
      FinalStatus final_status) const;

  // May initiate a preconnect to |url_arg| based on |origin|.
  void MaybePreconnect(Origin origin, const GURL& url_arg) const;

  // The configuration.
  Config config_;

  // The browser_context that owns this NoStatePrefetchManager.
  raw_ptr<content::BrowserContext> browser_context_;

  // The delegate that allows content embedder to override the logic in this
  // class.
  std::unique_ptr<NoStatePrefetchManagerDelegate> delegate_;

  // All running prefetches. Sorted by expiry time, in ascending order.
  NoStatePrefetchDataVector active_prefetches_;

  // Prefetches awaiting deletion.
  NoStatePrefetchDataVector to_delete_prefetches_;

  // List of recent navigations in this browser_context, sorted by ascending
  // |navigate_time_|.
  std::vector<NavigationRecord> navigations_;

  // List of recent prefetches, sorted by ascending navigate time.
  std::vector<NavigationRecord> prefetches_;

  std::unique_ptr<NoStatePrefetchContents::Factory>
      no_state_prefetch_contents_factory_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer repeating_timer_;

  // Track time of last prefetch to limit prefetch spam.
  base::TimeTicks last_prefetch_start_time_;

  std::vector<std::unique_ptr<content::WebContents>> old_web_contents_list_;

  std::vector<std::unique_ptr<OnCloseWebContentsDeleter>>
      on_close_web_contents_deleters_;

  const std::unique_ptr<NoStatePrefetchHistory> prefetch_history_;

  const std::unique_ptr<PrerenderHistograms> histograms_;

  // Set of process hosts being prerendered.
  using PrerenderProcessSet =
      std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>;
  PrerenderProcessSet prerender_process_hosts_;

  raw_ptr<const base::TickClock> tick_clock_;

  std::vector<std::unique_ptr<NoStatePrefetchManagerObserver>> observers_;

  base::WeakPtrFactory<NoStatePrefetchManager> weak_factory_{this};
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_H_
