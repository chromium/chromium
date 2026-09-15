// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_model_observer.h"

#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

SidePanelTabModelObserver::SidePanelTabModelObserver(
    TabModel* tab_model,
    SidePanelCoordinatorAndroid* coordinator)
    : coordinator_(coordinator), tab_model_(tab_model) {
  CHECK(tab_model);
  if (tabs::TabInterface* active_tab = tab_model_->GetActiveTab()) {
    active_tab_handle_ = active_tab->GetHandle();
  }
  tab_model_->AddObserver(this);
}

SidePanelTabModelObserver::~SidePanelTabModelObserver() {
  if (tab_model_) {
    tab_model_->RemoveObserver(this);
  }
}

void SidePanelTabModelObserver::DidSelectTab(TabAndroid* tab,
                                             TabModel::TabSelectionType type) {
  CHECK(tab) << "New active tab should never be null.";

  TabAndroid* old_tab = TabAndroid::FromTabHandle(active_tab_handle_);

  // For some reason DidSelectTab() is triggered _twice_ when we call
  // `TabListInterface::ActivateTab` in tests, so here we check whether
  // `DidSelectTab` is called for the first time. If not, we should not
  // update the coordinator.
  //
  // TODO(crbug.com/497986571): Investigate.
  if (old_tab == tab) {
    return;
  }

  active_tab_handle_ = tab->GetHandle();
  coordinator_->OnTabSelected(old_tab, tab);
}

void SidePanelTabModelObserver::DidRemoveTabForClosure(TabAndroid* tab) {
  CHECK(tab);

  if (active_tab_handle_ == tab->GetHandle()) {
    active_tab_handle_ = {};
  }

  coordinator_->OnTabClosed(tab);
}

void SidePanelTabModelObserver::OnFinishingTabClosure(
    TabAndroid* tab,
    TabModel::TabClosingSource source) {
  coordinator_->OnTabWillBeDestroyed(tab);
}

void SidePanelTabModelObserver::OnFinishingMultipleTabClosure(
    const std::vector<TabAndroid*>& tabs,
    bool canRestore) {
  if (are_all_tabs_closing_) {
    are_all_tabs_closing_ = false;
    coordinator_->OnAllTabsWillBeDestroyed();
  }
}

void SidePanelTabModelObserver::TabClosureUndone(TabAndroid* tab) {
  are_all_tabs_closing_ = false;
}

void SidePanelTabModelObserver::TabRemoved(TabAndroid* tab) {
  CHECK(tab);

  if (active_tab_handle_ == tab->GetHandle()) {
    active_tab_handle_ = {};
  }

  coordinator_->OnTabReparented(tab);
}

void SidePanelTabModelObserver::WillCloseTabs(
    const std::vector<TabAndroid*>& tabs,
    bool is_all_tabs,
    bool allow_undo) {
  if (is_all_tabs) {
    are_all_tabs_closing_ = true;
    coordinator_->OnAllTabsWillClose();
  }
}

void SidePanelTabModelObserver::AllTabsAreClosing() {
  WillCloseTabs({}, /*is_all_tabs=*/true, /*allow_undo=*/false);
}

void SidePanelTabModelObserver::OnTabModelDestroyed(TabModel& tab_model) {
  CHECK(tab_model_ == &tab_model);
  tab_model_->RemoveObserver(this);
  tab_model_ = nullptr;
  active_tab_handle_ = {};
  are_all_tabs_closing_ = false;
}
