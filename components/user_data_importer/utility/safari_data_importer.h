// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_

#include "base/files/scoped_temp_dir.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "components/user_data_importer/utility/zip_ffi_glue.rs.h"

namespace autofill {
class CreditCard;
class PaymentsDataManager;
}  // namespace autofill

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

class ReadingListModel;

namespace user_data_importer {

struct ImportedBookmarkEntry;

// Main model-layer object for extracting and importing user data from a bundle
// of data exported by Safari. The bundle is a ZIP file containing various data
// types in individual files, the format of which is documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc
// Users of this class must also provide an object implementing the
// `BookmarkParser` interface, which abstracts out certain logic which
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
                     autofill::PaymentsDataManager* payments_data_manager,
                     history::HistoryService* history_service,
                     bookmarks::BookmarkModel* bookmark_model,
                     ReadingListModel* reading_list_model,
                     std::unique_ptr<BookmarkParser> bookmark_parser,
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

  // Callback called one or multiple times from the rust parser when importing
  // history.
  void ParseHistoryCallback(
      std::vector<HistoryEntry> history_entries,
      ImportCallback history_callback = base::DoNothing());

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
  void ImportBookmarks(base::FilePath html, ImportCallback bookmarks_callback);

  // Receives the result of parsing bookmarks, stores them for later use,
  // and invokes `callback` with the number of parsed bookmarks.
  void OnBookmarksParsed(ImportCallback callback,
                         BookmarkParser::BookmarkParsingResult result);

  // Calls "history_callback" with an approximation of the number of URLs
  // contains in the history file contained in the zip file archive.
  void StartImportHistory(ImportCallback history_callback);

  // Attempts to import history from the zip file archive.
  // "ParseHistoryCallback" may be called multiple times during this process.
  void ImportHistory(ImportCallback history_callback);

  // Transforms the HistoryEntry objects into URLRow objects and uses the
  // history service to import them.
  void ImportHistoryEntries(std::vector<HistoryEntry> history_entries,
                            ImportCallback history_callback);

  // Attempts to import passwords by parsing the provided CSV data.
  // Calls "results_callback" when done.
  void ImportPasswords(std::string csv_data,
                       PasswordImportCallback passwords_callback);

  // Converts payment_cards to autofill::CreditCard objects.
  // Calls "payment_cards_callback" when done.
  void ImportPaymentCards(std::vector<PaymentCardEntry> payment_cards,
                          ImportCallback payment_cards_callback);

  // Imports Credit Cards to the Payments Data Manager.
  void ContinueImportPaymentCards(ImportCallback payment_cards_callback);

  // Writes the contents of `html_data` to a `bookmarks.html` file in a tmp
  // directory.
  std::optional<base::FilePath> WriteBookmarksToTmpFile(
      const std::string& html_data);

  // Launches the task which will call "ImportBookmarks".
  void LaunchImportBookmarksTask(ImportCallback bookmarks_callback);

  // Launches the task which will call "ImportPasswords".
  void LaunchImportPasswordsTask(PasswordImportCallback passwords_callback);

  // Launches the task which will call "ImportPaymentCards".
  void LaunchImportPaymentCardsTask(ImportCallback payment_cards_callback);

  // Posts a task on "task_runner_" to call the provided callback.
  void PostCallback(auto callback, auto results);

  // Launches task to close the zip file archive once it is no longer needed.
  void CloseZipFileArchive();

  // Closes the zip file archive from a worker thread.
  void CloseZipFileArchiveInWorkerThread();

  // The Rust zip file archive.
  std::optional<rust::Box<ZipFileArchive>> zip_file_archive_;

  // The password importer used to import passwords and resolve conflicts.
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;

  // The payments data manager.
  const raw_ref<autofill::PaymentsDataManager> payments_data_manager_;

  // Service used to import history URLs.
  const raw_ref<history::HistoryService> history_service_;

  // Service used to import bookmarks.
  const raw_ref<bookmarks::BookmarkModel> bookmark_model_;

  // Service used to import reading lists.
  const raw_ref<ReadingListModel> reading_list_model_;

  // The model-layer object used to parse bookmarks from an HTML file.
  std::unique_ptr<BookmarkParser> bookmark_parser_;

  // The task runner from which the import task was launched. The purpose of
  // this task runner is to post tasks on the thread where the importer lives,
  // which we have to do for "password_importer_" tasks and for all callbacks,
  // for example.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Stores the credit cards parsed from the "PaymentCards" JSON file.
  std::vector<autofill::CreditCard> cards_to_import_;

  // Stores bookmarks data on disk after unzip but before parsing.
  base::ScopedTempDir temp_dir_;

  // Bookmarks which have been parsed, but not yet committed to permanent
  // storage.
  std::vector<ImportedBookmarkEntry> pending_bookmarks_;

  // Reading List entries which have been parsed, but not yet committed to
  // permanent storage.
  std::vector<ImportedBookmarkEntry> pending_reading_list_;

  // The application locale, used to set credit card information.
  const std::string app_locale_;

  // Number of URLs imported during the history import process.
  size_t history_urls_imported_;

  // Maximum size the history entries vector can reach before calling
  // "ParseHistoryCallback". Can be modified for testing purposes.
  size_t history_size_threshold_ = 1000;

  // Creates WeakPtr to this. Use with caution across sequence boundaries.
  base::WeakPtrFactory<SafariDataImporter> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
