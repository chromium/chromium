// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"

#include <jni.h>

#include <cstddef>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/android_buildflags.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

// "chrome/browser/ui/browser_window" is available on desktop Android, but not
// other Android builds.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#endif

TestTabModel::TestTabModel(Profile* profile,
                           chrome::android::ActivityType activity_type,
                           TabModelType tab_model_type)
    : TabModel(profile, activity_type, std::nullopt, tab_model_type) {}

TestTabModel::~TestTabModel() = default;

void TestTabModel::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  NOTIMPLEMENTED();
}

void TestTabModel::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  NOTIMPLEMENTED();
}

int TestTabModel::GetTabCount() const {
  return tab_count_ != 0 ? tab_count_
                         : static_cast<int>(web_contents_list_.size());
}

int TestTabModel::GetActiveIndex() const {
  return 0;
}

tabs::TabInterface* TestTabModel::GetActiveTab() {
  return nullptr;
}

content::WebContents* TestTabModel::GetWebContentsAt(int index) const {
  if (index < static_cast<int>(web_contents_list_.size())) {
    return web_contents_list_[index];
  }
  return nullptr;
}

base::android::ScopedJavaLocalRef<jobject> TestTabModel::GetJavaObject() const {
  return nullptr;
}

tabs::TabInterface* TestTabModel::CreateTab(
    TabAndroid* parent,
    std::unique_ptr<content::WebContents> web_contents,
    int index,
    TestTabModel::TabLaunchType type,
    bool should_pin) {
  return nullptr;
}

void TestTabModel::HandlePopupNavigation(TabAndroid* parent,
                                         NavigateParams* params) {}

content::WebContents* TestTabModel::CreateNewTabForDevTools(const GURL& url,
                                                            bool new_window) {
  return nullptr;
}

bool TestTabModel::IsSessionRestoreInProgress() const {
  return false;
}

bool TestTabModel::IsActiveModel() const {
  return is_active_;
}

void TestTabModel::SetIsActiveModel(bool is_active) {
  is_active_ = is_active;
}

TabAndroid* TestTabModel::GetTabAt(int index) const {
  return nullptr;
}

bool TestTabModel::HasTab(TabAndroid* tab) const {
  return false;
}

std::vector<tabs::TabHandle> TestTabModel::GetOrderedMultiSelectedTabs() const {
  NOTIMPLEMENTED();
  return {};
}

void TestTabModel::SetActiveIndex(int index) {}

void TestTabModel::ForceCloseAllTabs() {}

void TestTabModel::CloseTabAt(int index) {}

std::unique_ptr<content::WebContents> TestTabModel::DetachWebContents(
    tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestTabModel::AddObserver(TabModelObserver* observer) {
  observer_ = observer;
}

void TestTabModel::RemoveObserver(TabModelObserver* observer) {
  if (observer == observer_) {
    observer_ = nullptr;
  }
}

TabModelObserver* TestTabModel::GetObserver() {
  return observer_;
}

void TestTabModel::SetTabCount(int tab_count) {
  tab_count_ = tab_count;
}

void TestTabModel::SetWebContentsList(
    const std::vector<raw_ptr<content::WebContents>>& web_contents_list) {
  web_contents_list_ = web_contents_list;
}

int TestTabModel::GetTabCountNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) const {
  return 0;
}

void TestTabModel::CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                                  const base::Time& end_time) {}

