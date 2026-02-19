// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_menu_delegate_android.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuBridge_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuTypes_jni.h"

namespace {

// Returns a Java ExtensionsMenuTypes.ControlState object.
base::android::ScopedJavaLocalRef<jobject> CreateJavaControlState(
    JNIEnv* env,
    const ExtensionsMenuViewModel::ControlState& state) {
  return extensions::Java_ControlState_Constructor(
      env, static_cast<int>(state.status), state.text, state.accessible_name,
      state.tooltip_text, state.is_on);
}

}  // namespace

namespace extensions {

using base::android::ScopedJavaLocalRef;
using PermissionsManager = extensions::PermissionsManager;

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

std::vector<base::android::ScopedJavaLocalRef<jobject>>
ExtensionsMenuDelegateAndroid::GetMenuEntries(JNIEnv* env) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_entries;

  for (const auto& action_model : menu_model_->action_models()) {
    // TODO(crbug.com/471016915): Use the correct size for the icon once
    // implemented. For now, using a placeholder.
    auto icon_size = gfx::Size();
    extensions::ExtensionId id = action_model->GetId();
    ExtensionsMenuViewModel::MenuEntryState state =
        menu_model_->GetMenuEntryState(id, icon_size);

    base::android::ScopedJavaLocalRef<jobject> j_item =
        Java_MenuEntryState_Constructor(
            env, id, CreateJavaControlState(env, state.action_button));
    java_entries.push_back(std::move(j_item));
  }

  return java_entries;
}

base::android::ScopedJavaLocalRef<jobject>
ExtensionsMenuDelegateAndroid::GetSiteSettings(JNIEnv* env) {
  ExtensionsMenuViewModel::SiteSettingsState site_settings_state =
      menu_model_->GetSiteSettingsState();

  base::android::ScopedJavaLocalRef<jobject> j_toggle_state =
      CreateJavaControlState(env, site_settings_state.toggle);

  return extensions::Java_SiteSettingsState_Constructor(
      env, site_settings_state.label, site_settings_state.has_tooltip,
      j_toggle_state);
}

bool ExtensionsMenuDelegateAndroid::IsReady(JNIEnv* env) {
  return menu_model_->is_populated();
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

void ExtensionsMenuDelegateAndroid::OnPageNavigation() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onModelChanged(env, java_object_);
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
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onModelChanged(env, java_object_);
}

void ExtensionsMenuDelegateAndroid::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(crbug.com/473213114)
}

void ExtensionsMenuDelegateAndroid::OnActionsInitialized() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onReady(env, java_object_);
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

void ExtensionsMenuDelegateAndroid::OnActionButtonClicked(
    const extensions::ExtensionId& extension_id) {
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
  PermissionsManager::UserSiteSetting site_setting =
      is_on ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
            : PermissionsManager::UserSiteSetting::kBlockAllExtensions;
  menu_model_->UpdateSiteSetting(site_setting);
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

void ExtensionsMenuDelegateAndroid::OnSiteSettingsToggleChanged(
    JNIEnv* env,
    bool is_checked) {
  OnSiteSettingsToggleButtonPressed(is_checked);
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
