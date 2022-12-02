// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_ui_manager.h"

#include "base/auto_reset.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"

namespace web_app {

namespace {

// Keeps track of whether the testing code has set an action to be performed
// when the app identity update confirmation dialog is set to show. The behavior
// is determined by the IdentityUpdateDialogAction enum in
// web_app_ui_manager.h.
absl::optional<AppIdentityUpdate>
    g_auto_resolve_app_identity_update_dialog_for_testing = absl::nullopt;

}  // namespace

base::AutoReset<absl::optional<AppIdentityUpdate>>
SetIdentityUpdateDialogActionForTesting(  // IN-TEST
    absl::optional<AppIdentityUpdate> auto_accept_action) {
  return base::AutoReset<absl::optional<AppIdentityUpdate>>(
      &g_auto_resolve_app_identity_update_dialog_for_testing,
      auto_accept_action);
}

absl::optional<AppIdentityUpdate>
GetIdentityUpdateDialogActionForTesting() {  // IN-TEST
  return g_auto_resolve_app_identity_update_dialog_for_testing;
}

WebAppUiManager::WebAppUiManager() = default;

WebAppUiManager::~WebAppUiManager() {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnWebAppUiManagerDestroyed();
}

void WebAppUiManager::AddObserver(WebAppUiManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppUiManager::RemoveObserver(WebAppUiManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppUiManager::NotifyReadyToCommitNavigation(
    const AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnReadyToCommitNavigation(app_id, navigation_handle);
}

}  // namespace web_app
