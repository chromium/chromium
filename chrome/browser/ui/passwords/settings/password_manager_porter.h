// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

namespace password_manager {
class SavedPasswordsPresenter;
class PasswordManagerExporter;
}  // namespace password_manager

class Profile;

// Handles the exporting of passwords to a file, and the importing of such a
// file to the Password Manager.
class PasswordManagerPorter : public ui::SelectFileDialog::Listener {
 public:
  using ProgressCallback =
      base::RepeatingCallback<void(password_manager::ExportProgressStatus,
                                   const std::string&)>;

  // |profile| for which credentials to be importerd.
  // |presenter| provides the credentials which can be exported.
  // |on_export_progress_callback| will be called with updates to the progress
  // of exporting.
  PasswordManagerPorter(Profile* profile,
                        password_manager::SavedPasswordsPresenter* presenter,
                        ProgressCallback on_export_progress_callback);

  PasswordManagerPorter(const PasswordManagerPorter&) = delete;
  PasswordManagerPorter& operator=(const PasswordManagerPorter&) = delete;

  ~PasswordManagerPorter() override;

  // Triggers passwords export flow for the given |web_contents|.
  bool Export(content::WebContents* web_contents);

  void CancelExport();
  password_manager::ExportProgressStatus GetExportProgressStatus();

  // The next export will use |exporter|, instead of creating a new instance.
  void SetExporterForTesting(
      std::unique_ptr<password_manager::PasswordManagerExporter> exporter);

  // The next import will use |importer|, instead of creating a new instance.
  void SetImporterForTesting(
      std::unique_ptr<password_manager::PasswordImporter> importer);

  // Triggers passwords import flow for the given |web_contents|.
  void Import(content::WebContents* web_contents);

 private:
  enum Type {
    PASSWORD_IMPORT,
    PASSWORD_EXPORT,
  };

  // Display the file-picker dialogue for either importing or exporting
  // passwords.
  void PresentFileSelector(content::WebContents* web_contents, Type type);

  // Callback from the file selector dialogue when a file has been picked (for
  // either import or export).
  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void ImportPasswordsFromPath(const base::FilePath& path);

  void ExportPasswordsToPath(const base::FilePath& path);

  std::unique_ptr<password_manager::PasswordManagerExporter> exporter_;
  std::unique_ptr<password_manager::PasswordImporter> importer_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<Profile> profile_;

  // We store |presenter_| and
  // |on_export_progress_callback_| to use them to create a new
  // PasswordManagerExporter instance for each export.
  raw_ptr<password_manager::SavedPasswordsPresenter> presenter_;
  ProgressCallback on_export_progress_callback_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_
