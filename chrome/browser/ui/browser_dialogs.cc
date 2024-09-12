// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"

#if defined(TOOLKIT_VIEWS)
#include "components/constrained_window/constrained_window_views.h"
#else
#include "ui/shell_dialogs/selected_file_info.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/web_contents.h"
#include "ui/android/modal_dialog_wrapper.h"
#include "ui/android/window_android.h"
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

#if defined(TOOLKIT_VIEWS)
void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                  content::WebContents* web_contents) {
  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}
#elif BUILDFLAG(IS_ANDROID)
void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                  content::WebContents* web_contents) {
  ui::WindowAndroid* window = web_contents->GetTopLevelNativeWindow();
  ui::ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window);
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