tabs::TabStripCollection* TestTabModel::GetTabStripCollection(
    base::PassKey<tabs_api::AndroidTabStripModelAdapter>) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestTabModel::ActivateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* TestTabModel::OpenTab(const GURL& url,
                                          int index,
                                          bool foreground) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestTabModel::SetOpenerForTab(tabs::TabHandle target,
                                   tabs::TabHandle opener) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* TestTabModel::GetOpenerForTab(tabs::TabHandle target) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* TestTabModel::InsertWebContentsAt(
    int index,
    std::unique_ptr<content::WebContents> web_contents,
    bool should_pin,
    std::optional<tab_groups::TabGroupId> group) {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* TestTabModel::DiscardTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* TestTabModel::DuplicateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* TestTabModel::GetTab(int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

int TestTabModel::GetIndexOfTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return -1;
}

void TestTabModel::HighlightTabs(tabs::TabHandle tab_to_activate,
                                 const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
}

void TestTabModel::MoveTab(tabs::TabHandle tab, int index) {
  NOTIMPLEMENTED();
}

void TestTabModel::CloseTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

std::vector<tabs::TabInterface*> TestTabModel::GetAllTabs() {
  NOTIMPLEMENTED();
  return {};
}

void TestTabModel::PinTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

void TestTabModel::UnpinTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

bool TestTabModel::ContainsTabGroup(tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<tab_groups::TabGroupId> TestTabModel::ListTabGroups() {
  NOTIMPLEMENTED();
  return {};
}

std::optional<tab_groups::TabGroupVisualData>
TestTabModel::GetTabGroupVisualData(tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

gfx::Range TestTabModel::GetTabGroupTabIndices(
    tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return {};
}

std::optional<tab_groups::TabGroupId> TestTabModel::CreateTabGroup(
    const std::vector<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<split_tabs::SplitTabId> TestTabModel::CreateSplit(
    const std::vector<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void TestTabModel::SetTabGroupVisualData(
    tab_groups::TabGroupId group_id,
    const tab_groups::TabGroupVisualData& visual_data) {
  NOTIMPLEMENTED();
}

std::optional<tab_groups::TabGroupId> TestTabModel::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void TestTabModel::Ungroup(const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
}

void TestTabModel::Unsplit(split_tabs::SplitTabId split_id) {
  NOTIMPLEMENTED();
}

void TestTabModel::MoveGroupTo(tab_groups::TabGroupId group_id, int index) {
  NOTIMPLEMENTED();
}

void TestTabModel::MoveTabToWindow(tabs::TabHandle tab,
                                   SessionID destination_window_id,
                                   int destination_index) {
  NOTIMPLEMENTED();
}

bool TestTabModel::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                        SessionID destination_window_id,
                                        int destination_index) {
  NOTIMPLEMENTED();
  return false;
}

bool TestTabModel::IsThisTabListEditable() {
  NOTIMPLEMENTED();
  return true;
}

bool TestTabModel::IsClosingAllTabs() {
  NOTIMPLEMENTED();
  return false;
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
void TestTabModel::AssociateWithBrowserWindow(BrowserWindowInterface* browser) {
  scoped_unowned_user_data_ =
      std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
          browser->GetUnownedUserDataHost(), *this);
}
#endif

OwningTestTabModel::OwningTestTabModel(
    Profile* profile,
    chrome::android::ActivityType activity_type,
    TabModelType tab_model_type)
    : TabModel(profile, activity_type, std::nullopt, tab_model_type) {
  TabModelList::AddTabModel(this);
}

OwningTestTabModel::~OwningTestTabModel() {
  observer_list_.Notify(&TabModelObserver::OnTabModelDestroyed,
                        std::ref(*this));
  ForceCloseAllTabs();
  TabModelList::RemoveTabModel(this);
}

void OwningTestTabModel::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  NOTIMPLEMENTED();
}

int OwningTestTabModel::GetTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return owned_tabs_.size();
}

int OwningTestTabModel::GetActiveIndex() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!active_tab_) {
    return -1;
  }
  for (size_t index = 0; index < owned_tabs_.size(); ++index) {
    if (owned_tabs_.at(index).get() == active_tab_.get()) {
      return index;
    }
  }
  NOTREACHED();
}

tabs::TabInterface* OwningTestTabModel::GetActiveTab() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_tab_.get();
}

std::vector<tabs::TabHandle> OwningTestTabModel::GetOrderedMultiSelectedTabs()
    const {
  NOTIMPLEMENTED();
  return {};
}

content::WebContents* OwningTestTabModel::GetWebContentsAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetTabAt(index)->web_contents();
}

TabAndroid* OwningTestTabModel::GetTabAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return owned_tabs_.at(index).get();
}

bool OwningTestTabModel::HasTab(TabAndroid* tab) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& owned_tab : owned_tabs_) {
    if (owned_tab.get() == tab) {
      return true;
    }
  }
  return false;
}

void OwningTestTabModel::SetActiveIndex(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SelectTab(GetTabAt(index), TabModel::TabSelectionType::FROM_USER);
}

void OwningTestTabModel::ForceCloseAllTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<int> indices(GetTabCount());
  std::iota(indices.begin(), indices.end(), 0);
  CloseTabsAt(indices);
}

void OwningTestTabModel::CloseTabAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseTabsAt({index});
}

