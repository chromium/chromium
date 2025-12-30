// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_

#include <memory>

#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/models/dialog_model.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;
class ExtensionPostInstallDialogModel;

namespace content {
class WebContents;
}

namespace extensions {

// Shows the extension post-install dialog. This function is platform-agnostic.
void ShowExtensionPostInstallDialog(
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<ExtensionPostInstallDialogModel> model);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_
