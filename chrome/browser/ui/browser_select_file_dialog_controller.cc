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
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

BrowserSelectFileDialogController::BrowserSelectFileDialogController(
    Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

BrowserSelectFileDialogController::~BrowserSelectFileDialogController() {
  if (select_file_dialog_.get()) {
    // There may be pending file dialogs, we need to tell them that we've gone
    // away so they don't try and call back to us.
    select_file_dialog_->ListenerDestroyed();
  }
}

void BrowserSelectFileDialogController::OpenFile(
    content::WebContents* web_contents,
    gfx::NativeWindow parent_window,
    FileSelectedCallback callback) {
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

  file_selected_callback_ = std::move(callback);

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

  if (url.is_empty()) {
    file_selected_callback_.Reset();
    return;
  }

  CHECK(file_selected_callback_);
  std::move(file_selected_callback_).Run(url);
}

void BrowserSelectFileDialogController::FileSelectionCanceled() {
  file_selected_callback_.Reset();
  select_file_dialog_.reset();
}
