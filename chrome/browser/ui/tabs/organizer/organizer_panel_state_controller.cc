// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"

#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_USER_DATA(OrganizerPanelStateController);

OrganizerPanelStateController::OrganizerPanelStateController(
    BrowserWindowInterface* browser_window,
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  UpdateOrganizerActionItem();
}

OrganizerPanelStateController::~OrganizerPanelStateController() = default;

// static
OrganizerPanelStateController* OrganizerPanelStateController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool OrganizerPanelStateController::IsOrganizerPanelVisible() const {
  return is_visible_;
}

void OrganizerPanelStateController::SetOrganizerVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
  NotifyStateChanged();
}

base::CallbackListSubscription
OrganizerPanelStateController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

void OrganizerPanelStateController::NotifyStateChanged() {
  UpdateOrganizerActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void OrganizerPanelStateController::UpdateOrganizerActionItem() {
  const auto& text = IsOrganizerPanelVisible() ? IDS_HIDE_ORGANIZER_PANEL
                                               : IDS_VIEW_ORGANIZER_PANEL;

  actions::ActionItem* organizer_action =
      actions::ActionManager::Get().FindAction(kActionToggleOrganizerPanel,
                                               root_action_item_);
  if (organizer_action) {
    organizer_action->SetText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
    organizer_action->SetTooltipText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
  }
}
