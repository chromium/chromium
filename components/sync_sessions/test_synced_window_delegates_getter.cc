// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/test_synced_window_delegates_getter.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/tab_node_pool.h"

namespace sync_sessions {
namespace {

const char kTitle[] = "title";

}  // namespace

TestSyncedTabDelegate::TestSyncedTabDelegate(
    SessionID window_id,
    SessionID tab_id,
    const base::RepeatingCallback<void(SyncedTabDelegate*)>& notify_cb)
    : window_id_(window_id), tab_id_(tab_id), notify_cb_(notify_cb) {}

TestSyncedTabDelegate::~TestSyncedTabDelegate() = default;

void TestSyncedTabDelegate::Navigate(const std::string& url,
                                     base::Time time,
                                     ui::PageTransition transition) {
  sync_pb::TabNavigation tab_navigation;
  tab_navigation.set_virtual_url(url);
  tab_navigation.set_title(kTitle);
  tab_navigation.set_global_id(time.ToInternalValue());
  tab_navigation.set_unique_id(time.ToInternalValue());
  tab_navigation.set_http_status_code(200);

  auto entry = std::make_unique<sessions::SerializedNavigationEntry>(
      SessionNavigationFromSyncData(0, tab_navigation));
  sessions::SerializedNavigationEntryTestHelper::SetTimestamp(time,
                                                              entry.get());
  sessions::SerializedNavigationEntryTestHelper::SetTransitionType(transition,
                                                                   entry.get());

  entries_.push_back(std::move(entry));
  page_language_per_index_.push_back(std::string());
  set_current_entry_index(GetCurrentEntryIndex() + 1);
  notify_cb_.Run(this);
}

void TestSyncedTabDelegate::set_current_entry_index(int i) {
  current_entry_index_ = i;
}

void TestSyncedTabDelegate::set_blocked_navigations(
    const std::vector<std::unique_ptr<sessions::SerializedNavigationEntry>>&
        navs) {
  for (auto& entry : navs) {
    blocked_navigations_.push_back(
        std::make_unique<sessions::SerializedNavigationEntry>(*entry));
  }
}

void TestSyncedTabDelegate::SetPageLanguageAtIndex(
    int i,
    const std::string& language) {
  page_language_per_index_[i] = language;
}

bool TestSyncedTabDelegate::IsInitialBlankNavigation() const {
  // This differs from NavigationControllerImpl, which has an initial blank
  // NavigationEntry.
  return GetEntryCount() == 0;
}

int TestSyncedTabDelegate::GetCurrentEntryIndex() const {
  return current_entry_index_;
}

GURL TestSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  if (static_cast<size_t>(i) >= entries_.size())
    return GURL();
  return entries_[i]->virtual_url();
}

GURL TestSyncedTabDelegate::GetFaviconURLAtIndex(int i) const {
  return GURL();
}

ui::PageTransition TestSyncedTabDelegate::GetTransitionAtIndex(int i) const {
  if (static_cast<size_t>(i) >= entries_.size())
    return ui::PAGE_TRANSITION_LINK;
  return entries_[i]->transition_type();
}

std::string TestSyncedTabDelegate::GetPageLanguageAtIndex(int i) const {
  DCHECK(static_cast<size_t>(i) < page_language_per_index_.size());
  return page_language_per_index_[i];
}

void TestSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  if (static_cast<size_t>(i) >= entries_.size())
    return;
  *serialized_entry = *entries_[i];
}

int TestSyncedTabDelegate::GetEntryCount() const {
  return entries_.size();
}

SessionID TestSyncedTabDelegate::GetWindowId() const {
  return window_id_;
}

SessionID TestSyncedTabDelegate::GetSessionId() const {
  return tab_id_;
}

bool TestSyncedTabDelegate::IsBeingDestroyed() const {
  return false;
}

std::string TestSyncedTabDelegate::GetExtensionAppId() const {
  return std::string();
}

bool TestSyncedTabDelegate::ProfileIsSupervised() const {
  return is_supervised_;
}

void TestSyncedTabDelegate::set_is_supervised(bool is_supervised) {
  is_supervised_ = is_supervised;
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
TestSyncedTabDelegate::GetBlockedNavigations() const {
  return &blocked_navigations_;
}

bool TestSyncedTabDelegate::IsPlaceholderTab() const {
  return false;
}

bool TestSyncedTabDelegate::ShouldSync(SyncSessionsClient* sessions_client) {
  // This is just a simple filter that isn't meant to fully reproduce
  // the TabContentsTabDelegate's ShouldSync logic.
  // Verify all URL's are valid (which will ignore an initial blank page) and
  // that there is at least one http:// url.
  int http_count = 0;
  for (auto& entry : entries_) {
    if (!entry->virtual_url().is_valid())
      return false;
    if (entry->virtual_url().SchemeIsHTTPOrHTTPS())
      http_count++;
  }
  return http_count > 0;
}

int64_t TestSyncedTabDelegate::GetTaskIdForNavigationId(int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  return -1;
}

int64_t TestSyncedTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  return -1;
}

