// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_ASK_PARENT_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_ASK_PARENT_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}

namespace extensions {

// Shows a dialog to notify the user that they need to ask their parent for
// approval to install an extension. This is the first of a set of dialogs for
// supervised user accounts on Android.
void ShowExtensionInstallAskParentDialog(content::WebContents* web_contents,
                                         base::OnceClosure cancel_callback,
                                         base::OnceClosure approve_callback);

}  // namespace extensions

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_ASK_PARENT_DIALOG_H_
