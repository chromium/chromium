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

#if BUILDFLAG(IS_WIN)
#endif

namespace {

// The following are not used on Android due to the |SelectFileDialog| being
// unused.
#if !BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

// Returns the file extensions corresponding to supported formats.
// Inner vector indicates equivalent extensions. For example:
//   { { "html", "htm" }, { "csv" } }
std::vector<std::vector<base::FilePath::StringType>>
GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

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
#endif

}  // namespace

PasswordManagerPorter::PasswordManagerPorter(
    Profile* profile,
    password_manager::SavedPasswordsPresenter* presenter,
    ExportProgressCallback on_export_progress_callback)
    : profile_(profile),
      presenter_(presenter),
      on_export_progress_callback_(on_export_progress_callback) {}

PasswordManagerPorter::~PasswordManagerPorter() = default;

bool PasswordManagerPorter::Export(content::WebContents* web_contents) {
  if (exporter_ && exporter_->GetProgressStatus() ==
                       password_manager::ExportProgressStatus::IN_PROGRESS) {
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
  PresentFileSelector(web_contents,
                      PasswordManagerPorter::Type::PASSWORD_EXPORT);

  return true;
}

void PasswordManagerPorter::CancelExport() {
  if (exporter_)
    exporter_->Cancel();
}

password_manager::ExportProgressStatus
PasswordManagerPorter::GetExportProgressStatus() {
  return exporter_ ? exporter_->GetProgressStatus()
                   : password_manager::ExportProgressStatus::NOT_STARTED;
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
      (importer_ && importer_->IsRunning())) {
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

  PresentFileSelector(web_contents,
                      PasswordManagerPorter::Type::PASSWORD_IMPORT);
}

void PasswordManagerPorter::SetImporterForTesting(
    std::unique_ptr<password_manager::PasswordImporter> importer) {
  importer_ = std::move(importer);
}

void PasswordManagerPorter::PresentFileSelector(
    content::WebContents* web_contents,
    Type type) {
// This method should never be called on Android (as there is no file selector),
// and the relevant IDS constants are not present for Android.
#if !BUILDFLAG(IS_ANDROID)
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  DCHECK(web_contents);

  // Get the default file extension for password files.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = GetSupportedFileExtensions();
  DCHECK(!file_type_info.extensions.empty());
  DCHECK(!file_type_info.extensions[0].empty());
  file_type_info.include_all_files = true;

  // Present the file selector dialogue.
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::Type file_selector_mode =
      ui::SelectFileDialog::SELECT_NONE;
  unsigned title = 0;
  switch (type) {
    case PASSWORD_IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE;
      title = IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE;
      break;
    case PASSWORD_EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE;
      break;
  }
  // Check that a valid action has been chosen.
  DCHECK(file_selector_mode);
  DCHECK(title);

  select_file_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title),
      GetDefaultFilepathForPasswordFile(file_type_info.extensions[0][0]),
      &file_type_info, 1, file_type_info.extensions[0][0],
      web_contents->GetTopLevelNativeWindow(), reinterpret_cast<void*>(type));
#endif
}

void PasswordManagerPorter::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  switch (reinterpret_cast<uintptr_t>(params)) {
    case PASSWORD_IMPORT:
      ImportPasswordsFromPath(path);
      break;
    case PASSWORD_EXPORT:
      ExportPasswordsToPath(path);
      break;
  }

  select_file_dialog_.reset();
}

void PasswordManagerPorter::FileSelectionCanceled(void* params) {
  if (reinterpret_cast<uintptr_t>(params) == PASSWORD_EXPORT) {
    exporter_->Cancel();
  }

  if (!import_results_callback_.is_null()) {
    password_manager::ImportResults results;
    results.status = password_manager::ImportResults::Status::DISMISSED;
    std::move(import_results_callback_).Run(results);
  }

  select_file_dialog_.reset();
}

void PasswordManagerPorter::ExportPasswordsToPath(const base::FilePath& path) {
  exporter_->SetDestination(path);
}

void PasswordManagerPorter::ExportDone() {
  exporter_.reset();
}

void PasswordManagerPorter::ImportDone() {
  importer_.reset();
}

void PasswordManagerPorter::ImportPasswordsFromPath(
    const base::FilePath& path) {
  DCHECK(!import_results_callback_.is_null());
  if (!importer_) {
    importer_ =
        std::make_unique<password_manager::PasswordImporter>(presenter_);
  }
  importer_->Import(path, to_store_, std::move(import_results_callback_),
                    base::BindOnce(&PasswordManagerPorter::ImportDone,
                                   weak_ptr_factory_.GetWeakPtr()));
}
