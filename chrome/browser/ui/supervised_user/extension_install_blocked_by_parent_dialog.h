// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_H_
#define CHROME_BROWSER_UI_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kParentBlockedDialogMessage);

// The type of action that the ExtensionInstalledBlockedByParentDialog
// is being shown in reaction to.
enum class ExtensionInstalledBlockedByParentDialogAction {
  kAdd,     // The user attempted to add the extension.
  kEnable,  // The user attempted to enable the extension.
};

// Displays a dialog to notify the user that the extension installation is
// blocked by a parent
void ShowExtensionInstallBlockedByParentDialog(
    ExtensionInstalledBlockedByParentDialogAction action,
    const Extension* extension,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_H_
