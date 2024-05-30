// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

#include <optional>
#include <unordered_map>

#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "ui/base/clipboard/clipboard.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class NavigationHandle;
struct GlobalRenderFrameHostId;
}

namespace safe_browsing {

class SafeBrowsingNavigationObserver;
struct NavigationEvent;
struct ResolvedIPAddress;

// User data stored in DownloadItem for referrer chain information.
class ReferrerChainData : public base::SupportsUserData::Data {
 public:
  ReferrerChainData(ReferrerChainProvider::AttributionResult attribution_result,
                    std::unique_ptr<ReferrerChain> referrer_chain,
                    size_t referrer_chain_length,
                    size_t recent_navigation_to_collect);
  ~ReferrerChainData() override;
  ReferrerChainProvider::AttributionResult attribution_result() const {
    return attribution_result_;
  }
  ReferrerChain* GetReferrerChain();
  size_t referrer_chain_length() { return referrer_chain_length_; }
  size_t recent_navigations_to_collect() {
    return recent_navigations_to_collect_;
  }

  // Unique user data key used to get and set referrer chain data in
  // DownloadItem.
  static const char kDownloadReferrerChainDataKey[];

 private:
  // Result of trying to get the referrer chain. Referrer chains are
  // fetched once per download, at the beginning of downloading to disk.
  ReferrerChainProvider::AttributionResult attribution_result_ =
      ReferrerChainProvider::AttributionResult::NAVIGATION_EVENT_NOT_FOUND;
  // The referrer chain itself
  std::unique_ptr<ReferrerChain> referrer_chain_;
  // This is the actual referrer chain length before appending recent navigation
  // events;
  size_t referrer_chain_length_;
  // |recent_navigations_to_collect_| is controlled by finch parameter. If the
  // user is incognito mode or hasn't enabled extended reporting, this value is
  // always 0.
  size_t recent_navigations_to_collect_;
};

// Struct to store a URL copied to the clipboard, along with which frame and
// main_frame this was copied from.
struct CopyPasteEntry {
  explicit CopyPasteEntry(GURL target,
                          GURL source_frame_url,
                          GURL source_main_frame_url,
                          base::Time recorded_time);
  CopyPasteEntry(const CopyPasteEntry& other);
  GURL target_;
  GURL source_frame_url_;
  GURL source_main_frame_url_;
  base::Time recorded_time_;
};

// Struct that manages insertion, cleanup, and lookup of NavigationEvent
// objects. Its maximum size is `GetNavigationRecordMaxSize()`.
struct NavigationEventList {
 public:
  explicit NavigationEventList(std::size_t size_limit);

  ~NavigationEventList();

  // Finds the index of the most recent navigation event that navigated to
  // |target_url| and  its associated |target_main_frame_url| in the tab with
  // ID |target_tab_id|. Returns an empty optional if event is not found.
  // If navigation happened in the main frame, |target_url| and
  // |target_main_frame_url| are the same.
  // If |target_url| is empty, we use its main frame url (a.k.a.
  // |target_main_frame_url|) to search for navigation events.
  // If |target_tab_id| is invalid, we look for all tabs for the most
  // recent navigation to |target_url| or |target_main_frame_url|.
  // This method starts traversing the list in reverse order of events starting
  // from |start_index| to prevent infinite loops.
  // For some cases, the most recent navigation to |target_url| may not
  // be relevant. For example, url1 in window A opens url2 in window B, url1
  // then opens an about:blank page window C and injects script code in it to
  // trigger a delayed event (e.g. a download) in Window D. Before the event
  // occurs, url2 in window B opens a different about:blank page in window C.
  // A ---- C - D
  //   \   /
  //     B
  // In this case, FindNavigationEvent() will think url2 in Window B is the
  // referrer of about::blank in Window C since this navigation is more recent.
  // However, it does not prevent us to attribute url1 in Window A as the cause
  // of all these navigations. Returns an empty optional if an event is not
  // found.
  //
  // If an |outermost_main_frame_id| is supplied, the function attempts to find
  // a navigation event per the logic described above with the additional
  // constraint that the |outermost_main_frame_id| match. If there is no such
  // event, it will return the first main frame event that matches the other
  // criteria. And if there is still no matching event, the function will return
  // an empty optional.
  std::optional<size_t> FindNavigationEvent(
      const base::Time& last_event_timestamp,
      const GURL& target_url,
      const GURL& target_main_frame_url,
      SessionID target_tab_id,
      const content::GlobalRenderFrameHostId& outermost_main_frame_id,
      size_t start_index);

  // Finds the the navigation event in the |pending_navigation_events_| map that
  // has the same destination URL as the |target_url|. If there are multiple
  // matches, returns the one with the latest updated time.
  NavigationEvent* FindPendingNavigationEvent(const GURL& target_url);

