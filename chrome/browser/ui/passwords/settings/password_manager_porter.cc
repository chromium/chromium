// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_WIN)
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
// The default directory and filename when importing and exporting passwords.
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
#endif  // !IS_ANDROID

}  // namespace

PasswordManagerPorter::PasswordManagerPorter(
    Profile* profile,
    password_manager::SavedPasswordsPresenter* presenter,
    ExportProgressCallback on_export_progress_callback)
    : profile_(profile),
      presenter_(presenter),
      on_export_progress_callback_(on_export_progress_callback) {}

PasswordManagerPorter::~PasswordManagerPorter() {
  // There may be open file selection dialogs. We need to let them know that we
  // have gone away so that they do not attempt to call us back.
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

bool PasswordManagerPorter::Export(
    base::WeakPtr<content::WebContents> web_contents) {
  if (exporter_ && exporter_->GetProgressStatus() ==
                       password_manager::ExportProgressStatus::kInProgress) {
    return false;
  }

  if (!exporter_) {
    // Set a new exporter for this request.
    exporter_ = std::make_unique<password_manager::PasswordManagerExporter>(
        presenter_, on_export_progress_callback_,
        base::BindOnce(&PasswordManagerPorter::ExportDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Start serialising while the user selects a file.
  exporter_->PreparePasswordsForExport();
  PresentExportFileSelector(web_contents.get());

  return true;
}

void PasswordManagerPorter::CancelExport() {
  if (exporter_) {
    exporter_->Cancel();
  }
}

password_manager::ExportProgressStatus
PasswordManagerPorter::GetExportProgressStatus() {
  return exporter_ ? exporter_->GetProgressStatus()
                   : password_manager::ExportProgressStatus::kNotStarted;
}

void PasswordManagerPorter::SetExporterForTesting(
    std::unique_ptr<password_manager::PasswordManagerExporter> exporter) {
  exporter_ = std::move(exporter);
}

void PasswordManagerPorter::Import(
    content::WebContents* web_contents,
    password_manager::PasswordForm::Store to_store,
    ImportResultsCallback results_callback) {
  DCHECK(web_contents);
  if (!import_results_callback_.is_null() ||
      (importer_ &&
       (importer_->IsState(password_manager::PasswordImporter::kInProgress) ||
        importer_->IsState(password_manager::PasswordImporter::kConflicts)))) {
    // Early return to prevent crashes due to already active import process in
    // other window.
    password_manager::ImportResults results;
    results.status =
        password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE;

    // For consistency |results_callback| is always run asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(results_callback), results));
    return;
  }

  import_results_callback_ = std::move(results_callback);
  to_store_ = to_store;

  PresentImportFileSelector(web_contents);
}

void PasswordManagerPorter::ContinueImport(
    const std::vector<int>& selected_ids,
    ImportResultsCallback results_callback) {
  if (importer_ &&
      importer_->IsState(password_manager::PasswordImporter::kConflicts)) {
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
  password_manager::ImportResults results;
  if (importer_) {
    results.status =
        password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE;
  } else {
    results.status = password_manager::ImportResults::Status::UNKNOWN_ERROR;
  }

  // For consistency |results_callback| is always run asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(results_callback), results));
}

void PasswordManagerPorter::ResetImporter(bool delete_file) {
  // Importer can be reset only in kNotStarted, kFinished, kConflicts states,
  // but not in kInProgress.
  if (!importer_ ||
      importer_->IsState(password_manager::PasswordImporter::kInProgress)) {
    return;
  }
  if (delete_file &&
      importer_->IsState(password_manager::PasswordImporter::kFinished)) {
    // File deletion can only be triggered if the importer is in kFinished
    // state.
    importer_->DeleteFile();
  }
  importer_.reset();
}

void PasswordManagerPorter::SetImporterForTesting(
    std::unique_ptr<password_manager::PasswordImporter> importer) {
  importer_ = std::move(importer);
}

PasswordManagerPorter::ImportFileSelectListener::ImportFileSelectListener(
    PasswordManagerPorter* owner)
    : owner_(owner) {}

PasswordManagerPorter::ImportFileSelectListener::~ImportFileSelectListener() =
    default;

void PasswordManagerPorter::ImportFileSelectListener::FileSelected(
    const ui::SelectedFileInfo& file,
    int /* index */) {
  owner_->ImportPasswordsFromPath(file.path());
  owner_->select_file_dialog_.reset();
}

void PasswordManagerPorter::ImportFileSelectListener::FileSelectionCanceled() {
  if (owner_->import_results_callback_) {
    password_manager::ImportResults results;
    results.status = password_manager::ImportResults::Status::DISMISSED;
    std::move(owner_->import_results_callback_).Run(results);
  }
  owner_->select_file_dialog_.reset();
}

PasswordManagerPorter::ExportFileSelectListener::ExportFileSelectListener(
    PasswordManagerPorter* owner)
    : owner_(owner) {}

PasswordManagerPorter::ExportFileSelectListener::~ExportFileSelectListener() =
    default;

void PasswordManagerPorter::ExportFileSelectListener::FileSelected(
    const ui::SelectedFileInfo& file,
    int /* index */) {
  owner_->ExportPasswordsToPath(file.path());
  owner_->select_file_dialog_.reset();
}

void PasswordManagerPorter::ExportFileSelectListener::FileSelectionCanceled() {
  owner_->exporter_->Cancel();
  owner_->select_file_dialog_.reset();
}

#if !BUILDFLAG(IS_ANDROID)
static ui::SelectFileDialog::FileTypeInfo FileTypeInfoForImportExport() {
  ui::SelectFileDialog::FileTypeInfo info{{FILE_PATH_LITERAL("csv")}};
  info.include_all_files = true;
  return info;
}
#endif

void PasswordManagerPorter::PresentImportFileSelector(
    content::WebContents* web_contents) {
// This method should never be called on Android (as there is no file selector),
// and the relevant IDS constants are not present for Android.
#if !BUILDFLAG(IS_ANDROID)
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    return;
  }

  ui::SelectFileDialog::FileTypeInfo info = FileTypeInfoForImportExport();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      &import_listener_,
      std::make_unique<ChromeSelectFilePolicy>(web_contents));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE),
      GetDefaultFilepathForPasswordFile(info.extensions[0][0]), &info, 1,
      info.extensions[0][0], web_contents->GetTopLevelNativeWindow());
#endif
}

void PasswordManagerPorter::PresentExportFileSelector(
    content::WebContents* web_contents) {
// This method should never be called on Android (as there is no file selector),
// and the relevant IDS constants are not present for Android.
#if !BUILDFLAG(IS_ANDROID)
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    return;
  }

  ui::SelectFileDialog::FileTypeInfo info = FileTypeInfoForImportExport();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      &export_listener_,
      std::make_unique<ChromeSelectFilePolicy>(web_contents));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE),
      GetDefaultFilepathForPasswordFile(info.extensions[0][0]), &info, 1,
      info.extensions[0][0], web_contents->GetTopLevelNativeWindow(), nullptr);
#endif
}

void PasswordManagerPorter::ExportPasswordsToPath(const base::FilePath& path) {
  exporter_->SetDestination(path);
}

void PasswordManagerPorter::ExportDone() {
  exporter_.reset();
}

void PasswordManagerPorter::ImportPasswordsFromPath(
    const base::FilePath& path) {
  DCHECK(!import_results_callback_.is_null());
  if (!importer_) {
    importer_ =
        std::make_unique<password_manager::PasswordImporter>(presenter_);
  }
  importer_->Import(path, to_store_, std::move(import_results_callback_));
}
