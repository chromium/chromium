// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_menu_delegate_android.h"

ExtensionsMenuDelegateAndroid::ExtensionsMenuDelegateAndroid(
    BrowserWindowInterface* browser)
    : menu_model_(
          std::make_unique<ExtensionsMenuViewModel>(browser,
                                                    /*delegate=*/this)) {
  menu_model_observation_.Observe(menu_model_.get());
}

ExtensionsMenuDelegateAndroid::~ExtensionsMenuDelegateAndroid() = default;

std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473192151)
  return nullptr;
}

void ExtensionsMenuDelegateAndroid::OnActiveWebContentsChanged(
    content::WebContents* web_contents) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnActionAdded(
    ExtensionActionViewModel* action_model,
    int index) {
  // TODO(crbug.com/473213114)
}
void ExtensionsMenuDelegateAndroid::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnActionUpdated() {
  // TODO(crbug.com/473213114)
}
void ExtensionsMenuDelegateAndroid::OnActionsInitialized() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestAddedOrUpdated(
    const extensions::ExtensionId& extension_id,
    content::WebContents* web_contents) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestsCleared() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestDismissedByUser(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnShowHostAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnToolbarPinnedActionsChanged() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnUserPermissionsSettingsChanged() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::CloseBubble() {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnAllowExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473213115)
}
void ExtensionsMenuDelegateAndroid::OnDismissExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnExtensionToggleSelected(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnShowRequestsTogglePressed(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnSiteAccessSelected(
    const extensions::ExtensionId& extension_id,
    extensions::PermissionsManager::UserSiteAccess site_access) {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnSiteSettingsToggleButtonPressed(
    bool is_on) {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OnReloadPageButtonClicked() {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OpenMainPage() {
  // TODO(crbug.com/473213115)
}

void ExtensionsMenuDelegateAndroid::OpenSitePermissionsPage(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/473213115)
}
