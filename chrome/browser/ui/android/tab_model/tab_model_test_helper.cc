// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"

#include <jni.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/android_buildflags.h"
#include "chrome/browser/android/tab_android.h"
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
                           chrome::android::ActivityType activity_type)
    : TabModel(profile, activity_type, TabModel::TabModelType::kStandard) {}

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

void TestTabModel::SetActiveIndex(int index) {}

void TestTabModel::ForceCloseAllTabs() {}

void TestTabModel::CloseTabAt(int index) {}

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

void TestTabModel::ActivateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* TestTabModel::OpenTab(const GURL& url, int index) {
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

void TestTabModel::MoveGroupTo(tab_groups::TabGroupId group_id, int index) {
  NOTIMPLEMENTED();
}

void TestTabModel::MoveTabToWindow(tabs::TabHandle tab,
                                   SessionID destination_window_id,
                                   int destination_index) {
  NOTIMPLEMENTED();
}

void TestTabModel::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                        SessionID destination_window_id,
                                        int destination_index) {
  NOTIMPLEMENTED();
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
    chrome::android::ActivityType activity_type)
    : TabModel(profile, activity_type, TabModel::TabModelType::kStandard) {
  TabModelList::AddTabModel(this);
}

OwningTestTabModel::~OwningTestTabModel() {
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

content::WebContents* OwningTestTabModel::GetWebContentsAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetTabAt(index)->web_contents();
}

TabAndroid* OwningTestTabModel::GetTabAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return owned_tabs_.at(index).get();
}

void OwningTestTabModel::SetActiveIndex(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SelectTab(GetTabAt(index), TabModel::TabSelectionType::FROM_USER);
}

void OwningTestTabModel::ForceCloseAllTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (GetTabCount() > 0) {
    CloseTabAt(GetTabCount() - 1);
  }
}

void OwningTestTabModel::CloseTabAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), owned_tabs_.size());
  auto tab_it = owned_tabs_.begin() + index;
  observer_list_.Notify(&TabModelObserver::WillCloseTab, tab_it->get());

  if (active_tab_.get() == tab_it->get()) {
    // Deselect the tab before removing it. If it was the last tab, the tab to
    // its left (the new last tab) will become active, otherwise the tab to its
    // right will.
    TabAndroid* new_active_tab = nullptr;
    if (static_cast<size_t>(index) + 1 < owned_tabs_.size()) {
      new_active_tab = GetTabAt(index + 1);
    } else if (index > 0) {
      new_active_tab = GetTabAt(index - 1);
    }
    SelectTab(new_active_tab, TabModel::TabSelectionType::FROM_CLOSE);
  }

  // Remove the tab from the list. Its WebContents will be deleted when it goes
  // out of scope.
  std::unique_ptr<TabAndroid> tab = std::move(*tab_it);
  owned_tabs_.erase(tab_it);

  observer_list_.Notify(&TabModelObserver::TabRemoved, tab.get());
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

void OwningTestTabModel::ActivateTab(tabs::TabHandle tab) {
  NOTIMPLEMENTED();
}

tabs::TabInterface* OwningTestTabModel::OpenTab(const GURL& url, int index) {
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

void OwningTestTabModel::MoveGroupTo(tab_groups::TabGroupId group_id,
                                     int index) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::MoveTabToWindow(tabs::TabHandle tab,
                                         SessionID destination_window_id,
                                         int destination_index) {
  NOTIMPLEMENTED();
}

void OwningTestTabModel::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                              SessionID destination_window_id,
                                              int destination_index) {
  NOTIMPLEMENTED();
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
