// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

DEFINE_USER_DATA(ProjectsPanelStateController);

ProjectsPanelStateController::ProjectsPanelStateController(
    BrowserWindowInterface* browser_window,
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  UpdateProjectsActionItem();
}

ProjectsPanelStateController::~ProjectsPanelStateController() = default;

// static
ProjectsPanelStateController* ProjectsPanelStateController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool ProjectsPanelStateController::IsProjectsPanelVisible() const {
  return is_visible_;
}

void ProjectsPanelStateController::SetProjectsVisible(bool visible) {
  if (is_visible_ != visible) {
    is_visible_ = visible;
    NotifyStateChanged();
  }
}

base::CallbackListSubscription
ProjectsPanelStateController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

void ProjectsPanelStateController::NotifyStateChanged() {
  UpdateProjectsActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void ProjectsPanelStateController::UpdateProjectsActionItem() {
  const gfx::VectorIcon& icon = IsProjectsPanelVisible()
                                    ? kCloseChromeRefreshIcon
                                    : kSavedTabGroupBarEverythingIcon;
  const auto& text = IsProjectsPanelVisible() ? IDS_HIDE_PROJECTS_PANEL
                                              : IDS_VIEW_PROJECTS_PANEL;

  actions::ActionItem* projects_action =
      actions::ActionManager::Get().FindAction(kActionToggleProjectsPanel,
                                               root_action_item_);
  if (projects_action) {
    projects_action->SetImage(
        ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
    projects_action->SetText(BrowserActions::GetCleanTitleAndTooltipText(
        l10n_util::GetStringUTF16(text)));
    projects_action->SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
        l10n_util::GetStringUTF16(text)));
  }
}
