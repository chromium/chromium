// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_export_controller.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/branded_strings.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

// The default directory and filename when exporting passwords.
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

ui::SelectFileDialog::FileTypeInfo FileTypeInfoForExport() {
  ui::SelectFileDialog::FileTypeInfo info{{FILE_PATH_LITERAL("csv")}};
  info.include_all_files = true;
  return info;
}

}  // namespace

PasswordExportController::PasswordExportController(
    password_manager::SavedPasswordsPresenter* presenter,
    ExportProgressCallback on_export_progress_callback)
    : presenter_(presenter),
      on_export_progress_callback_(on_export_progress_callback) {}

PasswordExportController::~PasswordExportController() {
  // There may be open file selection dialogs. We need to let them know that we
  // have gone away so that they do not attempt to call us back.
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

bool PasswordExportController::Export(
    base::WeakPtr<content::WebContents> web_contents) {
  if (exporter_ && exporter_->GetProgressStatus() ==
                       password_manager::ExportProgressStatus::kInProgress) {
    return false;
  }

  if (!exporter_) {
    // Set a new exporter for this request.
    exporter_ = std::make_unique<password_manager::PasswordManagerExporter>(
        presenter_, on_export_progress_callback_,
        base::BindOnce(&PasswordExportController::ExportDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Start serialising while the user selects a file.
  exporter_->PreparePasswordsForExport();
  PresentExportFileSelector(web_contents.get());

  return true;
}

void PasswordExportController::CancelExport() {
  if (exporter_) {
    exporter_->Cancel();
  }
}

password_manager::ExportProgressStatus
PasswordExportController::GetExportProgressStatus() {
  return exporter_ ? exporter_->GetProgressStatus()
                   : password_manager::ExportProgressStatus::kNotStarted;
}

void PasswordExportController::SetExporterForTesting(  // IN-TEST
    std::unique_ptr<password_manager::PasswordManagerExporter> exporter) {
  exporter_ = std::move(exporter);
}

void PasswordExportController::FileSelected(const ui::SelectedFileInfo& file,
                                            int /* index */) {
  ExportPasswordsToPath(file.path());
  select_file_dialog_.reset();
}

void PasswordExportController::FileSelectionCanceled() {
  exporter_->Cancel();
  select_file_dialog_.reset();
}

void PasswordExportController::PresentExportFileSelector(
    content::WebContents* web_contents) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    return;
  }

  ui::SelectFileDialog::FileTypeInfo info = FileTypeInfoForExport();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE),
      GetDefaultFilepathForPasswordFile(info.extensions[0][0]), &info, 1,
      info.extensions[0][0], web_contents->GetTopLevelNativeWindow(), nullptr);
}

void PasswordExportController::ExportPasswordsToPath(
    const base::FilePath& path) {
  exporter_->SetDestination(path);
}

void PasswordExportController::ExportDone() {
  exporter_.reset();
}
