// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_IMPORT_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_IMPORT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/settings/password_import_controller_interface.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ui/shell_dialogs/select_file_dialog.h"
// Handles the importing of a password file to the Password Manager.
class PasswordImportController : public PasswordImportControllerInterface,
                                 public ui::SelectFileDialog::Listener {
 public:
  // |presenter| provides the credentials which can be imported.
  explicit PasswordImportController(
      password_manager::SavedPasswordsPresenter* presenter);

  PasswordImportController(const PasswordImportController&) = delete;
  PasswordImportController& operator=(const PasswordImportController&) = delete;

  ~PasswordImportController() override;

  // PasswordImportControllerInterface:
  void Import(content::WebContents* web_contents,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback) override;
  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback) override;
  void ResetImporter(bool delete_file) override;

  // The next import will use |importer|, instead of creating a new instance.
  void SetImporterForTesting(
      std::unique_ptr<password_manager::PasswordImporter> importer);

 private:
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // Show the platform file selection dialog for importing.
  void PresentImportFileSelector(content::WebContents* web_contents);

  void ImportPasswordsFromPath(const base::FilePath& path);

  void ImportDone();

  std::unique_ptr<password_manager::PasswordImporter> importer_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  const raw_ptr<password_manager::SavedPasswordsPresenter> presenter_;

  // |import_results_callback_|, |to_store_| are stored in the controller
  // while the file is being selected.
  ImportResultsCallback import_results_callback_;
  password_manager::PasswordForm::Store to_store_;

  base::WeakPtrFactory<PasswordImportController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_IMPORT_CONTROLLER_H_
