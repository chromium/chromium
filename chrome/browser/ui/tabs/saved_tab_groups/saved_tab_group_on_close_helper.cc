// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_on_close_helper.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace tab_groups {

SavedTabGroupOnCloseHelper::SavedTabGroupOnCloseHelper(
    TabGroupSyncService* service,
    tabs::TabInterface* tab)
    : service_(service), tab_(tab), saved_tab_group_id_(std::nullopt) {}

SavedTabGroupOnCloseHelper::~SavedTabGroupOnCloseHelper() = default;

void SavedTabGroupOnCloseHelper::SetGroup(
    const base::Uuid& closed_saved_group_id) {
  CHECK(service_);
  saved_tab_group_id_ = closed_saved_group_id;
  Observe(tab_->GetContents());
  tab_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &SavedTabGroupOnCloseHelper::OnTabClose, base::Unretained(this)));
}

void SavedTabGroupOnCloseHelper::UnsetGroup() {
  saved_tab_group_id_ = std::nullopt;
}

bool SavedTabGroupOnCloseHelper::WillTryToAddToSavedGroupOnClose() {
  return saved_tab_group_id_.has_value();
}

void SavedTabGroupOnCloseHelper::BeforeUnloadDialogCancelled() {
  UnsetGroup();
}

void SavedTabGroupOnCloseHelper::OnTabClose(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (tab_ != tab) {
    return;
  }

  if (tab_->GetContents()->NeedToFireBeforeUnloadOrUnloadEvents()) {
    return;
  }

  if (!saved_tab_group_id_.has_value()) {
    return;
  }

  if (!service_) {
    return;
  }

  // Add to the group on deletion
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    service_->AddUrl(saved_tab_group_id_.value(),
                     tab_->GetContents()->GetTitle(),
                     tab_->GetContents()->GetLastCommittedURL());
  }

  // Make a new tab if we are closing the last tab, to preserve the window.
  // We do not want the window to close if the user selected all tabs and
  // added them to a closed saved tab group.
  TabStripModel* model = tab->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(model);
  if (model->count() == 1) {
    model->delegate()->AddTabAt(GURL(), -1, true);
  }

  UnsetGroup();
}

}  // namespace tab_groups
