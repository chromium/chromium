// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/import/password_importer.h"

namespace user_data_importer {

class SafariDataImporter {
 public:
  // A callback used to obtain the number of successfully imported bookmarks,
  // urls (for history import) or payment cards.
  using ImportCallback = base::OnceCallback<void(int)>;

  using PasswordImportCallback =
      password_manager::PasswordImporter::ImportResultsCallback;
  using PasswordImportResults = password_manager::ImportResults;

  SafariDataImporter(password_manager::SavedPasswordsPresenter* presenter);
  ~SafariDataImporter();

  // Attempts to import various data types (passwords, payment cards, bookmarks
  // and history) from the file provided in "path". Each data type is optional
  // may or may not be present in the file. "passwords_callback" is called at
  // the end of the password import process and will be provided a list of
  // successful imports as well as conflicts and errors.
  // "bookmarks_callback", "history_callback" and "payment_cards_callback" will
  // be called at the end of the import processes of each type of data to return
  // the number of successful imports.
  void Import(const base::FilePath& path,
              PasswordImportCallback passwords_callback,
              ImportCallback bookmarks_callback,
              ImportCallback history_callback,
              ImportCallback payment_cards_callback);

  // Called after calling "Import" in order to import passwords. In case of
  // conflicts, "selected_ids" provides the list of conflicting passwords to
  // import.
  void ContinuePasswordImport(const std::vector<int>& selected_ids,
                              PasswordImportCallback passwords_callback);

 private:
  friend class SafariDataImporterTest;

  // Gets a weak pointer to this object.
  base::WeakPtr<SafariDataImporter> AsWeakPtr();

  // This function imports the various data types present in the file provided
  // by "zip_filename" and should be called from a worker thread.
  void ImportInWorkerThread(std::string zip_filename,
                            PasswordImportCallback passwords_callback,
                            ImportCallback bookmarks_callback,
                            ImportCallback history_callback,
                            ImportCallback payment_cards_callback);

  // Attempts to import bookmarks by parsing the provided HTML data.
  // Calls "bookmarks_callback" when done.
  void ImportBookmarks(std::string html_data,
                       ImportCallback bookmarks_callback);

  // Attempts to import history from the file provided by "zip_filename".
  // Calls "history_callback" when done.
  void ImportHistory(const std::string& zip_filename,
                     ImportCallback history_callback);

  // Attempts to import passwords by parsing the provided CSV data.
  // Calls "results_callback" when done.
  void ImportPasswords(std::string csv_data,
                       PasswordImportCallback passwords_callback);

  // Attempts to import payment cards by parsing the provided JSON data.
  // Calls "payment_cards_callback" when done.
  void ImportPaymentCards(std::string json_data,
                          ImportCallback payment_cards_callback);

  // Launches the task which will call "ImportBookmarks".
  void LaunchImportBookmarksTask(const std::string& zip_filename,
                                 ImportCallback bookmarks_callback);

  // Launches the task which will call "ImportPasswords".
  void LaunchImportPasswordsTask(const std::string& zip_filename,
                                 PasswordImportCallback passwords_callback);

  // Launches the task which will call "ImportPaymentCards".
  void LaunchImportPaymentCardsTask(const std::string& zip_filename,
                                    ImportCallback payment_cards_callback);

  // Posts a task on "task_runner_" to call the provided callback.
  void PostCallback(auto callback, auto results);

  // The password importer used to import passwords and resolve conflicts.
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;

  // The task runner from which the import task was launched. The purpose of
  // this task runner is to post tasks on the thread where the importer lives,
  // which we have to do for "password_importer_" tasks and for all callbacks,
  // for example.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This is necessary because this object could be deleted during any callback,
  // and we don't want to risk a UAF if that happens.
  base::WeakPtrFactory<SafariDataImporter> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