void OwningTestTabModel::CloseTabsAt(const std::vector<int>& indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (indices.empty()) {
    return;
  }

  std::set<int> index_set(indices.begin(), indices.end());
  std::vector<TabAndroid*> tabs_to_close;
  tabs_to_close.reserve(index_set.size());
  for (int index : index_set) {
    CHECK_GE(index, 0);
    CHECK_LT(static_cast<size_t>(index), owned_tabs_.size());
    tabs_to_close.push_back(owned_tabs_[index].get());
  }

  bool is_all_tabs = (tabs_to_close.size() == owned_tabs_.size());

  if (base::FeatureList::IsEnabled(
          chrome::android::kTabClosureMethodRefactor)) {
    observer_list_.Notify(&TabModelObserver::WillCloseTabs, tabs_to_close,
                          is_all_tabs, /*allow_undo=*/false);
  } else {
    for (auto* tab : tabs_to_close) {
      observer_list_.Notify(&TabModelObserver::WillCloseTab, tab);
    }
    if (is_all_tabs) {
      observer_list_.Notify(&TabModelObserver::AllTabsAreClosing);
    }
  }

  int active_index = GetActiveIndex();
  bool active_tab_is_closing =
      active_index != -1 && index_set.contains(active_index);

  if (active_tab_is_closing) {
    TabAndroid* new_active_tab = nullptr;
    if (!is_all_tabs) {
      // 1. Try to find the nearest non-closing tab to the right.
      for (int i = active_index + 1; i < GetTabCount(); ++i) {
        if (!index_set.contains(i)) {
          new_active_tab = owned_tabs_[i].get();
          break;
        }
      }
      // 2. If no non-closing tab to the right, find the nearest non-closing tab
      // to the left.
      if (!new_active_tab) {
        for (int i = active_index - 1; i >= 0; --i) {
          if (!index_set.contains(i)) {
            new_active_tab = owned_tabs_[i].get();
            break;
          }
        }
      }
    }
    SelectTab(new_active_tab, TabModel::TabSelectionType::FROM_CLOSE);
  }

  std::vector<std::unique_ptr<TabAndroid>> removed_tabs;
  removed_tabs.reserve(index_set.size());

  auto keep_it = owned_tabs_.begin();
  for (size_t i = 0; i < owned_tabs_.size(); ++i) {
    if (index_set.contains(static_cast<int>(i))) {
      removed_tabs.push_back(std::move(owned_tabs_[i]));
    } else {
      // Shift remaining items to the front of the existing vector
      *keep_it++ = std::move(owned_tabs_[i]);
    }
  }
  // Erase the leftover moved-from elements at the tail end
  owned_tabs_.erase(keep_it, owned_tabs_.end());

  for (const auto& tab : removed_tabs) {
    observer_list_.Notify(&TabModelObserver::TabRemoved, tab.get());
  }
}

std::unique_ptr<content::WebContents> OwningTestTabModel::DetachWebContents(
    tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* OwningTestTabModel::CreateTab(
    TabAndroid* parent,
    std::unique_ptr<content::WebContents> web_contents,
    int index,
    TestTabModel::TabLaunchType type,
    bool should_pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t insertion_index =
      (index == TabModel::kInvalidIndex) ? owned_tabs_.size() : index;

  bool is_new_tab_incognito =
      web_contents->GetBrowserContext()->IsOffTheRecord();
  bool select_tab = TabModelJniBridge::IsTabLaunchedInForeground(
      type, is_new_tab_incognito, GetProfile()->IsOffTheRecord());

  // Take ownership of the WebContents.
  return AddTabFromWebContents(std::move(web_contents), insertion_index,
                               select_tab,
                               TabModel::TabLaunchType::FROM_RESTORE);
}

bool OwningTestTabModel::IsActiveModel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_active_model_;
}

void OwningTestTabModel::AddObserver(TabModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void OwningTestTabModel::RemoveObserver(TabModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

base::android::ScopedJavaLocalRef<jobject> OwningTestTabModel::GetJavaObject()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void OwningTestTabModel::HandlePopupNavigation(TabAndroid* parent,
                                               NavigateParams* params) {
  NOTIMPLEMENTED();
}

content::WebContents* OwningTestTabModel::CreateNewTabForDevTools(
    const GURL& url,
    bool new_window) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool OwningTestTabModel::IsSessionRestoreInProgress() const {
  NOTIMPLEMENTED();
  return false;
}

int OwningTestTabModel::GetTabCountNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) const {
  NOTIMPLEMENTED();
  return 0;
}

void OwningTestTabModel::CloseTabsNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) {
  NOTIMPLEMENTED();
}

tabs::TabStripCollection* OwningTestTabModel::GetTabStripCollection(
    base::PassKey<tabs_api::AndroidTabStripModelAdapter>) {
  NOTIMPLEMENTED();
  return nullptr;
}

void OwningTestTabModel::ActivateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* OwningTestTabModel::OpenTab(const GURL& url,
                                                int index,
                                                bool foreground) {
  NOTIMPLEMENTED();
  return nullptr;
}

