// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_

#include <stdint.h>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
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
class TabOrganizationService;
class OptimizationGuideKeyedService;

namespace tabs {
class TabDeclutterController;
}

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

class DuplicateTabsObserver : public content::WebContentsObserver {
 public:
  DuplicateTabsObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void()> on_url_changed_callback);
  ~DuplicateTabsObserver() override;

  void PrimaryPageChanged(content::Page& page) override;

 private:
  base::RepeatingCallback<void()> on_url_changed_callback_;
};

class TabSearchPageHandler
    : public tab_search::mojom::PageHandler,
      public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate,
      public TabOrganizationSession::Observer,
      public TabOrganizationObserver,
      public TabDeclutterObserver,
      public optimization_guide::SettingsEnabledObserver {
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
  void DeclutterTabs(const std::vector<int32_t>& tab_ids,
                     const std::vector<GURL>& urls) override;
  void AcceptTabOrganization(
      int32_t session_id,
      int32_t organization_id,
      std::vector<tab_search::mojom::TabPtr> tabs) override;
  void RejectTabOrganization(int32_t session_id,
                             int32_t organization_id) override;
  void RenameTabOrganization(int32_t session_id,
                             int32_t organization_id,
                             const std::u16string& name) override;
  void ExcludeFromStaleTabs(int32_t tab_id) override;
  void ExcludeFromDuplicateTabs(const GURL& url) override;
  void GetProfileData(GetProfileDataCallback callback) override;
  void GetUnusedTabs(GetUnusedTabsCallback callback) override;
  void GetTabSearchSection(GetTabSearchSectionCallback callback) override;
  void GetTabOrganizationFeature(
      GetTabOrganizationFeatureCallback callback) override;
  void GetTabOrganizationSession(
      GetTabOrganizationSessionCallback callback) override;
  void GetTabOrganizationModelStrategy(
      GetTabOrganizationModelStrategyCallback callback) override;
  void GetIsSplit(GetIsSplitCallback callback) override;
  void SwitchToTab(
      tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) override;
  void OpenRecentlyClosedEntry(int32_t session_id) override;
  void RequestTabOrganization() override;
  void RemoveTabFromOrganization(int32_t session_id,
                                 int32_t organization_id,
                                 tab_search::mojom::TabPtr tab) override;
  void RejectSession(int32_t session_id) override;
  void ReplaceActiveSplitTab(int32_t replacement_tab_id) override;
  void RestartSession() override;
  void SaveRecentlyClosedExpandedPref(bool expanded) override;
  void SetOrganizationFeature(
      tab_search::mojom::TabOrganizationFeature feature) override;
  void StartTabGroupTutorial() override;
  void TriggerFeedback(int32_t session_id) override;
  void TriggerSignIn() override;
  void OpenHelpPage() override;
  void SetTabOrganizationModelStrategy(
      tab_search::mojom::TabOrganizationModelStrategy strategy) override;
  void SetTabOrganizationUserInstruction(
      const std::string& user_instruction) override;
  void SetUserFeedback(int32_t session_id,
                       tab_search::mojom::UserFeedback feedback) override;
  void NotifyOrganizationUIReadyToShow() override;
  void NotifySearchUIReadyToShow() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // TabDeclutterObserver:
  void OnUnusedTabsProcessed(
      std::vector<tabs::TabInterface*> stale_tabs,
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // Returns true if the WebContents hosting the WebUI is visible to the user
  // (in either a fully visible or partially occluded state).
  bool IsWebContentsVisible();

  // Convert TabOrganizations data to mojo serialized objects.
  tab_search::mojom::TabPtr GetMojoForTabData(TabData* tab_data) const;
  tab_search::mojom::TabOrganizationPtr GetMojoForTabOrganization(
      const TabOrganization* organization) const;
  tab_search::mojom::TabOrganizationSessionPtr GetMojoForTabOrganizationSession(
      const TabOrganizationSession* session) const;

  // TabOrganizationSession::Observer
  void OnTabOrganizationSessionUpdated(
      const TabOrganizationSession* session) override;
  void OnTabOrganizationSessionDestroyed(
      TabOrganizationSession::ID session_id) override;

  // TabOrganizationObserver
  void OnSessionCreated(const Browser* browser,
                        TabOrganizationSession* session) override;

  // SettingsEnabledObserver
  void OnChangeInFeatureCurrentlyEnabledState(bool is_now_enabled) override;

  void disable_last_active_elapsed_text_for_testing() {
    disable_last_active_time_for_testing_ = true;
  }

  std::vector<tabs::TabInterface*> stale_tabs_for_testing() {
    return stale_tabs_;
  }

  std::map<GURL, std::vector<tabs::TabInterface*>>
  duplicate_tabs_for_testing() {
    return duplicate_tabs_;
  }

  void SetTabDeclutterControllerForTesting(
      tabs::TabDeclutterController* tab_declutter_controller);

  static constexpr int kMinRecentlyClosedItemDisplayCount = 8;

 protected:
  void SetTimerForTesting(std::unique_ptr<base::RetainingOneShotTimer> timer);

 private:
  // Used to determine if a specific tab should be included or not in the
  // results of GetProfileData. Tab url/group combinations that have been
  // previously added to the ProfileData will not be added more than once by
  // leveraging DedupKey comparisons.
  typedef std::tuple<GURL, std::optional<base::Token>> DedupKey;

  enum class UnusedTabType { kInactive, kDuplicate };

  // Encapsulates tab details to facilitate performing an action on a tab.
  struct TabDetails {
    explicit TabDetails(tabs::TabInterface* tab) : tab(tab) {}

    int GetIndex() const {
      return tab->GetBrowserWindowInterface()
          ->GetTabStripModel()
          ->GetIndexOfTab(tab);
    }

    raw_ptr<tabs::TabInterface> tab;
  };

  // Show the UI if all tabs are ready to be shown.
  void MaybeShowUI();

  tab_search::mojom::ProfileDataPtr CreateProfileData();
  void UpdateUnusedTabs();

  void SetTabDeclutterController(
      tabs::TabDeclutterController* tab_declutter_controller);

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
      sessions::tab_restore::Tab* tab,
      const base::Time& close_time,
      std::vector<tab_search::mojom::RecentlyClosedTabPtr>&
          recently_closed_tabs,
      std::set<DedupKey>& tab_dedup_keys,
      std::set<tab_groups::TabGroupId>& tab_group_ids,
      std::vector<tab_search::mojom::TabGroupPtr>& tab_groups);

  tab_search::mojom::TabPtr GetTab(
      const TabStripModel* tab_strip_model,
      content::WebContents* contents,
      int index,
      std::string custom_last_active_text = "") const;
  tab_search::mojom::RecentlyClosedTabPtr GetRecentlyClosedTab(
      sessions::tab_restore::Tab* tab,
      const base::Time& close_time);

  // Returns tab details required to perform an action on the tab.
  std::optional<TabDetails> GetTabDetails(int32_t tab_id);

  // Schedule a timer to call TabsChanged() when it times out
  // in order to reduce numbers of RPC.
  void ScheduleDebounce();

  // Call TabsChanged() and stop the timer if it's running.
  void NotifyTabsChanged();

  void NotifyTabIndexPrefChanged(const Profile* profile);

  void NotifyOrganizationFeaturePrefChanged(const Profile* profile);

  void NotifyShowFREPrefChanged(const Profile* profile);

  mojo::StructPtr<tab_search::mojom::UnusedTabInfo> GetMojoUnusedTabs();
  std::vector<mojo::StructPtr<tab_search::mojom::Tab>> GetMojoStaleTabs();
  base::flat_map<std::string,
                 std::vector<mojo::StructPtr<tab_search::mojom::Tab>>>
  GetMojoDuplicateTabs();

  void UnregisterTabCallbacks();
  void RegisterInactiveTabDeclutterCallbacks(tabs::TabInterface* tab);
  void RegisterDuplicateTabDeclutterCallbacks(tabs::TabInterface* tab);

  void OnStaleTabDidEnterForeground(tabs::TabInterface* tab);
  void OnDuplicateTabWillDiscardWebContents(tabs::TabInterface* tab,
                                            content::WebContents* old_content,
                                            content::WebContents* new_content);

  void OnUnusedTabWillDetach(tabs::TabInterface* tab,
                             tabs::TabInterface::DetachReason reason,
                             UnusedTabType type);
  void OnUnusedTabPinnedStateChanged(tabs::TabInterface* tab,
                                     bool new_pinned_state,
                                     UnusedTabType type);
  void OnUnusedTabGroupChanged(tabs::TabInterface* tab,
                               std::optional<tab_groups::TabGroupId> new_group,
                               UnusedTabType type);

  void RemoveStaleTab(tabs::TabInterface* tab);

  // Removes a tab from the duplicate tab list, along with its associated
  // subscriptions and observations. If the duplicate list for the tab's URL
  // contains only one remaining tab after removal, that tab is also removed,
  // and the list is erased from the map. If the tab is not found, the method
  // exits without performing any action.
  void RemoveDuplicateTab(tabs::TabInterface* tab);

  // Called when the browser window context for this WebUI has changed.
  void BrowserWindowInterfaceChanged();

  std::vector<tabs::TabInterface*> FilterDuplicateTabsFromStaleTabs(
      std::vector<tabs::TabInterface*> stale_tabs,
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs);

  mojo::Receiver<tab_search::mojom::PageHandler> receiver_;
  mojo::Remote<tab_search::mojom::Page> page_;
  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<TopChromeWebUIController, DanglingUntriaged> webui_controller_;
  raw_ptr<Browser> browser_;
  const raw_ptr<MetricsReporter> metrics_reporter_;
  BrowserTabStripTracker browser_tab_strip_tracker_{this, this};
  std::unique_ptr<base::RetainingOneShotTimer> debounce_timer_;
  raw_ptr<TabOrganizationService> organization_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<tabs::TabDeclutterController> tab_declutter_controller_;

  // Tracks how many times |CloseTab()| has been evoked for the currently open
  // instance of Tab Search for logging in UMA.
  int num_tabs_closed_ = 0;

  // Tracks whether or not we have sent the initial payload to the Tab Search
  // UI for metric collection purposes.
  bool sent_initial_payload_ = false;

  // Tracks whether the user has evoked |SwitchToTab()| for metric collection
  // purposes.
  bool called_switch_to_tab_ = false;

  // Tracks whether a session restart is currently in progress.
  bool restarting_ = false;

  // Tracks whether each tab within the UI is ready to be shown. The bubble
  // will only be shown once all tabs are ready.
  bool organization_ready_to_show_ = false;
  bool search_ready_to_show_ = false;

  bool disable_last_active_time_for_testing_ = false;

  // Notifies this when the browser window context changes.
  base::CallbackListSubscription browser_window_changed_subscription_;

  // Listened TabOrganization sessions.
  std::vector<raw_ptr<TabOrganizationSession, VectorExperimental>>
      listened_sessions_;

  std::vector<tabs::TabInterface*> stale_tabs_;
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs_;

  std::map<tabs::TabInterface*, std::vector<base::CallbackListSubscription>>
      inactive_tab_subscriptions_map_;

  std::map<tabs::TabInterface*, std::vector<base::CallbackListSubscription>>
      duplicate_tab_subscriptions_map_;

  std::map<tabs::TabInterface*, std::unique_ptr<DuplicateTabsObserver>>
      duplicate_tab_webcontents_observers_;

  base::ScopedObservation<TabOrganizationService, TabOrganizationObserver>
      tab_organization_observation_{this};

  base::ScopedObservation<tabs::TabDeclutterController, TabDeclutterObserver>
      tab_declutter_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
