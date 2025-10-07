// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class TabModelObserver;

// TestTabModel is a thin TabModel that can be used for testing Android tab
// behavior. It holds pointers to WebContents that are owned elsewhere. Many
// functions are unimplemented. Use OwningTestTabModel for a more complete
// TabModel implementation.
// TODO(crbug.com/415309796): Convert tests using this to OwningTestTabModel and
// remove it.
class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile,
                        chrome::android::ActivityType activity_type =
                            chrome::android::ActivityType::kTabbed);
  ~TestTabModel() override;

  // TabModel:
  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer) override;
  void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) override;
  // Returns tab_count_ if not 0. Otherwise, returns size of web_contents_list_.
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  tabs::TabInterface* GetActiveTab() override;
  content::WebContents* GetWebContentsAt(int index) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;
  content::WebContents* CreateNewTabForDevTools(const GURL& url,
                                                bool new_window) override;
  bool IsSessionRestoreInProgress() const override;

  bool IsActiveModel() const override;
  void SetIsActiveModel(bool is_active);

  TabAndroid* GetTabAt(int index) const override;
  void SetActiveIndex(int index) override;
  void ForceCloseAllTabs() override;
  void CloseTabAt(int index) override;
  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  TabModelObserver* GetObserver();
  void SetTabCount(int tab_count);
  void SetWebContentsList(
      const std::vector<raw_ptr<content::WebContents>>& web_contents_list);
  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override;
  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override;

  // TODO(crbug.com/415351293): Implement these.
  // TabListInterface implementation.
  tabs::TabInterface* OpenTab(const GURL& url, int index) override;
  void DiscardTab(tabs::TabHandle tab) override;
  tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) override;
  tabs::TabInterface* GetTab(int index) override;
  int GetIndexOfTab(tabs::TabHandle tab) override;
  void HighlightTabs(tabs::TabHandle tab_to_activate,
                     const std::set<tabs::TabHandle>& tabs) override;
  void MoveTab(tabs::TabHandle tab, int index) override;
  void CloseTab(tabs::TabHandle tab) override;
  std::vector<tabs::TabInterface*> GetAllTabs() override;
  void PinTab(tabs::TabHandle tab) override;
  void UnpinTab(tabs::TabHandle tab) override;
  std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) override;
  void Ungroup(const std::set<tabs::TabHandle>& tabs) override;
  void MoveGroupTo(tab_groups::TabGroupId group_id, int index) override;
  void MoveTabToWindow(tabs::TabHandle tab,
                       SessionID destination_window_id,
                       int destination_index) override;
  void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                            SessionID destination_window_id,
                            int destination_index) override;

// BrowserWindowInterface is available on desktop Android, but not other Android
// builds.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  void AssociateWithBrowserWindow(BrowserWindowInterface* browser_window);
#endif

 private:
  // A fake value for the current number of tabs.
  int tab_count_ = 0;
  bool is_active_ = false;

  raw_ptr<TabModelObserver> observer_ = nullptr;
  std::vector<raw_ptr<content::WebContents>> web_contents_list_;

  std::unique_ptr<ui::ScopedUnownedUserData<TabModel>>
      scoped_unowned_user_data_;
};

// A TabModel that owns the WebContents for each tab and simulates many of the
// operations that are provided by the JNI bridge in the production TabModel.
class OwningTestTabModel : public TabModel {
 public:
  // Creates a TabModel that starts empty. The model will automatically be added
  // to the TabModelList, and removed when it's destroyed.
  explicit OwningTestTabModel(Profile* profile,
                              chrome::android::ActivityType activity_type =
                                  chrome::android::ActivityType::kTabbed);

  ~OwningTestTabModel() override;

  OwningTestTabModel(const OwningTestTabModel&) = delete;
  OwningTestTabModel& operator=(const OwningTestTabModel&) = delete;

  // TabModel:

  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer) override;
  void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  tabs::TabInterface* GetActiveTab() override;
  content::WebContents* GetWebContentsAt(int index) const override;
  TabAndroid* GetTabAt(int index) const override;
  void SetActiveIndex(int index) override;
  void ForceCloseAllTabs() override;
  void CloseTabAt(int index) override;
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override;
  bool IsActiveModel() const override;
  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  // Unimplemented methods.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;
  content::WebContents* CreateNewTabForDevTools(const GURL& url,
                                                bool new_window) override;
  bool IsSessionRestoreInProgress() const override;
  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override;
  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override;

  // Test accessors:

  // Adds a new tab containing an empty `web_contents` at `index`. If `select`
  // is true the new tab will become active.
  TabAndroid* AddEmptyTab(size_t index,
                          bool select = false,
                          TabModel::TabLaunchType launch_type =
                              TabModel::TabLaunchType::FROM_CHROME_UI);

  // Adds a new tab containing `web_contents` at `index`. If `select` is true
  // the new tab will become active.
  TabAndroid* AddTabFromWebContents(
      std::unique_ptr<content::WebContents> web_contents,
      size_t index,
      bool select = false,
      TabModel::TabLaunchType launch_type =
          TabModel::TabLaunchType::FROM_CHROME_UI);

  void SetIsActiveModel(bool is_active);

  // TODO(crbug.com/415351293): Implement these.
  // TabListInterface implementation.
  tabs::TabInterface* OpenTab(const GURL& url, int index) override;
  void DiscardTab(tabs::TabHandle tab) override;
  tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) override;
  tabs::TabInterface* GetTab(int index) override;
  int GetIndexOfTab(tabs::TabHandle tab) override;
  void HighlightTabs(tabs::TabHandle tab_to_activate,
                     const std::set<tabs::TabHandle>& tabs) override;
  void MoveTab(tabs::TabHandle tab, int index) override;
  void CloseTab(tabs::TabHandle tab) override;
  std::vector<tabs::TabInterface*> GetAllTabs() override;
  void PinTab(tabs::TabHandle tab) override;
  void UnpinTab(tabs::TabHandle tab) override;
  std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) override;
  void Ungroup(const std::set<tabs::TabHandle>& tabs) override;
  void MoveGroupTo(tab_groups::TabGroupId group_id, int index) override;
  void MoveTabToWindow(tabs::TabHandle tab,
                       SessionID destination_window_id,
                       int destination_index) override;
  void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                            SessionID destination_window_id,
                            int destination_index) override;

 private:
  void SelectTab(TabAndroid* tab, TabModel::TabSelectionType selection_type);

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<TabModelObserver>::Unchecked observer_list_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::vector<std::unique_ptr<TabAndroid>> owned_tabs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<TabAndroid> active_tab_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  bool is_active_model_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  int next_tab_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
};

// Waits until a TabAndroid has a WebContents assigned that's finished loading.
class TabAndroidLoadedWaiter final : public TabAndroid::Observer {
 public:
  explicit TabAndroidLoadedWaiter(TabAndroid* tab);
  ~TabAndroidLoadedWaiter() final;

  TabAndroidLoadedWaiter(const TabAndroidLoadedWaiter&) = delete;
  TabAndroidLoadedWaiter& operator=(const TabAndroidLoadedWaiter&) = delete;

  bool Wait();

  // TabAndroid::Observer:
  void OnInitWebContents(TabAndroid* tab) final;

 private:
  content::WaiterHelper waiter_helper_;
  bool load_succeeded_ = false;
  base::ScopedObservation<TabAndroid, TabAndroid::Observer> tab_observation_{
      this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_
