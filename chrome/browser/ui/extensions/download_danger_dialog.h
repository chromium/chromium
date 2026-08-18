// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_DANGER_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_DANGER_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}

namespace download {
class DownloadItem;
}

namespace extensions {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDownloadDangerDialogCancelButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDownloadDangerDialogKeepButtonElementId);

// Shows a dialog that prompts the user for whether to accept a dangerous
// DownloadItem using native UI. This step is necessary to prevent a malicious
// extension from accepting a dangerous download.
void ShowDownloadDangerDialog(
    download::DownloadItem* download_item,
    content::WebContents* web_contents,
    base::OnceCallback<void(DownloadDangerPrompt::Action)> done_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_DOWNLOAD_DANGER_DIALOG_H_
