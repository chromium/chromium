// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_

#include <memory>

#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "ui/gfx/native_ui_types.h"

class ExtensionActionViewController;

namespace extensions {
class ExtensionViewHost;
}  // namespace extensions

class ExtensionActionPlatformDelegate {
 public:
  virtual ~ExtensionActionPlatformDelegate() = default;

  // Attaches the delegate to an ExtensionActionViewController. It is called
  // by the controller on its constructor.
  virtual void AttachToController(
      ExtensionActionViewController* controller) = 0;

  // Detaches the delegate from an ExtensionActionViewController. It is called
  // by the controller on its destructor.
  virtual void DetachFromController() = 0;

  // The following are forwarded from ToolbarActionViewController. See that
  // class for the definitions.
  virtual void RegisterCommand() = 0;
  virtual void UnregisterCommand() = 0;

  // Returns whether there is currently a popup visible.
  virtual bool IsShowingPopup() const = 0;

  // Hides the current popup, if one is visible.
  virtual void HidePopup() = 0;

  // Returns the native view for the popup, if one is active.
  virtual gfx::NativeView GetPopupNativeView() = 0;

  // Begins the process of showing the popup for the extension action on the
  // current web contents. |by_user| is true if popup is being triggered by a
  // user action.
  // The popup may not be shown synchronously if the extension is hidden and
  // first needs to slide itself out.
  virtual void TriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                            PopupShowAction show_action,
                            bool by_user,
                            ShowPopupCallback callback) = 0;

  // Shows the context menu for the action as a fallback for performing another
  // action.
  virtual void ShowContextMenuAsFallback() = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