void OwningTestTabModel::SetOpenerForTab(tabs::TabHandle target,
                                         tabs::TabHandle opener) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* OwningTestTabModel::GetOpenerForTab(
    tabs::TabHandle target) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* OwningTestTabModel::InsertWebContentsAt(
    int index,
    std::unique_ptr<content::WebContents> web_contents,
    bool should_pin,
    std::optional<tab_groups::TabGroupId> group) {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* OwningTestTabModel::DiscardTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* OwningTestTabModel::DuplicateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabInterface* OwningTestTabModel::GetTab(int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

int OwningTestTabModel::GetIndexOfTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
  return -1;
}

void OwningTestTabModel::HighlightTabs(tabs::TabHandle tab_to_activate,
                                       const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::MoveTab(tabs::TabHandle tab, int index) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::CloseTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

std::vector<tabs::TabInterface*> OwningTestTabModel::GetAllTabs() {
  NOTIMPLEMENTED();
  return {};
}

void OwningTestTabModel::PinTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::UnpinTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

bool OwningTestTabModel::ContainsTabGroup(tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return false;
}

std::optional<tab_groups::TabGroupId> OwningTestTabModel::CreateTabGroup(
    const std::vector<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<split_tabs::SplitTabId> OwningTestTabModel::CreateSplit(
    const std::vector<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::vector<tab_groups::TabGroupId> OwningTestTabModel::ListTabGroups() {
  NOTIMPLEMENTED();
  return {};
}

std::optional<tab_groups::TabGroupVisualData>
OwningTestTabModel::GetTabGroupVisualData(tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

gfx::Range OwningTestTabModel::GetTabGroupTabIndices(
    tab_groups::TabGroupId group_id) {
  NOTIMPLEMENTED();
  return {};
}

void OwningTestTabModel::SetTabGroupVisualData(
    tab_groups::TabGroupId group_id,
    const tab_groups::TabGroupVisualData& visual_data) {
  NOTIMPLEMENTED();
}

std::optional<tab_groups::TabGroupId> OwningTestTabModel::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void OwningTestTabModel::Ungroup(const std::set<tabs::TabHandle>& tabs) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::Unsplit(split_tabs::SplitTabId split_id) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::MoveGroupTo(tab_groups::TabGroupId group_id,
                                     int index) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::MoveTabToWindow(tabs::TabHandle tab,
                                         SessionID destination_window_id,
                                         int destination_index) {
  NOTIMPLEMENTED();
}

bool OwningTestTabModel::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                              SessionID destination_window_id,
                                              int destination_index) {
  NOTIMPLEMENTED();
  return false;
}

bool OwningTestTabModel::IsThisTabListEditable() {
  NOTIMPLEMENTED();
  return true;
}

bool OwningTestTabModel::IsClosingAllTabs() {
  NOTIMPLEMENTED();
  return false;
}

TabAndroid* OwningTestTabModel::AddEmptyTab(
    size_t index,
    bool select,
    TabModel::TabLaunchType launch_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddTabFromWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile())),
      index, select, launch_type);
}

TabAndroid* OwningTestTabModel::AddTabFromWebContents(
    std::unique_ptr<content::WebContents> web_contents,
    size_t index,
    bool select,
    TabModel::TabLaunchType launch_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LE(index, owned_tabs_.size());

  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      GetProfile(), next_tab_id_++, std::move(web_contents));
  TabAndroid* raw_tab = tab.get();

  observer_list_.Notify(&TabModelObserver::WillAddTab, raw_tab, launch_type);
  owned_tabs_.insert(owned_tabs_.begin() + index, std::move(tab));
  observer_list_.Notify(&TabModelObserver::DidAddTab, raw_tab, launch_type);

  // The first tab will always be selected.
  if (select || owned_tabs_.size() == 1) {
    SelectTab(raw_tab, TabModel::TabSelectionType::FROM_NEW);
  }

  return raw_tab;
}

void OwningTestTabModel::SetIsActiveModel(bool is_active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_active_model_ = is_active;
}

void OwningTestTabModel::SelectTab(TabAndroid* tab,
                                   TabModel::TabSelectionType selection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_tab_) {
    active_tab_->web_contents()->UpdateWebContentsVisibility(
        content::Visibility::HIDDEN);
  }
  active_tab_ = tab;
  if (active_tab_) {
    active_tab_->web_contents()->UpdateWebContentsVisibility(
        content::Visibility::VISIBLE);
  }
  observer_list_.Notify(&TabModelObserver::DidSelectTab, active_tab_.get(),
                        selection_type);
}

TabAndroidLoadedWaiter::TabAndroidLoadedWaiter(TabAndroid* tab) {
  if (tab->web_contents()) {
    OnInitWebContents(tab);
  } else {
    tab_observation_.Observe(tab);
  }
}

TabAndroidLoadedWaiter::~TabAndroidLoadedWaiter() = default;

bool TabAndroidLoadedWaiter::Wait() {
  return waiter_helper_.Wait() && load_succeeded_;
}

void TabAndroidLoadedWaiter::OnInitWebContents(TabAndroid* tab) {
  CHECK(tab->web_contents());
  load_succeeded_ = content::WaitForLoadStop(tab->web_contents());
  waiter_helper_.OnEvent();
}
