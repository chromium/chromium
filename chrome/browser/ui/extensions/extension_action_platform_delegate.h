// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_

#include <memory>

#include "chrome/browser/ui/extensions/extension_popup_types.h"

class ExtensionActionViewController;

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
  virtual void UnregisterCommand() = 0;

  // Shows the given |host| in an extension popup.
  virtual void ShowPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                         PopupShowAction show_action,
                         ShowPopupCallback callback) = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_H_
