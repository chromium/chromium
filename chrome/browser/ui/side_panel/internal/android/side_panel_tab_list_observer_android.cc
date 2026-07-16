// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_list_observer_android.h"

#include "base/check.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

SidePanelTabListObserverAndroid::SidePanelTabListObserverAndroid(
    TabListInterface* tab_list,
    SidePanelCoordinatorAndroid* coordinator)
    : coordinator_(coordinator) {
  CHECK(tab_list);
  if (tabs::TabInterface* active_tab = tab_list->GetActiveTab()) {
    active_tab_handle_ = active_tab->GetHandle();
  }
  observation_.Observe(tab_list);
}

SidePanelTabListObserverAndroid::~SidePanelTabListObserverAndroid() = default;

void SidePanelTabListObserverAndroid::OnActiveTabChanged(
    TabListInterface& tab_list,
    tabs::TabInterface* tab) {
  CHECK(tab) << "New active tab should never be null.";

  tabs::TabInterface* old_tab = active_tab_handle_.Get();

  // For some reason onActiveTabChanged() is triggered _twice_ when we call
  // `TabListInterface::ActivateTab` in tests, so here we check whether
  // `OnActiveTabChanged` is called for the first time. If not, we should not
  // invoke OnActiveTabChanged() on the coordinator.
  //
  // TODO(crbug.com/497986571): Investigate.
  if (old_tab == tab) {
    return;
  }

  content::WebContents* old_contents =
      old_tab ? old_tab->GetContents() : nullptr;
  content::WebContents* new_contents = tab->GetContents();
  // `old_tab` is evaluated from `active_tab_handle_.Get()`.
  //
  // 1. Tab Closure Case:
  // If the underlying tab was destroyed (e.g., normal tab closure), `Get()`
  // returns `nullptr` and `tab_removed_for_deletion` evaluates to `true`.
  //
  // 2. Tab Reparenting Case:
  // When a tab is reparented out, the outgoing tab object still exists in
  // memory (moving to another window), so `old_tab` remains valid and
  // `tab_removed_for_deletion` evaluates to `false`.
  //
  // In multi-tab windows, when the active tab is reparented out, the source
  // window activates another tab first. Since `tab_removed_for_deletion` is
  // `false`, the coordinator's `OnActiveTabChanged()` will receive the valid
  // outgoing contextual registry, allowing it to cleanly close or replace the
  // side panel in the source window if needed.
  bool tab_removed_for_deletion = (old_tab == nullptr);

  coordinator_->OnActiveTabChanged(old_contents, new_contents,
                                   tab_removed_for_deletion);

  active_tab_handle_ = tab->GetHandle();
}

void SidePanelTabListObserverAndroid::OnTabRemoved(
    TabListInterface& tab_list,
    tabs::TabInterface* tab,
    TabRemovedReason removed_reason) {
  if (tab) {
    coordinator_->ClearDeferredEntryForTab(tab->GetHandle());
    if (removed_reason == TabRemovedReason::kInsertedIntoOtherTabStrip) {
      coordinator_->OnTabReparented(tab);
    }
  }
}

void SidePanelTabListObserverAndroid::OnAllTabsAreClosing(
    TabListInterface& tab_list) {
  // Usually when a tab is closed, OnActiveTabChanged() will be called and it
  // will update the side panel states, including closing the side panel if the
  // new active tab doesn't need it.
  //
  // However, when the user closes all tabs, such as via the three-dot menu in
  // the Grid Tab Switcher (GTS), OnActiveTabChanged() won't be called, but we
  // also need to close the side panel if it's shown. Otherwise, when the user
  // creates a new tab, the side panel for a destroyed tab will remain.
  //
  // A common question might be: When the user creates a new tab after closing
  // all tabs, shouldn't OnActiveTabChanged() fix the side panel states?
  //
  // The answer:
  //
  // First of all, Chrome on Android has a _stable_ 0-tab UI state, such as
  // when the user has closed all tabs in GTS, but hasn't created any new tab.
  // Side panel code should reflect this state because GTS is laid on top of the
  // main browser UI (i.e., the main browser UI is still alive).
  //
  // Secondly, OnActiveTabChanged() only closes the side panel if
  // (1) the side panel is currently shown,
  // (2) the new active tab doesn't have an active SidePanelEntry, and
  // (3) the old tab hasn't been deleted.
  //
  // Relying on OnActiveTabChanged() won't meet the condition in (3), and we
  // shouldn't change (3) as it prevents holding/dereferencing an _invalid_
  // pointer to the SidePanelRegistry of the deleted tab.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
}

void SidePanelTabListObserverAndroid::OnTabListDestroyed(
    TabListInterface& tab_list) {
  observation_.Reset();
}
