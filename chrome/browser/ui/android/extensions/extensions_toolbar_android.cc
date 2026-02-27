// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_toolbar_android.h"

#include <cstdint>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia_rep.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionAction_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsToolbarBridge_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/RequestAccessButtonParams_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::WebContents;

namespace extensions {

ExtensionsToolbarAndroid::ExtensionsToolbarAndroid(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      toolbar_view_model_(std::make_unique<ExtensionsToolbarViewModel>(
          this,
          browser,
          ToolbarActionsModel::Get(browser_->GetProfile()))),
      scoped_toolbar_view_model_user_data_(browser->GetUnownedUserDataHost(),
                                           *toolbar_view_model_),
      java_object_(java_object) {
  toolbar_view_model_observation_.Observe(toolbar_view_model_.get());
}

ExtensionsToolbarAndroid::~ExtensionsToolbarAndroid() = default;

void ExtensionsToolbarAndroid::TriggerPopup(
    const ToolbarActionsModel::ActionId& action_id,
    std::unique_ptr<ExtensionViewHost> host) {
  Java_ExtensionsToolbarBridge_triggerPopup(
      AttachCurrentThread(), java_object_, action_id,
      reinterpret_cast<int64_t>(host.release()));
}

std::unique_ptr<ExtensionActionViewModel>
ExtensionsToolbarAndroid::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateAndroid>(browser_.get(),
                                                       action_id, this));
}

base::android::ScopedJavaLocalRef<jobject>
ExtensionsToolbarAndroid::GetRequestAccessButtonParams(
    JNIEnv* env,
    content::WebContents* web_contents) {
  ExtensionsToolbarViewModel::RequestAccessButtonParams params =
      toolbar_view_model_->GetRequestAccessButtonParams(web_contents);
  return Java_RequestAccessButtonParams_Constructor(env, params.extension_ids,
                                                    params.tooltip_text);
}

void ExtensionsToolbarAndroid::OnRequestAccessButtonParamsChanged(
    content::WebContents* web_contents) {
  Java_ExtensionsToolbarBridge_onRequestAccessButtonParamsChanged(
      AttachCurrentThread(), java_object_);
}

void ExtensionsToolbarAndroid::HideActivePopup() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
}

bool ExtensionsToolbarAndroid::CloseOverflowMenuIfOpen() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionsToolbarAndroid::CanShowToolbarActionPopupForAPICall(
    const std::string& action_id) {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
  return false;
}

void ExtensionsToolbarAndroid::ToggleExtensionsMenu() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
}

void ExtensionsToolbarAndroid::OnActionsInitialized() {
  for (const auto& action_id : toolbar_view_model_->GetAllActionIds()) {
    RegisterIconObserverForAction(action_id);
  }
  Java_ExtensionsToolbarBridge_onActionsInitialized(AttachCurrentThread(),
                                                    java_object_);
}

void ExtensionsToolbarAndroid::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  RegisterIconObserverForAction(action_id);
  Java_ExtensionsToolbarBridge_onActionAdded(AttachCurrentThread(),
                                             java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  icon_subscriptions_.erase(action_id);
  Java_ExtensionsToolbarBridge_onActionRemoved(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnPinnedActionsChanged() {
  Java_ExtensionsToolbarBridge_onPinnedActionsChanged(AttachCurrentThread(),
                                                      java_object_);
}

void ExtensionsToolbarAndroid::OnActiveWebContentsChanged(
    bool /*is_same_document*/) {
  Java_ExtensionsToolbarBridge_onActiveWebContentsChanged(AttachCurrentThread(),
                                                          java_object_);
}

void ExtensionsToolbarAndroid::OnToolbarControlStateUpdated() {
  Java_ExtensionsToolbarBridge_onToolbarControlStateUpdated(
      AttachCurrentThread(), java_object_);
}

void ExtensionsToolbarAndroid::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarAndroid::GetAction(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);
  return Java_ExtensionAction_Constructor(
      env, action_id, base::UTF16ToUTF8(action->GetActionName()));
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarAndroid::GetIcon(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents,
    int canvas_width_dp,
    int canvas_height_dp,
    float scale_factor) {
  gfx::Size size(canvas_width_dp, canvas_height_dp);

  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);
  DCHECK(action);
  ui::ImageModel model = action->GetIcon(web_contents, size);

  if (model.IsEmpty() || !model.IsImage()) {
    return nullptr;
  }

  gfx::ImageSkia image_skia = model.GetImage().AsImageSkia();

  const gfx::ImageSkiaRep& rep = image_skia.GetRepresentation(scale_factor);
  const SkBitmap& bitmap = rep.GetBitmap();

  if (bitmap.isNull()) {
    return nullptr;
  }

  return gfx::ConvertToJavaBitmap(bitmap);
}

std::vector<ToolbarActionsModel::ActionId>
ExtensionsToolbarAndroid::GetAllActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetAllActionIds();
  return std::vector(ids.begin(), ids.end());
}

std::vector<ToolbarActionsModel::ActionId>
ExtensionsToolbarAndroid::GetPinnedActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetPinnedActionIds();
  return std::vector(ids.begin(), ids.end());
}

int ExtensionsToolbarAndroid::GetExtensionsMenuButtonState(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return static_cast<int>(toolbar_view_model_->GetButtonState(*web_contents));
}

void ExtensionsToolbarAndroid::ExecuteUserAction(
    const ToolbarActionsModel::ActionId& action_id,
    ToolbarActionViewModel::InvocationSource source) {
  toolbar_view_model_->ExecuteUserAction(action_id, source);
}

void ExtensionsToolbarAndroid::RegisterIconObserverForAction(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action_model =
      toolbar_view_model_->GetActionModelForId(action_id);
  DCHECK(action_model);

  icon_subscriptions_.insert_or_assign(
      action_id, action_model->RegisterIconUpdateObserver(base::BindRepeating(
                     &ExtensionsToolbarAndroid::OnActionIconUpdated,
                     base::Unretained(this), action_id)));
}

void ExtensionsToolbarAndroid::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    int target_index) {
  toolbar_view_model_->MovePinnedAction(action_id, target_index);
}

static int64_t JNI_ExtensionsToolbarBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    int64_t j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<int64_t>(
      new ExtensionsToolbarAndroid(browser, java_object));
}

}  // namespace extensions

DEFINE_JNI(ExtensionAction)
DEFINE_JNI(ExtensionsToolbarBridge)
