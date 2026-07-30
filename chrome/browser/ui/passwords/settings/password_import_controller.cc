// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_import_controller.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

using password_manager::ImportResults;
using password_manager::PasswordForm;
using password_manager::PasswordImporter;
using password_manager::SavedPasswordsPresenter;

namespace {

// The default directory and filename when importing passwords.
base::FilePath GetDefaultFilepathForPasswordFile(
    const base::FilePath::StringType& default_extension) {
  base::FilePath default_path;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
#if BUILDFLAG(IS_WIN)
  std::wstring file_name = base::UTF8ToWide(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_DEFAULT_EXPORT_FILENAME));
#else
  std::string file_name =
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_DEFAULT_EXPORT_FILENAME);
#endif
  return default_path.Append(file_name).AddExtension(default_extension);
}

ui::SelectFileDialog::FileTypeInfo FileTypeInfoForImport() {
  ui::SelectFileDialog::FileTypeInfo info{{FILE_PATH_LITERAL("csv")}};
  info.include_all_files = true;
  return info;
}

void PostAsyncResult(
    PasswordImportController::ImportResultsCallback callback,
    ImportResults::Status status) {
  ImportResults results;
  results.status = status;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

}  // namespace

PasswordImportController::PasswordImportController(
    SavedPasswordsPresenter* presenter)
    : presenter_(presenter) {
  CHECK(presenter_);
}

PasswordImportController::~PasswordImportController() {
  // There may be open file selection dialogs. We need to let them know that we
  // have gone away so that they do not attempt to call us back.
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void PasswordImportController::Import(
    content::WebContents* web_contents,
    PasswordForm::Store to_store,
    ImportResultsCallback results_callback) {
  CHECK(web_contents);
  if (!import_results_callback_.is_null() ||
      (importer_ &&
       (importer_->IsState(PasswordImporter::kInProgress) ||
        importer_->IsState(
            PasswordImporter::kUserInteractionRequired)))) {
    // Early return to prevent crashes due to already active import process in
    // other window.
    // For consistency |results_callback| is always run asynchronously.
    PostAsyncResult(std::move(results_callback),
                    ImportResults::Status::IMPORT_ALREADY_ACTIVE);
    return;
  }

  import_results_callback_ = std::move(results_callback);
  to_store_ = to_store;

  PresentImportFileSelector(web_contents);
}

void PasswordImportController::ContinueImport(
    const std::vector<int>& selected_ids,
    ImportResultsCallback results_callback) {
  if (importer_ &&
      importer_->IsState(
          PasswordImporter::kUserInteractionRequired)) {
    importer_->ContinueImport(selected_ids, std::move(results_callback));
    return;
  }
  // Respond with `IMPORT_ALREADY_ACTIVE`, when `PasswordImporter` is available
  // and not in the `CONFLICTS` state. Otherwise, return `UNKNOWN_ERROR`.
  // This code can be reached in 2 cases:
  // 1) `chrome.passwordsPrivate.continueImport` is called from the dev console.
  //    This should prevent crashing the browser by calling the private API.
  // 2) Import state is not synced across tabs, hence if import has been
  // launched from one window, but then continued from another window. If the
  // user also continues in the original window, we reach this code.
  // For consistency |results_callback| is always run asynchronously.
  PostAsyncResult(std::move(results_callback),
                  importer_ ? ImportResults::Status::IMPORT_ALREADY_ACTIVE
                            : ImportResults::Status::UNKNOWN_ERROR);
}

void PasswordImportController::ResetImporter(bool delete_file) {
  // Importer can be reset only in kNotStarted, kFinished,
  // kUserInteractionRequired states, but not in kInProgress.
  if (!importer_ ||
      importer_->IsState(PasswordImporter::kInProgress)) {
    return;
  }
  if (delete_file &&
      importer_->IsState(PasswordImporter::kFinished)) {
    // File deletion can only be triggered if the importer is in kFinished
    // state.
    importer_->DeleteFile();
  }
  importer_.reset();
}

void PasswordImportController::SetImporterForTesting(  // IN-TEST
    std::unique_ptr<PasswordImporter> importer) {
  importer_ = std::move(importer);
}

void PasswordImportController::FileSelected(const ui::SelectedFileInfo& file,
                                            int /* index */) {
  ImportPasswordsFromPath(file.path());
  select_file_dialog_.reset();
}

void PasswordImportController::FileSelectionCanceled() {
  if (import_results_callback_) {
    // For consistency |import_results_callback_| is always run asynchronously.
    PostAsyncResult(std::move(import_results_callback_),
                    ImportResults::Status::DISMISSED);
  }
  select_file_dialog_.reset();
}

void PasswordImportController::PresentImportFileSelector(
    content::WebContents* web_contents) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    return;
  }

  ui::SelectFileDialog::FileTypeInfo info = FileTypeInfoForImport();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE),
      GetDefaultFilepathForPasswordFile(info.extensions[0][0]), &info, 1,
      info.extensions[0][0], web_contents->GetTopLevelNativeWindow());
}

void PasswordImportController::ImportPasswordsFromPath(
    const base::FilePath& path) {
  CHECK(!import_results_callback_.is_null());
  if (!importer_) {
    importer_ =
        std::make_unique<PasswordImporter>(presenter_);
  }
  importer_->Import(path, to_store_, std::move(import_results_callback_));
}