  // Finds the index of the most recent retargeting NavigationEvent index in the
  // list that satisfies the |target_tab_id| and is not the same NavigationEvent
  // stored in |start_index|. Returns -1 if event is not found.
  size_t FindRetargetingNavigationEvent(const base::Time& last_event_timestamp,
                                        SessionID target_tab_id,
                                        size_t start_index);

  void RecordNavigationEvent(
      std::unique_ptr<NavigationEvent> nav_event,
      std::optional<CopyPasteEntry> last_copy_paste_entry = std::nullopt);

  void RecordPendingNavigationEvent(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<NavigationEvent> nav_event);

  void AddRedirectUrlToPendingNavigationEvent(
      content::NavigationHandle* navigation_handle,
      const GURL& server_redirect_url);

  void RemovePendingNavigationEvent(
      content::NavigationHandle* navigation_handle);

  // Removes stale NavigationEvents and return the number of items removed.
  std::size_t CleanUpNavigationEvents();

  std::size_t NavigationEventsSize() { return navigation_events_.size(); }
  std::size_t PendingNavigationEventsSize() {
    return pending_navigation_events_.size();
  }

  NavigationEvent* GetNavigationEvent(std::size_t index) {
    return navigation_events_[index].get();
  }

  const base::circular_deque<std::unique_ptr<NavigationEvent>>&
  navigation_events() {
    return navigation_events_;
  }

  const base::flat_map<content::NavigationHandle*,
                       std::unique_ptr<NavigationEvent>>&
  pending_navigation_events() {
    return pending_navigation_events_;
  }

 private:
  base::circular_deque<std::unique_ptr<NavigationEvent>> navigation_events_;
  // A map of pending navigation events. They are added when the navigation
  // starts and removed when the navigation is finished.
  base::flat_map<content::NavigationHandle*, std::unique_ptr<NavigationEvent>>
      pending_navigation_events_;

