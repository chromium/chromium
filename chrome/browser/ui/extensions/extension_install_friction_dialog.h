// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}

namespace extensions {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kExtensionInstallFrictionLearnMoreLink);

// Shows a modal dialog to Enhanced Safe Browsing users before the extension
// install dialog if the extension is not included in the Safe Browsing CRX
// allowlist. `callback` will be invoked with `true` if the user accepts or
// `false` if the user cancels the dialog.
void ShowExtensionInstallFrictionDialog(
    content::WebContents* contents,
    base::OnceCallback<void(bool)> callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_H_
