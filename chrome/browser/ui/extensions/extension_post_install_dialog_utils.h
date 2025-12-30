// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

// TODO(crbug.com/450296898): Evaluate if this file can be consolidated with
// extension_post_install_dialog.h or if the functions here should
// be moved to a more appropriate location, such as ExtensionInstallUI.

class Profile;
class SkBitmap;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// Triggers the post-install dialog for an extension.
void TriggerPostInstallDialog(
    Profile* profile,
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap& icon,
    base::OnceCallback<content::WebContents*()> get_web_contents_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_UTILS_H_