  const std::size_t size_limit_;
};

// Manager class for SafeBrowsingNavigationObserver, which is in charge of
// cleaning up stale navigation events, and identifying landing page/landing
// referrer for a specific Safe Browsing event.
class SafeBrowsingNavigationObserverManager
    : public ReferrerChainProvider,
      public content::ServiceWorkerContextObserver,
      public KeyedService,
      public ui::Clipboard::ClipboardWriteObserver {
 public:
  // Helper function to check if user gesture is older than
  // kUserGestureTTL.
  static bool IsUserGestureExpired(const base::Time& timestamp);

  // Helper function to strip ref fragment from a URL. Many pages end up with a
  // fragment (e.g. http://bar.com/index.html#foo) at the end due to in-page
  // navigation or a single "#" at the end due to navigation triggered by
  // href="#" and javascript onclick function. We don't want to have separate
  // entries for these cases in the maps.
  static GURL ClearURLRef(const GURL& url);

  // Checks if we should enable observing navigations for safe browsing
  // purposes. Returns true if safe browsing is enabled and the safe browsing
  // service is present in the embedder.
  static bool IsEnabledAndReady(PrefService* prefs,
                                bool has_safe_browsing_service);

  // Sanitize referrer chain by only keeping origin information of all URLs.
  static void SanitizeReferrerChain(ReferrerChain* referrer_chain);

  explicit SafeBrowsingNavigationObserverManager(
      PrefService* pref_service,
      content::ServiceWorkerContext* context);

  SafeBrowsingNavigationObserverManager(
      const SafeBrowsingNavigationObserverManager&) = delete;
  SafeBrowsingNavigationObserverManager& operator=(
      const SafeBrowsingNavigationObserverManager&) = delete;

  ~SafeBrowsingNavigationObserverManager() override;

  // Adds |nav_event| to |navigation_event_list_|. Object pointed to by
  // |nav_event| will be no longer accessible after this function.
  void RecordNavigationEvent(content::NavigationHandle* navigation_handle,
                             std::unique_ptr<NavigationEvent> nav_event);
  void RecordPendingNavigationEvent(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<NavigationEvent> nav_event);
  // Record that a Push Notification initiated a navigation.
  // |script_url| is the URL of the service worker.
  // |url| is the destination URL.
  void RecordNotificationNavigationEvent(const GURL& script_url,
                                         const GURL& url);
  void AddRedirectUrlToPendingNavigationEvent(
      content::NavigationHandle* navigation_handle,
      const GURL& server_redirect_url);
  void RecordUserGestureForWebContents(content::WebContents* web_contents);
  void OnUserGestureConsumed(content::WebContents* web_contents);
  bool HasUserGesture(content::WebContents* web_contents);
  bool HasUnexpiredUserGesture(content::WebContents* web_contents);
  void RecordHostToIpMapping(const std::string& host, const std::string& ip);

  // Clean-ups need to be done when a WebContents gets destroyed.
  void OnWebContentDestroyed(content::WebContents* web_contents);

  // Removes all the observed NavigationEvents, user gestures, and resolved IP
  // addresses that are older than `GetNavigationFootprintTTL()`.
  void CleanUpStaleNavigationFootprints();

  // Based on the |event_url| and |event_tab_id|, traces back the observed
  // NavigationEvents in navigation_event_list_ to identify the sequence of
  // navigations leading to the target, with the coverage limited to
  // |user_gesture_count_limit| number of user gestures. Then converts these
  // identified NavigationEvents into ReferrerChainEntrys and appends them to
  // |out_referrer_chain|.
  AttributionResult IdentifyReferrerChainByEventURL(
      const GURL& event_url,
      SessionID event_tab_id,  // Invalid if tab id is unknown or not available.
      const content::GlobalRenderFrameHostId&
          event_outermost_main_frame_id,  // Can also be Invalid.
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) override;

  // Helper function to |IdentifyReferrerChainByEventURL| above in cases where
  // |event_outermost_main_frame_id| is not available. That value will default
  // to |content::GlobalRenderFrameHostId()|.
  AttributionResult IdentifyReferrerChainByEventURL(
      const GURL& event_url,
      SessionID event_tab_id,  // Invalid if tab id is unknown or not available.
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) override;

  // Based on the |event_url|, traces back the observed PendingNavigationEvents
  // and NavigationEvents in navigation_event_list_ to identify the sequence of
  // navigations leading to the |event_url|, with the coverage limited to
  // |user_gesture_count_limit| number of user gestures. Then converts these
  // identified NavigationEvents into ReferrerChainEntrys and appends them to
  // |out_referrer_chain|.
  // Note that the first entry of the ReferrerChainEntrys is matched against the
  // PendingNavigationEvents, and the remaining entries are matched against the
  // NavigationEvents.
  AttributionResult IdentifyReferrerChainByPendingEventURL(
      const GURL& event_url,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) override;

  // Based on the |render_frame_host| associated with an event, traces back the
  // observed NavigationEvents in |navigation_event_list_| to identify the
  // sequence of navigations leading to the event hosting page, with the
  // coverage limited to |user_gesture_count_limit| number of user gestures.
  // Then converts these identified NavigationEvents into ReferrerChainEntrys
  // and appends them to |out_referrer_chain|.
  AttributionResult IdentifyReferrerChainByRenderFrameHost(
      content::RenderFrameHost* render_frame_host,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain) override;

  // Based on the |initiating_frame_url| and its associated |tab_id|, traces
  // back the observed NavigationEvents in navigation_event_list_ to identify
  // those navigations leading to this |initiating_frame_url|. If this
  // initiating frame has a user gesture, we trace back with the coverage
  // limited to |user_gesture_count_limit|-1 number of user gestures, otherwise
  // we trace back |user_gesture_count_limit| number of user gestures. We then
  // converts these identified NavigationEvents into ReferrerChainEntrys and
  // appends them to |out_referrer_chain|.
  AttributionResult IdentifyReferrerChainByHostingPage(
      const GURL& initiating_frame_url,
      const GURL& initiating_main_frame_url,
      const content::GlobalRenderFrameHostId&
          initiating_outermost_main_frame_id,
      SessionID tab_id,
      bool has_user_gesture,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain);

  // Records the creation of a new WebContents by |source_web_contents|. This is
  // used to detect cross-frame and cross-tab navigations.
  void RecordNewWebContents(content::WebContents* source_web_contents,
                            content::RenderFrameHost* source_render_frame_host,
                            const GURL& target_url,
                            ui::PageTransition page_transition,
                            content::WebContents* target_web_contents,
                            bool renderer_initiated);

  // Based on user state, attribution result and finch parameter, calculates the
  // number of recent navigations we want to append to the referrer chain.
  static size_t CountOfRecentNavigationsToAppend(
      content::BrowserContext* browser_context,
      PrefService* prefs,
      AttributionResult result);

  // Appends |recent_navigation_count| number of recent navigation events to
  // referrer chain in reverse chronological order.
  void AppendRecentNavigations(size_t recent_navigation_count,
                               ReferrerChain* out_referrer_chain);

  // ui::Clipboard::ClipboardWriteObserver:
  // Event for new URLs copied to the clipboard
  void OnCopyURL(const GURL& url,
                 const GURL& source_frame_url,
                 const GURL& source_main_frame_url) override;

  // content::ServiceWorkerContextObserver implementation.
  void OnClientNavigated(const GURL& script_url, const GURL& url) override;
  void OnWindowOpened(const GURL& script_url, const GURL& url) override;

 protected:
  NavigationEventList* navigation_event_list() {
    return &navigation_event_list_;
  }

 private:
  friend class TestNavigationObserverManager;
  friend class SBNavigationObserverBrowserTest;
  friend class SBNavigationObserverTest;
  friend class ChromeClientSideDetectionHostDelegateTest;

  struct GurlHash {
    std::size_t operator()(const GURL& url) const {
      return std::hash<std::string>()(url.spec());
    }
  };

  typedef std::unordered_map<content::WebContents*, base::Time> UserGestureMap;
  typedef std::unordered_map<std::string, std::vector<ResolvedIPAddress>>
      HostToIpMap;

  HostToIpMap* host_to_ip_map() { return &host_to_ip_map_; }

  // Remove stale entries from navigation_event_list_ if they are older than
  // `GetNavigationFootprintTTL()`.
  void CleanUpNavigationEvents();

  // Remove stale entries from user_gesture_map_ if they are older than
  // `GetNavigationFootprintTTL()`.
  void CleanUpUserGestures();

  // Remove stale entries from host_to_ip_map_ if they are older than
  // `GetNavigationFootprintTTL()`.
  void CleanUpIpAddresses();

  // Remove stale copy entries.
  void CleanUpCopyData();

  // Remove stale entries from notification_navigation_events_.
  void CleanUpNotificationNavigationEvents();

  bool IsCleanUpScheduled() const;

  void ScheduleNextCleanUpAfterInterval(base::TimeDelta interval);

  // Adds the event to the referrer chain, unless it is older than
  // `GetNavigationFootprintTTL()`.
  void MaybeAddToReferrerChain(ReferrerChain* referrer_chain,
                               NavigationEvent* nav_event,
                               const GURL& destination_main_frame_url,
                               ReferrerChainEntry::URLType type);

  // Helper function to get the remaining referrer chain when we've already
  // traced back |current_user_gesture_count| number of user gestures.
  // This method uses a |last_nav_event_traced_index| to check where to start
  // in |navigation_events_|.
  // This function modifies the |out_referrer_chain| and |out_result|.
  void GetRemainingReferrerChain(size_t last_nav_event_traced_index,
                                 int current_user_gesture_count,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain,
                                 AttributionResult* out_result);

  // Helper function to get the remaining referrer chain when we've already
  // traced back |current_user_gesture_count| number of user gestures.
  // This method uses a |last_nav_event_traced_index| to check where to start
  // in |navigation_events_| and the |last_nav_event_traced| to get the next
  // entry. This function modifies the |out_referrer_chain| and |out_result|.
  void GetRemainingReferrerChainForNavEvent(
      NavigationEvent* last_nav_event_traced,
      size_t last_nav_event_traced_index,
      int current_user_gesture_count,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain,
      AttributionResult* out_result);

  // Removes URLs in |out_referrer_chain| that match the Safe Browsing allowlist
  // domains.
  void RemoveSafeBrowsingAllowlistDomains(ReferrerChain* out_referrer_chain);

  // navigation_event_list_ keeps track of all the observed navigations. Since
  // the same url can be requested multiple times across different tabs and
  // frames, this list of NavigationEvents are ordered by navigation finish
  // time. Entries in navigation_event_list_ will be removed if they are older
  // than 2 minutes since their corresponding navigations finish or there are
  // more than `GetNavigationRecordMaxSize()` entries.
  NavigationEventList navigation_event_list_;

  // user_gesture_map_ keeps track of the timestamp of last user gesture in
  // in each WebContents. We assume for majority of cases, a navigation
  // shortly after a user gesture indicate this navigation is user initiated.
  UserGestureMap user_gesture_map_;

  // Host to timestamped IP addresses map that covers all the main frame and
  // subframe URLs' hosts. Since it is possible for a host to resolve to more
  // than one IP in even a short period of time, we map a single host to a
  // vector of ResolvedIPAddresss.
  HostToIpMap host_to_ip_map_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  base::OneShotTimer cleanup_timer_;

  std::optional<CopyPasteEntry> last_copy_paste_entry_;

  // A map of destination URLs to Push notification initiated navigation events.
  base::flat_map<GURL, std::unique_ptr<NavigationEvent>>
      notification_navigation_events_;

  // A reference to the ServiceWorkerContext that enables us to observe clicks
  // on Push notifications.
  //
  // |notification_context_| is expected to outlive the
  // SafeBrowsingNavigationObserverManager.
  //
  // SafeBrowsingNavigationObserverManager is owned by
  // SafeBrowsingNavigationObserverManagerFactory which listens for
  // BrowserContextDestroyed events which happen before the BrowserContext is
  // destroyed. (Note: the BrowserContext initiates ServiceWorkerContext
  // destruction via the StoragePartition.)
  raw_ptr<content::ServiceWorkerContext> notification_context_;
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
