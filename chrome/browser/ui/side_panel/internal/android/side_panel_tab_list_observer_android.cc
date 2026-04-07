// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_list_observer_android.h"

#include "base/check.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

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

  // TODO(crbug.com/498278573): Refine the logic here.
  // In particular, check if `tab_removed_for_deletion` is correctly set to
  // cover all tab removal cases.
  content::WebContents* old_contents =
      old_tab ? old_tab->GetContents() : nullptr;
  content::WebContents* new_contents = tab->GetContents();
  bool tab_removed_for_deletion = (old_tab == nullptr);

  coordinator_->OnActiveTabChanged(old_contents, new_contents,
                                   tab_removed_for_deletion);

  active_tab_handle_ = tab->GetHandle();
}

void SidePanelTabListObserverAndroid::OnTabListDestroyed(
    TabListInterface& tab_list) {
  observation_.Reset();
}
