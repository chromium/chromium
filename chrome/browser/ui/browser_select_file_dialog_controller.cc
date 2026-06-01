// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_select_file_dialog_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "ui/base/base_window.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

BrowserSelectFileDialogController::BrowserSelectFileDialogController(
    Profile* profile,
    TabStripModel* tab_strip_model,
    ui::BaseWindow* base_window,
    content::PageNavigator* page_navigator)
    : profile_(CHECK_DEREF(profile)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)),
      base_window_(CHECK_DEREF(base_window)),
      page_navigator_(CHECK_DEREF(page_navigator)) {}

BrowserSelectFileDialogController::~BrowserSelectFileDialogController() {
  if (select_file_dialog_.get()) {
    // There may be pending file dialogs, we need to tell them that we've gone
    // away so they don't try and call back to us.
    select_file_dialog_->ListenerDestroyed();
  }
}

void BrowserSelectFileDialogController::OpenFile() {
  content::WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
  gfx::NativeWindow parent_window = base_window_->GetNativeWindow();

  // Ignore if there is already a select file dialog running.
  if (select_file_dialog_) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("OpenFile"));
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  if (!select_file_dialog_) {
    return;
  }

  const base::FilePath directory = profile_->last_selected_directory();
  // TODO(beng): figure out how to juggle this.
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  std::u16string(), directory, &file_types, 0,
                                  base::FilePath::StringType(), parent_window);
}

void BrowserSelectFileDialogController::FileSelected(
    const ui::SelectedFileInfo& file_info,
    int index) {
  // Transfer the ownership of select file dialog so that the ref count is
  // released after the function returns. This is needed because the passed-in
  // data such as |file_info| and |params| could be owned by the dialog.
  scoped_refptr<ui::SelectFileDialog> dialog = std::move(select_file_dialog_);

  profile_->set_last_selected_directory(file_info.file_path.DirName());

  const GURL url =
      file_info.url.value_or(net::FilePathToFileURL(file_info.local_path));

  if (!url.is_empty()) {
    page_navigator_->OpenURL(
        content::OpenURLParams(
            url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PAGE_TRANSITION_TYPED, /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }
}

void BrowserSelectFileDialogController::FileSelectionCanceled() {
  select_file_dialog_.reset();
}
