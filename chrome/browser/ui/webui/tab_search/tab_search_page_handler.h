// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_

#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class Browser;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchCloseAction {
  kNoAction = 0,
  kTabSwitch = 1,
  kMaxValue = kTabSwitch,
};

class TabSearchPageHandler : public tab_search::mojom::PageHandler,
                             public TabStripModelObserver,
                             public BrowserTabStripTrackerDelegate,
                             public content::WebContentsObserver {
 public:
  TabSearchPageHandler(
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_search::mojom::Page> page,
      content::WebUI* web_ui,
      ui::MojoBubbleWebUIController* webui_controller);
  TabSearchPageHandler(const TabSearchPageHandler&) = delete;
  TabSearchPageHandler& operator=(const TabSearchPageHandler&) = delete;
  ~TabSearchPageHandler() override;

  // tab_search::mojom::PageHandler:
  void CloseTab(int32_t tab_id) override;
  void GetProfileData(GetProfileDataCallback callback) override;
  void GetTabGroups(GetTabGroupsCallback callback) override;
  void SwitchToTab(
      tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) override;
  void OpenRecentlyClosedTab(int32_t tab_id) override;
  void ShowUI() override;
  // TODO(tluk): Remove this once all uses of the CloseUI() interface are
  // removed from the Tab Search WebUI code.
  void CloseUI() override {}

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

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 protected:
  void SetTimerForTesting(std::unique_ptr<base::RetainingOneShotTimer> timer);

 private:
  // Encapsulates tab details to facilitate performing an action on a tab.
  struct TabDetails {
    TabDetails(Browser* browser, TabStripModel* tab_strip_model, int index)
        : browser(browser), tab_strip_model(tab_strip_model), index(index) {}

    Browser* browser;
    TabStripModel* tab_strip_model;
    int index;
  };

  tab_search::mojom::ProfileDataPtr CreateProfileData();

  // Adds recently closed tabs in a flattened list.
  void AddRecentlyClosedTabs(
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::set<std::string>& tab_urls);

  // Tries to add a single recently closed tab to a flattened list. Returns
  // whether the the tab fits within |max_tab_count|.
  bool AddRecentlyClosedTab(
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      sessions::TabRestoreService::Tab* tab,
      std::set<std::string>& tab_urls,
      size_t max_tab_count);

  tab_search::mojom::TabPtr GetTab(TabStripModel* tab_strip_model,
                                   content::WebContents* contents,
                                   int index);
  tab_search::mojom::RecentlyClosedTabPtr GetRecentlyClosedTab(
      sessions::TabRestoreService::Tab* tab);

  // Returns tab details required to perform an action on the tab.
  base::Optional<TabDetails> GetTabDetails(int32_t tab_id);

  // Schedule a timer to call TabsChanged() when it times out
  // in order to reduce numbers of RPC.
  void ScheduleDebounce();

  // Call TabsChanged() and stop the timer if it's running.
  void NotifyTabsChanged();

  mojo::Receiver<tab_search::mojom::PageHandler> receiver_;
  mojo::Remote<tab_search::mojom::Page> page_;
  content::WebUI* const web_ui_;
  ui::MojoBubbleWebUIController* const webui_controller_;
  BrowserTabStripTracker browser_tab_strip_tracker_{this, this};
  std::unique_ptr<base::RetainingOneShotTimer> debounce_timer_;
  bool webui_hidden_ = false;

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
