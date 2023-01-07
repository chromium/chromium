// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"

#if !defined(TOOLKIT_VIEWS)
#include "ui/shell_dialogs/selected_file_info.h"
#endif

namespace chrome {

#if !defined(TOOLKIT_VIEWS)
void ShowWindowNamePrompt(Browser* browser) {
  NOTIMPLEMENTED();
}

void ShowWindowNamePromptForTesting(Browser* browser,
                                    gfx::NativeWindow context) {
  NOTIMPLEMENTED();
}
#endif

}  // namespace chrome

#if !defined(TOOLKIT_VIEWS)
// There's no dialog version of this available outside views, run callback as if
// the dialog was instantly accepted.
void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents) {
  std::move(callback).Run(selected_files);
}
#endif  // !defined(TOOLKIT_VIEWS)
