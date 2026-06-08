// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/settings/password_export_controller_interface.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct PasswordExportInfo;
}

class Profile;

// Handles the exporting of passwords to a file.
class PasswordExportController : public PasswordExportControllerInterface,
                                 public ui::SelectFileDialog::Listener {
 public:
  using ExportProgressCallback = base::RepeatingCallback<void(
      const password_manager::PasswordExportInfo&)>;

  // |profile| for which credentials to be exported.
  // |presenter| provides the credentials which can be exported.
  // |on_export_progress_callback| will be called with updates to the progress
  // of exporting.
  PasswordExportController(Profile* profile,
                           password_manager::SavedPasswordsPresenter* presenter,
                           ExportProgressCallback on_export_progress_callback);

  PasswordExportController(const PasswordExportController&) = delete;
  PasswordExportController& operator=(const PasswordExportController&) = delete;

  ~PasswordExportController() override;

  // PasswordExportControllerInterface:
  bool Export(base::WeakPtr<content::WebContents> web_contents) override;
  void CancelExport() override;
  password_manager::ExportProgressStatus GetExportProgressStatus() override;

  // The next export will use |exporter|, instead of creating a new instance.
  void SetExporterForTesting(  // IN-TEST
      std::unique_ptr<password_manager::PasswordManagerExporter> exporter);

 private:
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // Show the platform file selection dialog for exporting. This delivers
  // callbacks via ExportFileSelectListener (export_listener_).
  void PresentExportFileSelector(content::WebContents* web_contents);

  void ExportPasswordsToPath(const base::FilePath& path);

  void ExportDone();

  std::unique_ptr<password_manager::PasswordManagerExporter> exporter_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<Profile> profile_;

  // We store |presenter_| and
  // |on_export_progress_callback_| to use them to create a new
  // PasswordManagerExporter instance for each export.
  const raw_ptr<password_manager::SavedPasswordsPresenter> presenter_;
  ExportProgressCallback on_export_progress_callback_;

  base::WeakPtrFactory<PasswordExportController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_H_
