// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_

#include <stdint.h>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/browser_apis/tab_strip/observation/tab_strip_api_batched_observer.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tabs/public/tab_group.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class Browser;
class MetricsReporter;
class Profile;

namespace tabs {
class TabInterface;
}

namespace tabs_api {
class TabStripService;
class TabStripServiceAggregator;
namespace mojom {
class Container;
using ContainerPtr = mojo::StructPtr<Container>;
}  // namespace mojom
}  // namespace tabs_api

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchCloseAction {
  kNoAction = 0,
  kTabSwitch = 1,
  kMaxValue = kTabSwitch,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchRecentlyClosedToggleAction {
  kExpand = 0,
  kCollapse = 1,
  kMaxValue = kCollapse,
};

class TabSearchPageHandler
    : public tab_search::mojom::PageHandler,
      public tabs_api::observation::TabStripApiBatchedObserver {
 public:
  TabSearchPageHandler(
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_search::mojom::Page> page,
      content::WebUI* web_ui,
      TopChromeWebUIController* webui_controller,
      MetricsReporter* metrics_reporter);
  TabSearchPageHandler(const TabSearchPageHandler&) = delete;
  TabSearchPageHandler& operator=(const TabSearchPageHandler&) = delete;
  ~TabSearchPageHandler() override;

  // tab_search::mojom::PageHandler:
  void CloseTab(int32_t tab_id) override;
  void CloseTabs(const std::vector<int32_t>& tab_ids) override;
  void CloseWebUiTab() override;
  void GetProfileData(GetProfileDataCallback callback) override;
  void GetIsSplit(GetIsSplitCallback callback) override;
  void SwitchToTab(
      tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) override;
  void OpenRecentlyClosedEntry(int32_t session_id) override;
  void ReplaceActiveSplitTab(int32_t replacement_tab_id) override;
  void SaveRecentlyClosedExpandedPref(bool expanded) override;
  void StartTabGroupTutorial() override;
  void MaybeShowUI() override;

  // TabStripApiBatchedObserver:
  void OnTabEvents(
      const std::vector<tabs_api::mojom::TabsEventPtr>& events) override;

  // Returns true if the WebContents hosting the WebUI is visible to the user
  // (in either a fully visible or partially occluded state).
  bool IsWebContentsVisible();

  void BeforeBubbleWidgetShowed();

  void disable_last_active_elapsed_text_for_testing() {
    disable_last_active_time_for_testing_ = true;
  }

  static constexpr int kMinRecentlyClosedItemDisplayCount = 8;

 protected:
  void SetTimerForTesting(std::unique_ptr<base::RetainingOneShotTimer> timer);

 private:
  // Used to determine if a specific tab should be included or not in the
  // results of GetProfileData. Tab url/group combinations that have been
  // previously added to the ProfileData will not be added more than once by
  // leveraging DedupKey comparisons.
  typedef std::tuple<GURL, std::optional<base::Token>> DedupKey;

  tab_search::mojom::ProfileDataPtr CreateProfileData();

  // Walk the tab strip tree to collect tab and group data.
  void WalkContainer(const tabs_api::mojom::ContainerPtr& container,
                     tab_search::mojom::Window* window,
                     tab_search::mojom::ProfileData* profile_data,
                     std::set<DedupKey>& tab_dedup_keys,
                     std::set<tab_groups::TabGroupId>& tab_group_ids);

  // Adds recently closed tabs, tab groups, and split views.
  void AddRecentlyClosedEntries(
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::vector<tab_search::mojom::RecentlyClosedTabGroupPtr>&
          recently_closed_tab_groups,
      std::vector<tab_search::mojom::RecentlyClosedSplitViewPtr>&
          recently_closed_split_views,
      std::set<tab_groups::TabGroupId>& tab_group_ids,
      std::vector<tab_search::mojom::TabGroupPtr>& tab_groups,
      std::set<DedupKey>& tab_dedup_keys);

  // Tries to add a recently closed tab to the profile data.
  // Returns true if a recently closed tab was added to `recently_closed_tabs`
  bool AddRecentlyClosedTab(
      sessions::tab_restore::Tab* tab,
      const base::Time& close_time,
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::set<DedupKey>& tab_dedup_keys,
      std::set<tab_groups::TabGroupId>& tab_group_ids,
      std::vector<tab_search::mojom::TabGroupPtr>& tab_groups);

  tab_search::mojom::TabPtr GetTab(tabs::TabInterface* tab) const;
  tab_search::mojom::RecentlyClosedTabPtr GetRecentlyClosedTab(
      sessions::tab_restore::Tab* tab,
      const base::Time& close_time);

  tabs_api::TabStripService* GetTabStripService(
      BrowserWindowInterface* browser) const;

  // Returns the tab associated with the given tab id.
  tabs::TabInterface* GetTabInterface(int32_t tab_id);

  // Handles updates to tab data and notifies the WebUI page if the change
  // is relevant.
  void OnTabDataChanged(const tabs_api::mojom::TabChange& event);

  // Handles the removal of nodes (tabs or tab collections).
  void OnNodesRemoved(const tabs_api::mojom::OnNodesClosedEventPtr& event);

  // Called by OnNodesRemoved to notify tab closures to the WebUI page.
  void OnTabsRemoved(std::vector<int> tab_ids,
                     std::set<SessionID> tab_restore_ids);
  // Called by OnNodesRemoved to notify the WebUI page that split tab is
  // removed from split view.
  void OnSplitTabRemoved();

  // Schedule a timer to call TabsChanged() when it times out
  // in order to reduce numbers of RPC.
  void ScheduleDebounce();

  // Call TabsChanged() and stop the timer if it's running.
  void NotifyTabsChanged();

  // Called when the browser window context for this WebUI has changed.
  void BrowserWindowInterfaceChanged();

  mojo::Receiver<tab_search::mojom::PageHandler> receiver_;
  mojo::Remote<tab_search::mojom::Page> page_;
  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<TopChromeWebUIController> webui_controller_;
  raw_ptr<Browser> browser_;
  const raw_ptr<MetricsReporter> metrics_reporter_;
  std::unique_ptr<base::RetainingOneShotTimer> debounce_timer_;
  PrefChangeRegistrar pref_change_registrar_;

  // Tracks how many times |CloseTab()| has been evoked for the currently open
  // instance of Tab Search for logging in UMA.
  int num_tabs_closed_ = 0;

  // Tracks whether or not we have sent the initial payload to the Tab Search
  // UI for metric collection purposes.
  bool sent_initial_payload_ = false;

  // Tracks whether the user has evoked |SwitchToTab()| for metric collection
  // purposes.
  bool called_switch_to_tab_ = false;

  bool disable_last_active_time_for_testing_ = false;

  // Notifies this when the browser window context changes.
  base::CallbackListSubscription browser_window_changed_subscription_;

  std::unique_ptr<tabs_api::TabStripServiceAggregator> aggregator_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
