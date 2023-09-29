// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "components/sessions/core/tab_restore_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "url/gurl.h"

class Browser;
class MetricsReporter;

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

class TabSearchPageHandler : public tab_search::mojom::PageHandler,
                             public TabStripModelObserver,
                             public BrowserTabStripTrackerDelegate {
 public:
  TabSearchPageHandler(
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_search::mojom::Page> page,
      content::WebUI* web_ui,
      ui::MojoBubbleWebUIController* webui_controller,
      MetricsReporter* metrics_reporter);
  TabSearchPageHandler(const TabSearchPageHandler&) = delete;
  TabSearchPageHandler& operator=(const TabSearchPageHandler&) = delete;
  ~TabSearchPageHandler() override;

  // tab_search::mojom::PageHandler:
  void CloseTab(int32_t tab_id) override;
  void GetProfileData(GetProfileDataCallback callback) override;
  void SwitchToTab(
      tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) override;
  void OpenRecentlyClosedEntry(int32_t session_id) override;
  void RequestTabOrganization(RequestTabOrganizationCallback callback) override;
  void SaveRecentlyClosedExpandedPref(bool expanded) override;
  void ShowUI() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // Returns true if the WebContents hosting the WebUI is visible to the user
  // (in either a fully visible or partially occluded state).
  bool IsWebContentsVisible();

 protected:
  void SetTimerForTesting(std::unique_ptr<base::RetainingOneShotTimer> timer);

 private:
  // Used to determine if a specific tab should be included or not in the
  // results of GetProfileData. Tab url/group combinations that have been
  // previously added to the ProfileData will not be added more than once by
  // leveraging DedupKey comparisons.
  typedef std::tuple<GURL, absl::optional<base::Token>> DedupKey;

  // Encapsulates tab details to facilitate performing an action on a tab.
  struct TabDetails {
    TabDetails(Browser* browser, TabStripModel* tab_strip_model, int index)
        : browser(browser), tab_strip_model(tab_strip_model), index(index) {}

    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION Browser* browser;
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION TabStripModel* tab_strip_model;
    int index;
  };

  tab_search::mojom::ProfileDataPtr CreateProfileData();

  // Adds recently closed tabs and tab groups.
  void AddRecentlyClosedEntries(
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::vector<tab_search::mojom::RecentlyClosedTabGroupPtr>&
          recently_closed_tab_groups,
      std::set<tab_groups::TabGroupId>& tab_group_ids,
      std::vector<tab_search::mojom::TabGroupPtr>& tab_groups,
      std::set<DedupKey>& tab_dedup_keys);

  // Tries to add a recently closed tab to the profile data.
  // Returns true if a recently closed tab was added to `recently_closed_tabs`
  bool AddRecentlyClosedTab(
      sessions::TabRestoreService::Tab* tab,
      const base::Time& close_time,
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::set<DedupKey>& tab_dedup_keys,
      std::set<tab_groups::TabGroupId>& tab_group_ids,
      std::vector<tab_search::mojom::TabGroupPtr>& tab_groups);

  tab_search::mojom::TabPtr GetTab(TabStripModel* tab_strip_model,
                                   content::WebContents* contents,
                                   int index);
  tab_search::mojom::RecentlyClosedTabPtr GetRecentlyClosedTab(
      sessions::TabRestoreService::Tab* tab,
      const base::Time& close_time);

  // Returns tab details required to perform an action on the tab.
  absl::optional<TabDetails> GetTabDetails(int32_t tab_id);

  // Schedule a timer to call TabsChanged() when it times out
  // in order to reduce numbers of RPC.
  void ScheduleDebounce();

  // Call TabsChanged() and stop the timer if it's running.
  void NotifyTabsChanged();

  mojo::Receiver<tab_search::mojom::PageHandler> receiver_;
  mojo::Remote<tab_search::mojom::Page> page_;
  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<ui::MojoBubbleWebUIController, DanglingUntriaged>
      webui_controller_;
  const raw_ptr<MetricsReporter> metrics_reporter_;
  BrowserTabStripTracker browser_tab_strip_tracker_{this, this};
  std::unique_ptr<base::RetainingOneShotTimer> debounce_timer_;

  // Tracks how many times |CloseTab()| has been evoked for the currently open
  // instance of Tab Search for logging in UMA.
  int num_tabs_closed_ = 0;

  // Tracks whether or not we have sent the initial payload to the Tab Search
  // UI for metric collection purposes.
  bool sent_initial_payload_ = false;

  // Tracks whether the user has evoked |SwitchToTab()| for metric collection
  // purposes.
  bool called_switch_to_tab_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
