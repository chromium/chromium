// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

ExtensionsToolbarContainerViewController::
    ExtensionsToolbarContainerViewController(
        Profile* profile,
        ExtensionsToolbarContainer* extensions_container)
    : profile_(profile), extensions_container_(extensions_container) {
  model_observation_.Observe(ToolbarActionsModel::Get(profile_));
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(profile_));
}

ExtensionsToolbarContainerViewController::
    ~ExtensionsToolbarContainerViewController() {
  extensions_container_ = nullptr;
  model_observation_.Reset();
  permissions_manager_observation_.Reset();
}

void ExtensionsToolbarContainerViewController::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->AddAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->RemoveAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->UpdateAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarModelInitialized() {
  CHECK(extensions_container_);
  extensions_container_->CreateActions();
}

void ExtensionsToolbarContainerViewController::OnToolbarPinnedActionsChanged() {
  CHECK(extensions_container_);
  extensions_container_->UpdatePinnedActions();
}

void ExtensionsToolbarContainerViewController::OnUserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  CHECK(extensions_container_);
  extensions_container_->UpdateControlsVisibility();
  // TODO(crbug.com/1351778): Update request access button hover card. This
  // will be slightly different than 'OnToolbarActionUpdated' since site
  // settings update are not tied to a specific action.
}

void ExtensionsToolbarContainerViewController::
    OnShowAccessRequestsInToolbarChanged(
        const extensions::ExtensionId& extension_id,
        bool can_show_requests) {
  CHECK(extensions_container_);
  extensions_container_->UpdateControlsVisibility();
  // TODO(crbug.com/1351778): Update requests access button hover card. This is
  // tricky because it would need to change the items in the dialog. Another
  // option is to close the hover card if its shown whenever request access
  // button is updated.
}

void ExtensionsToolbarContainerViewController::OnExtensionDismissedRequests(
    const extensions::ExtensionId& extension_id,
    const url::Origin& origin) {
  CHECK(extensions_container_);
  auto* web_contents = extensions_container_->GetCurrentWebContents();
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(profile_)->GetUserSiteSetting(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  extensions_container_->UpdateRequestAccessButton(site_setting, web_contents);
}
