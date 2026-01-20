// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_toolbar_bridge.h"

#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsToolbarBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace extensions {

ExtensionsToolbarBridge::ExtensionsToolbarBridge(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      toolbar_view_model_(std::make_unique<ExtensionsToolbarViewModel>(
          this,
          ToolbarActionsModel::Get(browser_->GetProfile()))),
      java_object_(java_object) {
  toolbar_view_model_observation_.Observe(toolbar_view_model_.get());
}

ExtensionsToolbarBridge::~ExtensionsToolbarBridge() = default;

std::unique_ptr<ExtensionActionViewModel>
ExtensionsToolbarBridge::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateAndroid>(browser_.get()));
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
  Java_ExtensionsToolbarBridge_onActionsInitialized(AttachCurrentThread(),
                                                    java_object_);
}

void ExtensionsToolbarBridge::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionAdded(AttachCurrentThread(),
                                             java_object_, action_id);
}

void ExtensionsToolbarBridge::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
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

void ExtensionsToolbarBridge::Destroy(JNIEnv* env) {
  delete this;
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

DEFINE_JNI(ExtensionsToolbarBridge)
