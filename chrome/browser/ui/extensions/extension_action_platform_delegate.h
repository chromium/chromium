// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

namespace extensions {
class ExtensionViewHost;
}

class ExtensionActionPlatformDelegate {
 public:
  virtual ~ExtensionActionPlatformDelegate() {}

  // Returns a created ExtensionActionPlatformDelegate. This is defined in the
  // platform-specific implementation for the class.
  static std::unique_ptr<ExtensionActionPlatformDelegate> Create(
      ExtensionActionViewController* controller);

  // The following are forwarded from ToolbarActionViewController. See that
  // class for the definitions.
  virtual void RegisterCommand() = 0;

  // Called once the delegate is set, in order to do any extra initialization.
  virtual void OnDelegateSet() {}

  // Shows the given |host|. |grant_tab_permissions| is true if active tab
  // permissions should be given to the extension; this is only true if the
  // popup is opened through a user action.
  virtual void ShowPopup(
      std::unique_ptr<extensions::ExtensionViewHost> host,
      bool grant_tab_permissions,
      ExtensionActionViewController::PopupShowAction show_action) = 0;

  // Shows the context menu for the extension.
  virtual void ShowContextMenu() = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
