// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_menu_delegate_android.h"

#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuBridge_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuTypes_jni.h"

namespace {

// TODO(crbug.com/471016915): Placeholder size. Replace with size provided from
// Java.
constexpr gfx::Size kActionIconSize = gfx::Size(40, 40);

base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const ui::ImageModel& image_model) {
  if (image_model.IsEmpty() || !image_model.IsImage()) {
    return nullptr;
  }

  gfx::ImageSkia image_skia = image_model.GetImage().AsImageSkia();

  float icon_scale_factor = 1.0f;
  const gfx::ImageSkiaRep& rep =
      image_skia.GetRepresentation(icon_scale_factor);
  const SkBitmap& bitmap = rep.GetBitmap();

  if (bitmap.isNull()) {
    return nullptr;
  }

  return gfx::ConvertToJavaBitmap(bitmap);
}

// Returns a Java ExtensionsMenuTypes.ControlState object.
base::android::ScopedJavaLocalRef<jobject> CreateJavaControlState(
    JNIEnv* env,
    const ExtensionsMenuViewModel::ControlState& state) {
  auto state_icon_bitmap = ConvertToJavaBitmap(state.icon);
  return extensions::Java_ControlState_Constructor(
      env, static_cast<int>(state.status), state.text, state.accessible_name,
      state.tooltip_text, state.is_on, state_icon_bitmap);
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

base::android::ScopedJavaLocalRef<jobject>
ExtensionsMenuDelegateAndroid::GetActionIcon(JNIEnv* env, int action_index) {
  ui::ImageModel icon_model =
      menu_model_->GetActionIcon(action_index, kActionIconSize);
  return ConvertToJavaBitmap(icon_model);
}

base::android::ScopedJavaLocalRef<jobject>
ExtensionsMenuDelegateAndroid::GetExtensionSitePermissionsState(
    JNIEnv* env,
    const std::string& extension_id) {
  ExtensionsMenuViewModel::ExtensionSitePermissionsState state =
      menu_model_->GetExtensionSitePermissionsState(extension_id,
                                                    kActionIconSize);

  base::android::ScopedJavaLocalRef<jobject> j_show_requests_toggle =
      CreateJavaControlState(env, state.show_requests_toggle);

  return extensions::Java_ExtensionSitePermissionsState_Constructor(
      env, state.extension_name, ConvertToJavaBitmap(state.extension_icon),
      j_show_requests_toggle);
}

base::android::ScopedJavaLocalRef<jobject>
ExtensionsMenuDelegateAndroid::GetMenuEntry(JNIEnv* env, int action_index) {
  const auto& action_models = menu_model_->action_models();
  CHECK_GE(action_index, 0);
  CHECK_LT(static_cast<size_t>(action_index), action_models.size());

  const auto& action_model = action_models[action_index];
  extensions::ExtensionId id = action_model->GetId();
  ExtensionsMenuViewModel::MenuEntryState state =
      menu_model_->GetMenuEntryState(id, kActionIconSize);

  return Java_MenuEntryState_Constructor(
      env, id, CreateJavaControlState(env, state.action_button),
      CreateJavaControlState(env, state.context_menu_button),
      CreateJavaControlState(env, state.site_access_toggle),
      CreateJavaControlState(env, state.site_permissions_button));
}

int ExtensionsMenuDelegateAndroid::GetOptionalSection(JNIEnv* env) {
  return static_cast<int>(menu_model_->GetOptionalSection());
}

std::vector<base::android::ScopedJavaLocalRef<jobject>>
ExtensionsMenuDelegateAndroid::GetHostAccessRequests(JNIEnv* env) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_entries;

  const auto& requests = menu_model_->host_access_requests();
  for (const auto& extension_id : requests) {
    ExtensionsMenuViewModel::HostAccessRequest request =
        menu_model_->GetHostAccessRequest(extension_id, kActionIconSize);
    java_entries.push_back(extensions::Java_HostAccessRequest_Constructor(
        env, request.extension_id, request.extension_name,
        ConvertToJavaBitmap(request.extension_icon)));
  }

  return java_entries;
}

std::vector<base::android::ScopedJavaLocalRef<jobject>>
ExtensionsMenuDelegateAndroid::GetMenuEntries(JNIEnv* env) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_entries;

  for (size_t i = 0; i < menu_model_->action_models().size(); ++i) {
    java_entries.push_back(GetMenuEntry(env, i));
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
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onActionAdded(env, java_object_, index);
}

void ExtensionsMenuDelegateAndroid::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onActionRemoved(env, java_object_, index);
}

void ExtensionsMenuDelegateAndroid::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onActionUpdated(env, java_object_, index);
}

void ExtensionsMenuDelegateAndroid::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onActionIconUpdated(env, java_object_, index);
}

void ExtensionsMenuDelegateAndroid::OnActionsInitialized() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onReady(env, java_object_);
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(crbug.com/473213114)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onHostAccessRequestAdded(env, java_object_,
                                                     extension_id);
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(crbug.com/473213114)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onHostAccessRequestUpdated(env, java_object_,
                                                       extension_id);
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestsCleared() {
  // TODO(crbug.com/473213114)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onHostAccessRequestsCleared(env, java_object_);
}

void ExtensionsMenuDelegateAndroid::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(crbug.com/473213114)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onHostAccessRequestRemoved(env, java_object_,
                                                       extension_id);
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
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsMenuBridge_onModelChanged(env, java_object_);
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
  menu_model_->AllowHostAccessRequest(extension_id);
}

void ExtensionsMenuDelegateAndroid::OnDismissExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->DismissHostAccessRequest(extension_id);
}

void ExtensionsMenuDelegateAndroid::OnExtensionToggleSelected(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  if (is_on) {
    menu_model_->GrantSiteAccess(extension_id);
  } else {
    menu_model_->RevokeSiteAccess(extension_id);
  }
}

void ExtensionsMenuDelegateAndroid::OnShowRequestsTogglePressed(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  menu_model_->ShowHostAccessRequestsInToolbar(extension_id, is_on);
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
  menu_model_->ReloadWebContents();
}

void ExtensionsMenuDelegateAndroid::OnReloadPageButtonClicked(JNIEnv* env) {
  OnReloadPageButtonClicked();
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
