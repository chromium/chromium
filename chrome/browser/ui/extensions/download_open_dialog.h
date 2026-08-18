// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_OPEN_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_OPEN_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace extensions {

// Shows a dialog that prompts the user for whether to open a DownloadItem using
// native UI. This step is necessary to prevent a malicious extension from
// opening any downloaded file.
void ShowDownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> open_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_OPEN_DIALOG_H_
