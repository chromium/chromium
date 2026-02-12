// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_observer.h"

#include "base/check_op.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"

namespace {

std::unique_ptr<sessions::SessionTab> DeepCopySessionTab(
    const sessions::SessionTab* src) {
  auto dest = std::make_unique<sessions::SessionTab>();
  dest->window_id = src->window_id;
  dest->tab_id = src->tab_id;
  dest->tab_visual_index = src->tab_visual_index;
  dest->current_navigation_index = src->current_navigation_index;
  dest->group = src->group;
  dest->split_id = src->split_id;
  dest->pinned = src->pinned;
  dest->extension_app_id = src->extension_app_id;
  dest->user_agent_override = src->user_agent_override;
  dest->timestamp = src->timestamp;
  dest->last_active_time = src->last_active_time;
  dest->session_storage_persistent_id = src->session_storage_persistent_id;
  dest->guid = src->guid;
  dest->data = src->data;
  dest->extra_data = src->extra_data;
  dest->navigations = src->navigations;
  return dest;
}

std::unique_ptr<sessions::SessionTabGroup> DeepCopySessionTabGroup(
    const sessions::SessionTabGroup* src) {
  auto dest = std::make_unique<sessions::SessionTabGroup>(src->id);
  dest->visual_data = src->visual_data;
  dest->saved_guid = src->saved_guid;
  return dest;
}

std::unique_ptr<sessions::SessionSplitTab> DeepCopySessionSplitTab(
    const sessions::SessionSplitTab* src) {
  auto dest = std::make_unique<sessions::SessionSplitTab>(src->id_);
  dest->split_visual_data_ = src->split_visual_data_;
  return dest;
}

std::unique_ptr<sessions::SessionWindow> DeepCopySessionWindow(
    const sessions::SessionWindow* src) {
  auto dest = std::make_unique<sessions::SessionWindow>();
  dest->window_id = src->window_id;
  dest->bounds = src->bounds;
  dest->workspace = src->workspace;
  dest->visible_on_all_workspaces = src->visible_on_all_workspaces;
  dest->selected_tab_index = src->selected_tab_index;
  dest->type = src->type;
  dest->is_constrained = src->is_constrained;
  dest->timestamp = src->timestamp;
  dest->show_state = src->show_state;
  dest->app_name = src->app_name;
  dest->user_title = src->user_title;
  dest->extra_data = src->extra_data;

  dest->tabs.reserve(src->tabs.size());
  for (const auto& tab : src->tabs) {
    dest->tabs.push_back(DeepCopySessionTab(tab.get()));
  }

  dest->tab_groups.reserve(src->tab_groups.size());
  for (const auto& group : src->tab_groups) {
    dest->tab_groups.push_back(DeepCopySessionTabGroup(group.get()));
  }

  dest->split_tabs.reserve(src->split_tabs.size());
  for (const auto& split : src->split_tabs) {
    dest->split_tabs.push_back(DeepCopySessionSplitTab(split.get()));
  }

  return dest;
}

}  // namespace

TabStripInternalsObserver::TabStripInternalsObserver(Profile* profile,
                                                     UpdateCallback callback)
    : callback_(std::move(callback)) {
  BrowserList::AddObserver(this);
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        StartObservingBrowser(browser);
        return true;
      });
  StartObservingTabRestore(profile);
  SessionRestore::AddObserver(this);
}

TabStripInternalsObserver::~TabStripInternalsObserver() {
  BrowserList::RemoveObserver(this);
  TabStripModelObserver::StopObservingAll(this);
  StopObservingTabRestore();
  SessionRestore::RemoveObserver(this);
}

void TabStripInternalsObserver::OnBrowserAdded(Browser* browser) {
  StartObservingBrowser(browser);
  FireUpdate();
}

void TabStripInternalsObserver::OnBrowserRemoved(Browser* browser) {
  StopObservingBrowser(browser);
  FireUpdate();
}

void TabStripInternalsObserver::OnTabStripModelChanged(
    TabStripModel* /*tab_strip_model*/,
    const TabStripModelChange& /*change*/,
    const TabStripSelectionChange& /*selection*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnTabGroupChanged(
    const TabGroupChange& /*change*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnSplitTabChanged(
    const SplitTabChange& /*change*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnTabChangedAt(tabs::TabInterface* /*tab*/,
                                               int /*index*/,
                                               TabChangeType /*change_type*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnTabPinnedStateChanged(
    tabs::TabInterface* /*tab*/,
    int /*index*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabGroupedStateChanged(
    TabStripModel* /*tab_strip_model*/,
    std::optional<tab_groups::TabGroupId> /*old_group*/,
    std::optional<tab_groups::TabGroupId> /*new_group*/,
    tabs::TabInterface* /*tab*/,
    int /*index*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabRestoreServiceChanged(
    sessions::TabRestoreService* /*service*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  CHECK_EQ(service, service_);
  StopObservingTabRestore();
}

void TabStripInternalsObserver::OnGotSession(
    Profile* /*profile*/,
    bool /*for_app*/,
    const std::vector<const sessions::SessionWindow*>& windows) {
  last_session_windows_.clear();
  last_session_windows_.reserve(windows.size());

  for (const auto* window : windows) {
    // Deep copying here as a precaution in-case the caller of this observer
    // method gets destroyed before this callback's execution is complete.
    last_session_windows_.push_back(DeepCopySessionWindow(window));
  }

  FireUpdate();
}

// Private methods
void TabStripInternalsObserver::StartObservingBrowser(
    BrowserWindowInterface* browser) {
  if (TabStripModel* const tab_strip_model = browser->GetTabStripModel()) {
    tab_strip_model->AddObserver(this);
  }
}

void TabStripInternalsObserver::StopObservingBrowser(
    BrowserWindowInterface* browser) {
  if (TabStripModel* const tab_strip_model = browser->GetTabStripModel()) {
    tab_strip_model->RemoveObserver(this);
  }
}

void TabStripInternalsObserver::StartObservingTabRestore(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return;
  }
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service) {
    service_ = service;
    service_->AddObserver(this);
  }
}

void TabStripInternalsObserver::StopObservingTabRestore() {
  if (service_) {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }
}

void TabStripInternalsObserver::FireUpdate() {
  // TODO (crbug.com/427204855): Throttle updates by debouncing once
  // TabRestoreServiceObserver has been implemented.
  callback_.Run();
}
