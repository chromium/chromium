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
  bool Export(base::WeakPtr<content::WebContents> web_contents) override;
  void CancelExport() override;
  password_manager::ExportProgressStatus GetExportProgressStatus() override;
  void Import(content::WebContents* web_contents,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback) override;
  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback) override;
  void ResetImporter(bool delete_file) override;

  // The next export will use |exporter|, instead of creating a new instance.
  void SetExporterForTesting(
      std::unique_ptr<password_manager::PasswordManagerExporter> exporter);

  // The next import will use |importer|, instead of creating a new instance.
  void SetImporterForTesting(
      std::unique_ptr<password_manager::PasswordImporter> importer);

 private:
  // These two helper classes are used to listen for results from the
  // import/export file pickers respectively. They delegate most of their
  // behavior back to the containing class (PasswordManagerPorter).
  class ImportFileSelectListener : public ui::SelectFileDialog::Listener {
   public:
    explicit ImportFileSelectListener(PasswordManagerPorter* owner);
    ~ImportFileSelectListener() override;

    // ui::SelectFileDialog::Listener:
    void FileSelected(const ui::SelectedFileInfo& file, int index) override;
    void FileSelectionCanceled() override;

   private:
    raw_ptr<PasswordManagerPorter> owner_;
  };

  class ExportFileSelectListener : public ui::SelectFileDialog::Listener {
   public:
    explicit ExportFileSelectListener(PasswordManagerPorter* owner);
    ~ExportFileSelectListener() override;

    // ui::SelectFileDialog::Listener:
    void FileSelected(const ui::SelectedFileInfo& file, int index) override;
    void FileSelectionCanceled() override;

   private:
    raw_ptr<PasswordManagerPorter> owner_;
  };

  // Show the platform file selection dialog as appropriate for either importing
  // or exporting. These deliver callbacks via ImportFileSelectListener
  // (import_listener_) and ExportFileSelectListener (export_listener_).
  void PresentImportFileSelector(content::WebContents* web_contents);
  void PresentExportFileSelector(content::WebContents* web_contents);

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

  ImportFileSelectListener import_listener_{this};
  ExportFileSelectListener export_listener_{this};

  base::WeakPtrFactory<PasswordManagerPorter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_H_
