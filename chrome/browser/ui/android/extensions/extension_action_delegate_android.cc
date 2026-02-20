// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"

#include <utility>

#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"

using extensions::ActionInfo;

ExtensionActionDelegateAndroid::ExtensionActionDelegateAndroid(
    BrowserWindowInterface* browser,
    const ToolbarActionsModel::ActionId& action_id,
    extensions::ExtensionsToolbarAndroid* toolbar_android)
    : browser_(browser),
      action_id_(action_id),
      toolbar_android_(toolbar_android) {}

ExtensionActionDelegateAndroid::~ExtensionActionDelegateAndroid() = default;

void ExtensionActionDelegateAndroid::AttachToModel(
    ExtensionActionViewModel* model) {
  CHECK(model);
  CHECK(!model_);
  model_ = model;
}

void ExtensionActionDelegateAndroid::DetachFromModel() {
  CHECK(model_);
  model_ = nullptr;
}

void ExtensionActionDelegateAndroid::RegisterCommand() {
  // TODO(crbug.com/461981075)
}

void ExtensionActionDelegateAndroid::UnregisterCommand() {
  // TODO(crbug.com/461981075)
}

bool ExtensionActionDelegateAndroid::IsShowingPopup() const {
  // TODO(crbug.com/461981075)
  return false;
}

void ExtensionActionDelegateAndroid::HidePopup() {
  if (!toolbar_android_) {
    // TODO(crbug.com/461981075): Remove this check once
    // `ExtensionsMenuDelegateAndroid` passes a correct `toolbar_android_`
    // instead of `nullptr`.
    return;
  }

  toolbar_android_->HideActivePopup();
}

gfx::NativeView ExtensionActionDelegateAndroid::GetPopupNativeView() {
  // TODO(crbug.com/461981075)
  return nullptr;
}

void ExtensionActionDelegateAndroid::TriggerPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    bool by_user,
    ShowPopupCallback callback) {
  if (!toolbar_android_) {
    // TODO(crbug.com/461981075): Remove this check once
    // `ExtensionsMenuDelegateAndroid` passes a correct `toolbar_android_`
    // instead of `nullptr`.
    return;
  }

  toolbar_android_->TriggerPopup(action_id_, std::move(host));
}

void ExtensionActionDelegateAndroid::ShowContextMenuAsFallback() {
  // TODO(crbug.com/461981075)
}

bool ExtensionActionDelegateAndroid::CloseOverflowMenuIfOpen() {
  // TODO(crbug.com/461981075)
  return false;
}
