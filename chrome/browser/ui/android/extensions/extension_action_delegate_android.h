// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/android/extensions/extensions_toolbar_android.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"

class BrowserWindowInterface;

namespace extensions {
class ExtensionViewHost;
}  // namespace extensions

// Implements Android-specific extension action UI logic, such as showing the
// action's popup and the context menu.
class ExtensionActionDelegateAndroid : public ExtensionActionDelegate {
 public:
  ExtensionActionDelegateAndroid(
      BrowserWindowInterface* browser,
      const ToolbarActionsModel::ActionId& action_id,
      extensions::ExtensionsToolbarAndroid* toolbar_android);
  ExtensionActionDelegateAndroid(const ExtensionActionDelegateAndroid&) =
      delete;
  ExtensionActionDelegateAndroid& operator=(
      const ExtensionActionDelegateAndroid&) = delete;
  ~ExtensionActionDelegateAndroid() override;

 private:
  // ExtensionActionDelegate:
  void AttachToModel(ExtensionActionViewModel* model) override;
  void DetachFromModel() override;
  void RegisterCommand() override;
  void UnregisterCommand() override;
  bool IsShowingPopup() const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  void TriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                    PopupShowAction show_action,
                    bool by_user,
                    ShowPopupCallback callback) override;
  void ShowContextMenuAsFallback() override;
  bool CloseOverflowMenuIfOpen() override;

  // The corresponding browser window.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The ID for this action.
  const ToolbarActionsModel::ActionId action_id_;

  // The JNI bridge to communicate with the Java side.
  const raw_ptr<extensions::ExtensionsToolbarAndroid> toolbar_android_;

  // The platform-agnostic view model.
  raw_ptr<ExtensionActionViewModel> model_{nullptr};
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_DELEGATE_ANDROID_H_
