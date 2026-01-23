// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_toolbar_bridge.h"

#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia_rep.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionAction_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsToolbarBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace extensions {

ExtensionsToolbarBridge::ExtensionsToolbarBridge(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      toolbar_view_model_(std::make_unique<ExtensionsToolbarViewModel>(
          this,
          browser,
          ToolbarActionsModel::Get(browser_->GetProfile()))),
      java_object_(java_object) {
  toolbar_view_model_observation_.Observe(toolbar_view_model_.get());
}

ExtensionsToolbarBridge::~ExtensionsToolbarBridge() = default;

void ExtensionsToolbarBridge::TriggerPopup(
    const ToolbarActionsModel::ActionId& action_id,
    std::unique_ptr<ExtensionViewHost> host) {
  Java_ExtensionsToolbarBridge_triggerPopup(
      AttachCurrentThread(), java_object_, action_id,
      reinterpret_cast<jlong>(host.release()));
}

std::unique_ptr<ExtensionActionViewModel>
ExtensionsToolbarBridge::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateAndroid>(browser_.get(),
                                                       action_id, this));
}

void ExtensionsToolbarBridge::HideActivePopup() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
}

bool ExtensionsToolbarBridge::CloseOverflowMenuIfOpen() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionsToolbarBridge::CanShowToolbarActionPopupForAPICall(
    const std::string& action_id) {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
  return false;
}

void ExtensionsToolbarBridge::ToggleExtensionsMenu() {
  // TODO(crbug.com/461981075)
  NOTIMPLEMENTED();
}

void ExtensionsToolbarBridge::OnActionsInitialized() {
  for (const auto& action_id : toolbar_view_model_->GetAllActionIds()) {
    RegisterIconObserverForAction(action_id);
  }
  Java_ExtensionsToolbarBridge_onActionsInitialized(AttachCurrentThread(),
                                                    java_object_);
}

void ExtensionsToolbarBridge::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  RegisterIconObserverForAction(action_id);
  Java_ExtensionsToolbarBridge_onActionAdded(AttachCurrentThread(),
                                             java_object_, action_id);
}

void ExtensionsToolbarBridge::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  icon_subscriptions_.erase(action_id);
  Java_ExtensionsToolbarBridge_onActionRemoved(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarBridge::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarBridge::OnPinnedActionsChanged() {
  Java_ExtensionsToolbarBridge_onPinnedActionsChanged(AttachCurrentThread(),
                                                      java_object_);
}

void ExtensionsToolbarBridge::OnActiveWebContentsChanged() {
  // TODO(crbug.c/476295562)
}

void ExtensionsToolbarBridge::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarBridge::GetAction(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);
  return Java_ExtensionAction_Constructor(
      env, action_id, base::UTF16ToUTF8(action->GetActionName()));
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarBridge::GetIcon(
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
ExtensionsToolbarBridge::GetAllActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetAllActionIds();
  return std::vector(ids.begin(), ids.end());
}

std::vector<ToolbarActionsModel::ActionId>
ExtensionsToolbarBridge::GetPinnedActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetPinnedActionIds();
  return std::vector(ids.begin(), ids.end());
}

void ExtensionsToolbarBridge::ExecuteUserAction(
    const ToolbarActionsModel::ActionId& action_id,
    ToolbarActionViewModel::InvocationSource source) {
  toolbar_view_model_->ExecuteUserAction(action_id, source);
}

void ExtensionsToolbarBridge::RegisterIconObserverForAction(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action_model =
      toolbar_view_model_->GetActionModelForId(action_id);
  DCHECK(action_model);

  icon_subscriptions_.insert_or_assign(
      action_id, action_model->RegisterIconUpdateObserver(base::BindRepeating(
                     &ExtensionsToolbarBridge::OnActionIconUpdated,
                     base::Unretained(this), action_id)));
}

void ExtensionsToolbarBridge::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

static int64_t JNI_ExtensionsToolbarBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    int64_t j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<int64_t>(
      new ExtensionsToolbarBridge(browser, java_object));
}

}  // namespace extensions

DEFINE_JNI(ExtensionAction)
DEFINE_JNI(ExtensionsToolbarBridge)
