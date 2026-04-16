// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"

#include <utility>

#include "base/notimplemented.h"
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
  // No-op. On Android, toolbar action executions (as well as named commands)
  // are both handled by `extension_keybinding_registry`, instead of by each
  // action.
}

void ExtensionActionDelegateAndroid::UnregisterCommand() {
  // No-op. On Android, toolbar action executions (as well as named commands)
  // are both handled by `extension_keybinding_registry`, instead of by each
  // action.
}

bool ExtensionActionDelegateAndroid::IsShowingPopup() const {
  return toolbar_android_->HasActivePopup();
}

void ExtensionActionDelegateAndroid::HidePopup() {
  toolbar_android_->HideActivePopup();
}

gfx::NativeView ExtensionActionDelegateAndroid::GetPopupNativeViewForTesting() {
  // Unused for Android tests.
  NOTIMPLEMENTED();
  return nullptr;
}

void ExtensionActionDelegateAndroid::TriggerPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    bool by_user,
    ShowPopupCallback callback) {
  toolbar_android_->TriggerPopup(action_id_, std::move(host));
}

void ExtensionActionDelegateAndroid::ShowContextMenuAsFallback() {
  toolbar_android_->ShowContextMenu(action_id_);
}

void ExtensionActionDelegateAndroid::CloseExtensionsMenuIfOpen() {
  toolbar_android_->CloseExtensionsMenuIfOpen();
}
