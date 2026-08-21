// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_REQUEST_FILE_SYSTEM_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_REQUEST_FILE_SYSTEM_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "ui/base/mojom/dialog_button.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)

namespace content {
class WebContents;
}

namespace extensions {

// Shows a dialog requesting the user to grant the extension access to a file
// system.
void ShowRequestFileSystemDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::mojom::DialogButton)> callback);

}  // namespace extensions

#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_REQUEST_FILE_SYSTEM_DIALOG_H_
