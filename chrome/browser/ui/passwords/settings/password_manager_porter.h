// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/settings/password_manager_porter_interface.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace password_manager {
class PasswordManagerExporter;
struct PasswordExportInfo;
}  // namespace password_manager

class Profile;

// Handles the exporting of passwords to a file, and the importing of such a
// file to the Password Manager.
class PasswordManagerPorter : public PasswordManagerPorterInterface,
                              public ui::SelectFileDialog::Listener {
 public:
  using ExportProgressCallback = base::RepeatingCallback<void(
      const password_manager::PasswordExportInfo&)>;

  // |profile| for which credentials to be importerd.
  // |presenter| provides the credentials which can be exported.
  // |on_export_progress_callback| will be called with updates to the progress
  // of exporting.
  PasswordManagerPorter(Profile* profile,
                        password_manager::SavedPasswordsPresenter* presenter,
                        ExportProgressCallback on_export_progress_callback);

  PasswordManagerPorter(const PasswordManagerPorter&) = delete;
  PasswordManagerPorter& operator=(const PasswordManagerPorter&) = delete;

  ~PasswordManagerPorter() override;

  // PasswordManagerPorterInterface:
  bool Export(content::WebContents* web_contents) override;
  void CancelExport() override;
  password_manager::ExportProgressStatus GetExportProgressStatus() override;
  void Import(content::WebContents* web_contents,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback) override;

  // The next export will use |exporter|, instead of creating a new instance.
  void SetExporterForTesting(
      std::unique_ptr<password_manager::PasswordManagerExporter> exporter);

  // The next import will use |importer|, instead of creating a new instance.
  void SetImporterForTesting(
      std::unique_ptr<password_manager::PasswordImporter> importer);

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

  void ImportDone();

  void ExportDone();

  std::unique_ptr<password_manager::PasswordManagerExporter> exporter_;
  std::unique_ptr<password_manager::PasswordImporter> importer_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<Profile> profile_;

  // We store |presenter_| and
  // |on_export_progress_callback_| to use them to create a new
  // PasswordManagerExporter instance for each export.
  const raw_ptr<password_manager::SavedPasswordsPresenter> presenter_;
  ExportProgressCallback on_export_progress_callback_;

  // |import_results_callback_|, |to_store_| are stored in the porter
  // while the file is being selected.
  ImportResultsCallback import_results_callback_;
  password_manager::PasswordForm::Store to_store_;

  base::WeakPtrFactory<PasswordManagerPorter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_
