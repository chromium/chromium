// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}

namespace gfx {
class ImageSkia;
}

namespace extensions {

// Shows a dialog to notify the user that the extension installation is
// blocked due to policy. It also shows additional information from
// administrator if it exists.
void ShowExtensionInstallBlockedDialog(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_H_