int64_t TestSyncedTabDelegate::GetRootTaskIdForNavigationId(int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  return -1;
}

PlaceholderTabDelegate::PlaceholderTabDelegate(SessionID tab_id)
    : tab_id_(tab_id) {}

PlaceholderTabDelegate::~PlaceholderTabDelegate() = default;

SessionID PlaceholderTabDelegate::GetSessionId() const {
  return tab_id_;
}

bool PlaceholderTabDelegate::IsPlaceholderTab() const {
  return true;
}

SessionID PlaceholderTabDelegate::GetWindowId() const {
  NOTREACHED();
  return SessionID::InvalidValue();
}

bool PlaceholderTabDelegate::IsBeingDestroyed() const {
  NOTREACHED();
  return false;
}

std::string PlaceholderTabDelegate::GetExtensionAppId() const {
  NOTREACHED();
  return "";
}

bool PlaceholderTabDelegate::IsInitialBlankNavigation() const {
  NOTREACHED();
  return false;
}

int PlaceholderTabDelegate::GetCurrentEntryIndex() const {
  NOTREACHED();
  return 0;
}

int PlaceholderTabDelegate::GetEntryCount() const {
  NOTREACHED();
  return 0;
}

GURL PlaceholderTabDelegate::GetVirtualURLAtIndex(int i) const {
  NOTREACHED();
  return GURL();
}

GURL PlaceholderTabDelegate::GetFaviconURLAtIndex(int i) const {
  NOTREACHED();
  return GURL();
}

ui::PageTransition PlaceholderTabDelegate::GetTransitionAtIndex(int i) const {
  NOTREACHED();
  return ui::PageTransition();
}

std::string PlaceholderTabDelegate::GetPageLanguageAtIndex(int i) const {
  NOTREACHED();
  return std::string();
}

void PlaceholderTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  NOTREACHED();
}

bool PlaceholderTabDelegate::ProfileIsSupervised() const {
  NOTREACHED();
  return false;
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
PlaceholderTabDelegate::GetBlockedNavigations() const {
  NOTREACHED();
  return nullptr;
}

bool PlaceholderTabDelegate::ShouldSync(SyncSessionsClient* sessions_client) {
  NOTREACHED();
  return false;
}

int64_t PlaceholderTabDelegate::GetTaskIdForNavigationId(int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  NOTREACHED() << "Task IDs are not used for Placeholder Tabs";
  return -1;
}

int64_t PlaceholderTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  NOTREACHED() << "Task IDs are not used for Placeholder Tabs";
  return -1;
}

int64_t PlaceholderTabDelegate::GetRootTaskIdForNavigationId(int nav_id) const {
  // Task IDs are currently not used in the tests. -1 signals an unknown Task
  // ID.
  NOTREACHED() << "Task IDs are not used for Placeholder Tabs";
  return -1;
}

TestSyncedWindowDelegate::TestSyncedWindowDelegate(
    SessionID window_id,
    sync_pb::SessionWindow_BrowserType type)
    : window_id_(window_id),
      window_type_(type),
      is_session_restore_in_progress_(false) {}

TestSyncedWindowDelegate::~TestSyncedWindowDelegate() = default;

void TestSyncedWindowDelegate::OverrideTabAt(int index,
                                             SyncedTabDelegate* delegate) {
  if (index >= static_cast<int>(tab_delegates_.size()))
    tab_delegates_.resize(index + 1, nullptr);

  tab_delegates_[index] = delegate;
}

void TestSyncedWindowDelegate::CloseTab(SessionID tab_id) {
  base::EraseIf(tab_delegates_, [tab_id](SyncedTabDelegate* tab) {
    return tab->GetSessionId() == tab_id;
  });
}

void TestSyncedWindowDelegate::SetIsSessionRestoreInProgress(bool value) {
  is_session_restore_in_progress_ = value;
}

bool TestSyncedWindowDelegate::HasWindow() const {
  return true;
}

SessionID TestSyncedWindowDelegate::GetSessionId() const {
  return window_id_;
}

