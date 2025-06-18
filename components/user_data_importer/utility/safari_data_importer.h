// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/user_data_importer/utility/zip_ffi_glue.rs.h"

namespace user_data_importer {

class SafariDataImportManager;

// Main model-layer object for extracting and importing user data from a bundle
// of data exported by Safari. The bundle is a ZIP file containing various data
// types in individual files, the format of which is documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc
// Users of this class must also provide an object implementing the
// `SafariDataImportManager` interface, which abstracts out certain logic which
// can't live in the components layer (because of platform dependencies).
class SafariDataImporter {
 public:
  // A callback used to obtain the number of successfully imported bookmarks,
  // urls (for history import) or payment cards.
  using ImportCallback = base::OnceCallback<void(int)>;

  using PasswordImportCallback =
      password_manager::PasswordImporter::ImportResultsCallback;
  using PasswordImportResults = password_manager::ImportResults;

  SafariDataImporter(password_manager::SavedPasswordsPresenter* presenter,
                     std::unique_ptr<SafariDataImportManager> manager,
                     std::string app_locale);
  ~SafariDataImporter();

  // Attempts to import various data types (passwords, payment cards, bookmarks
  // and history) from the file provided in "path". Each data type is optional
  // may or may not be present in the file. "passwords_callback" is called at
  // the end of the password import process and will be provided a list of
  // successful imports as well as conflicts and errors.
  // "bookmarks_callback", "history_callback" and "payment_cards_callback" will
  // be called at the end of the import processes of each type of data to return
  // the number of successful imports.
  void StartImport(const base::FilePath& path,
                   PasswordImportCallback passwords_callback,
                   ImportCallback bookmarks_callback,
                   ImportCallback history_callback,
                   ImportCallback payment_cards_callback);

  // Called after calling "Import" in order to complete the import process. In
  // case of password conflicts, "selected_password_ids" provides the list of
  // conflicting passwords to import.
  void ContinueImport(const std::vector<int>& selected_password_ids,
                      PasswordImportCallback passwords_callback,
                      ImportCallback bookmarks_callback,
                      ImportCallback history_callback,
                      ImportCallback payment_cards_callback);

  // Called after calling "Import" in order to cancel the import process.
  void CancelImport();

 private:
  friend class SafariDataImporterTest;

  // Creates the zip file Rust archive from file provided by "zip_filename".
  // Returns whether `zip_file_archive_` was created successfully.
  bool CreateZipFileArchive(std::string zip_filename);

  // Returns the contents of the file of the desired type contained in the
  // zip file archive. Returns an empty string on failure.
  std::string Unzip(FileType filetype);

  // Returns the uncompressed size of a file within the zip file archive.
  size_t UncompressedFileSize(FileType filetype);

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

  // Calls "history_callback" with an approximation of the number of URLs
  // contains in the history file contained in the zip file archive.
  void StartImportHistory(ImportCallback history_callback);

  // Attempts to import history from the zip file archive.
  // Calls "history_callback" when done.
  void ImportHistory(ImportCallback history_callback);

  // Attempts to import passwords by parsing the provided CSV data.
  // Calls "results_callback" when done.
  void ImportPasswords(std::string csv_data,
                       PasswordImportCallback passwords_callback);

  // Converts payment_cards to autofill::CreditCard objects.
  // Calls "payment_cards_callback" when done.
  void ImportPaymentCards(std::vector<PaymentCardEntry> payment_cards,
                          ImportCallback payment_cards_callback);

  // Launches the task which will call "ImportBookmarks".
  void LaunchImportBookmarksTask(ImportCallback bookmarks_callback);

  // Launches the task which will call "ImportPasswords".
  void LaunchImportPasswordsTask(PasswordImportCallback passwords_callback);

  // Launches the task which will call "ImportPaymentCards".
  void LaunchImportPaymentCardsTask(ImportCallback payment_cards_callback);

  // Posts a task on "task_runner_" to call the provided callback.
  void PostCallback(auto callback, auto results);

  // Closes the zip file archive once it is no longer needed.
  void CloseZipFileArchive();

  // The Rust zip file archive.
  std::optional<rust::Box<ZipFileArchive>> zip_file_archive_;

  // The password importer used to import passwords and resolve conflicts.
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;

  // The task runner from which the import task was launched. The purpose of
  // this task runner is to post tasks on the thread where the importer lives,
  // which we have to do for "password_importer_" tasks and for all callbacks,
  // for example.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Encapsulates model-layer logic that has to be injected (e.g.,
  // platform-specific logic).
  std::unique_ptr<SafariDataImportManager> manager_;

  // Stores the credit cards parsed from the "PaymentCards" JSON file.
  std::vector<autofill::CreditCard> cards_to_import_;

  // The application locale, used to set credit card information.
  const std::string app_locale_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
