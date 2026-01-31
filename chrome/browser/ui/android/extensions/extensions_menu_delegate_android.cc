// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_menu_delegate_android.h"

#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuBridge_jni.h"

namespace extensions {

ExtensionsMenuDelegateAndroid::ExtensionsMenuDelegateAndroid(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      menu_model_(std::make_unique<ExtensionsMenuViewModel>(browser,
                                                            /*delegate=*/this)),
      java_object_(java_object) {
  menu_model_observation_.Observe(menu_model_.get());
}

ExtensionsMenuDelegateAndroid::~ExtensionsMenuDelegateAndroid() = default;

void ExtensionsMenuDelegateAndroid::Destroy(JNIEnv* env) {
  delete this;
}

std::unique_ptr<ExtensionActionViewModel>
ExtensionsMenuDelegateAndroid::CreateActionViewModel(
    const extensions::ExtensionId& extension_id) {
  // TODO(crbug.com/461981075): Pass a `bridge` instance instead of a nullptr.
  return ExtensionActionViewModel::Create(
      extension_id, browser_,
      std::make_unique<ExtensionActionDelegateAndroid>(browser_, extension_id,
                                                       nullptr));
}

void ExtensionsMenuDelegateAndroid::OnActiveWebContentsChanged() {
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

void ExtensionsMenuDelegateAndroid::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnActionsInitialized() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestsCleared() {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int index) {
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

static int64_t JNI_ExtensionsMenuBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    int64_t j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<int64_t>(
      new ExtensionsMenuDelegateAndroid(browser, java_object));
}

}  // namespace extensions

DEFINE_JNI(ExtensionsMenuBridge)