int TestSyncedWindowDelegate::GetTabCount() const {
  return tab_delegates_.size();
}

int TestSyncedWindowDelegate::GetActiveIndex() const {
  return 0;
}

bool TestSyncedWindowDelegate::IsTypeNormal() const {
  return window_type_ == sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
}

bool TestSyncedWindowDelegate::IsTypePopup() const {
  return window_type_ == sync_pb::SessionWindow_BrowserType_TYPE_POPUP;
}

bool TestSyncedWindowDelegate::IsTabPinned(const SyncedTabDelegate* tab) const {
  return false;
}

SyncedTabDelegate* TestSyncedWindowDelegate::GetTabAt(int index) const {
  if (index >= static_cast<int>(tab_delegates_.size()))
    return nullptr;

  return tab_delegates_[index];
}

SessionID TestSyncedWindowDelegate::GetTabIdAt(int index) const {
  SyncedTabDelegate* delegate = GetTabAt(index);
  if (!delegate)
    return SessionID::InvalidValue();
  return delegate->GetSessionId();
}

bool TestSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  return is_session_restore_in_progress_;
}

bool TestSyncedWindowDelegate::ShouldSync() const {
  return true;
}

TestSyncedWindowDelegatesGetter::TestSyncedWindowDelegatesGetter() = default;

TestSyncedWindowDelegatesGetter::~TestSyncedWindowDelegatesGetter() = default;

void TestSyncedWindowDelegatesGetter::ResetWindows() {
  delegates_.clear();
  windows_.clear();
}

TestSyncedWindowDelegate* TestSyncedWindowDelegatesGetter::AddWindow(
    sync_pb::SessionWindow_BrowserType type,
    SessionID window_id) {
  windows_.push_back(
      std::make_unique<TestSyncedWindowDelegate>(window_id, type));
  CHECK_EQ(window_id, windows_.back()->GetSessionId());
  delegates_[window_id] = windows_.back().get();
  return windows_.back().get();
}

TestSyncedTabDelegate* TestSyncedWindowDelegatesGetter::AddTab(
    SessionID window_id,
    SessionID tab_id) {
  tabs_.push_back(std::make_unique<TestSyncedTabDelegate>(
      window_id, tab_id,
      base::BindRepeating(&DummyRouter::NotifyNav,
                          base::Unretained(&router_))));
  for (auto& window : windows_) {
    if (window->GetSessionId() == window_id) {
      int tab_index = window->GetTabCount();
      window->OverrideTabAt(tab_index, tabs_.back().get());
    }
  }

  // Simulate the browser firing a tab parented notification, ahead of actual
  // navigations.
  router_.NotifyNav(tabs_.back().get());
  return tabs_.back().get();
}

void TestSyncedWindowDelegatesGetter::CloseTab(SessionID tab_id) {
  for (auto& window : windows_) {
    // CloseTab() will only take effect with the belonging window, the rest will
    // simply ignore the call.
    window->CloseTab(tab_id);
  }
}

void TestSyncedWindowDelegatesGetter::SessionRestoreComplete() {
  for (auto& window : windows_)
    window->SetIsSessionRestoreInProgress(false);

  router_.NotifySessionRestoreComplete();
}

LocalSessionEventRouter* TestSyncedWindowDelegatesGetter::router() {
  return &router_;
}

SyncedWindowDelegatesGetter::SyncedWindowDelegateMap
TestSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  return delegates_;
}

const SyncedWindowDelegate* TestSyncedWindowDelegatesGetter::FindById(
    SessionID id) {
  for (auto window_iter_pair : delegates_) {
    if (window_iter_pair.second->GetSessionId() == id)
      return window_iter_pair.second;
  }
  return nullptr;
}

TestSyncedWindowDelegatesGetter::DummyRouter::DummyRouter() = default;

TestSyncedWindowDelegatesGetter::DummyRouter::~DummyRouter() = default;

void TestSyncedWindowDelegatesGetter::DummyRouter::StartRoutingTo(
    LocalSessionEventHandler* handler) {
  handler_ = handler;
}

void TestSyncedWindowDelegatesGetter::DummyRouter::Stop() {
  handler_ = nullptr;
}

void TestSyncedWindowDelegatesGetter::DummyRouter::NotifyNav(
    SyncedTabDelegate* tab) {
  if (handler_)
    handler_->OnLocalTabModified(tab);
}

void TestSyncedWindowDelegatesGetter::DummyRouter::
    NotifySessionRestoreComplete() {
  if (handler_)
    handler_->OnSessionRestoreComplete();
}

}  // namespace sync_sessions
