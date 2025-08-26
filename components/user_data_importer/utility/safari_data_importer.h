// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_

#include "base/files/scoped_temp_dir.h"
#include "base/threading/sequence_bound.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/prefs/pref_service.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "components/user_data_importer/utility/importer_metrics_recorder.h"
#include "components/user_data_importer/utility/parsing_ffi/lib.rs.h"
#include "components/user_data_importer/utility/safari_data_import_client.h"

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

namespace syncer {
class SyncService;
}

class ReadingListModel;

namespace user_data_importer {

struct ImportedBookmarkEntry;

// See impl.
class RustHistoryCallback;

// Main model-layer object for extracting and importing user data from a bundle
// of data exported by Safari. The bundle is a ZIP file containing various data
// types in individual files, the format of which is documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc
// Users of this class must also provide an object implementing the
// `BookmarkParser` interface, which abstracts out certain logic which
// can't live in the components layer (because of platform dependencies).
class SafariDataImporter {
 public:
  using PasswordImportCallback =
      password_manager::PasswordImporter::ImportResultsCallback;
  using PasswordImportResults = password_manager::ImportResults;

  // All arguments passed as pointers are expected to outlive this importer.
  SafariDataImporter(SafariDataImportClient* client,
                     password_manager::SavedPasswordsPresenter* presenter,
                     autofill::PaymentsDataManager* payments_data_manager,
                     history::HistoryService* history_service,
                     bookmarks::BookmarkModel* bookmark_model,
                     ReadingListModel* reading_list_model,
                     syncer::SyncService* sync_service,
                     PrefService* pref_service,
                     std::unique_ptr<BookmarkParser> bookmark_parser,
                     std::string app_locale);
  ~SafariDataImporter();

  // Opens the ZIP file provided in `path` and prepares to import the data
  // (passwords, payment cards, bookmarks, and history) contained inside. Each
  // data type is optional and may or may not be present.
  void PrepareImport(const base::FilePath& path);

  // Logs the file size of the uncompressed ZIP file.
  void LogFileSize(int64_t file_size_bytes);

  // Called after calling `PrepareImport` in order to complete the import
  // process. In case of password conflicts, `selected_password_ids` provides
  // the list of conflicting passwords to import.
  void CompleteImport(const std::vector<int>& selected_password_ids);

  // Called after calling "Import" in order to cancel the import process.
  void CancelImport();

 private:
  friend class SafariDataImporterTest;

  // Encapsulates work which must occur in a blocking context (mainly I/O).
  // Intentionally does not provide access to any state or methods of
  // `SafariDataImporter`.
  class BlockingWorker {
   public:
    explicit BlockingWorker(std::unique_ptr<BookmarkParser> bookmark_parser);
    ~BlockingWorker();

    // Creates the zip file Rust archive from file provided by "zip_filename".
    // Returns whether the archive was created successfully; if false, the
    // import has failed and should terminate.
    bool CreateZipFileArchive(std::string zip_filename);

    // Closes the zip file archive when it is no longer needed. Calling this is
    // not mandatory; the archive will be cleaned up on the correct thread when
    // the BlockingWorker is destroyed, if it exists.
    void CloseZipFileArchive();

    // Returns the contents of the file of the desired type contained in the
    // zip file archive. Returns an empty string on failure.
    std::string Unzip(FileType filetype);

    // Returns the uncompressed size of a file within the zip file archive.
    size_t GetUncompressedFileSizeInBytes(FileType filetype);

    // Gets the size of the file at `path`. Returns nullopt on failure.
    std::optional<int64_t> GetInitialFileSize(const base::FilePath& path);

    struct BookmarkUnzipResult {
      std::optional<base::FilePath> path;
      size_t file_size_bytes;

      explicit BookmarkUnzipResult(std::optional<base::FilePath> p,
                                   size_t size);
      ~BookmarkUnzipResult();

      BookmarkUnzipResult(BookmarkUnzipResult&&);
      BookmarkUnzipResult& operator=(BookmarkUnzipResult&&);
      BookmarkUnzipResult(const BookmarkUnzipResult&) = delete;
      BookmarkUnzipResult& operator=(const BookmarkUnzipResult&) = delete;
    };

    // Unzips bookmarks in the ZIP archive and writes the contents to a
    // `bookmarks.html` file in a tmp directory. Returns nullopt if the file
    // could not be created.
    BookmarkUnzipResult WriteBookmarksToTmpFile();

    struct PaymentCardParseResult {
      std::vector<PaymentCardEntry> entries;
      size_t file_size_bytes;

      explicit PaymentCardParseResult(std::vector<PaymentCardEntry> e,
                                      size_t size);
      ~PaymentCardParseResult();

      PaymentCardParseResult(PaymentCardParseResult&&);
      PaymentCardParseResult& operator=(PaymentCardParseResult&&);
      PaymentCardParseResult(const PaymentCardParseResult&) = delete;
      PaymentCardParseResult& operator=(const PaymentCardParseResult&) = delete;
    };

    void ParseBookmarks(
        std::optional<base::FilePath> bookmarks_html,
        BookmarkParser::BookmarkParsingCallback bookmarks_callback);

    // Finds a file containing payment cards in the ZIP archive, parses it, and
    // returns the output. Returns empty on error.
    PaymentCardParseResult ParsePaymentCards();

    // Attempts to import history from the zip file archive.
    // `parse_history_callback` may be called multiple times during this
    // process, as batches of history entries become ready. `done_callback`
    // is called once, at the end.
    void ImportHistory(std::unique_ptr<RustHistoryCallback> callback,
                       size_t history_size_threshold);

    // The model-layer object used to parse bookmarks from an HTML file.
    std::unique_ptr<BookmarkParser> bookmark_parser_;

    // The Rust zip file archive.
    std::optional<rust::Box<ZipFileArchive>> zip_file_archive_;

    // Stores bookmarks data on disk after unzip but before parsing.
    std::unique_ptr<base::ScopedTempDir> bookmarks_temp_dir_;
  };

  // Once the ZIP archive has been created, checks its validity and triggers the
  // next step in the parsing pipeline.
  void OnZipArchiveReady(bool success);

  // Parses the provided `csv_data` and determines if any conflicts with
  // existing passwords will occur. Stops before actually committing any data.
  // Informs `client_` that passwords are ready when done.
  void PreparePasswords(std::string csv_data);

  // Converts payment_cards to autofill::CreditCard objects. Informs `client_`
  // that cards are ready when done.
  void PreparePaymentCards(
      BlockingWorker::PaymentCardParseResult payment_cards);

  // Attempts to parse the provided HTML data. Informs `client_` that bookmarks
  // are ready when done.
  void PrepareBookmarks(BlockingWorker::BookmarkUnzipResult result);

  // Receives the result of the first pass of the password importer, which
  // parses passwords and identifies any conflicts, then pauses. Informs the
  // client that passwords are ready.
  void OnPasswordsParsed(const password_manager::ImportResults& results);

  // Receives the result of parsing bookmarks, stores them for later use,
  // and informs the client that bookmarks are ready.
  void OnBookmarksParsed(BookmarkParser::BookmarkParsingResult result);

  // Logs an error and indicates to the client that no bookmarks are available.
  void OnBookmarkParsingError(BookmarkParser::BookmarkParsingError error);

  // Calls `history_callback` with an approximation of the number of URLs
  // contained in one or more files with total size `file_size_bytes`.
  void PrepareHistory(size_t file_size_bytes);

  // Transforms the SafariHistoryEntry objects into URLRow objects and uses the
  // history service to import them.
  void ImportHistoryEntries(std::vector<SafariHistoryEntry> history_entries);

  // Invoked if parsing of history fails. Forwards the results to `client_`.
  void OnHistoryImportFailed();

  // Invoked once parsing of history is completed. Forwards the results to
  // `client_`.
  void OnHistoryImportCompleted();

  // Invoked once parsing of passwords is completed. Forwards the results to
  // `client_`.
  void OnPasswordImportCompleted(
      const password_manager::ImportResults& results);

  // Imports Credit Cards to the Payments Data Manager.
  void ContinueImportPaymentCards();

  // Imports bookmarks and reading list entries from pending data into the
  // corresponding BookmarkModel and ReadingListModel.
  void ContinueImportBookmarks();

  // Invoked when all import processing tasks have concluded. Logs metrics.
  void OnImportComplete();

  // Objects used by this importer to do work (esp. parsing)

  // A queue for tasks which may block (e.g., I/O).
  scoped_refptr<base::SequencedTaskRunner> blocking_queue_;

  // An instance of BlockingWorker which is bound to `blocking_queue_`.
  base::SequenceBound<BlockingWorker> blocking_worker_;

  // The password importer used to import passwords and resolve conflicts.
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;

  // Interaction with UI layer

  // The client to inform when various processing events occur.
  const raw_ref<SafariDataImportClient> client_;

  // Model-layer objects used to actually save imported data

  // The payments data manager.
  const raw_ref<autofill::PaymentsDataManager> payments_data_manager_;

  // Service used to import history URLs.
  const raw_ref<history::HistoryService> history_service_;

  // Service used to import bookmarks.
  const raw_ref<bookmarks::BookmarkModel> bookmark_model_;

  // Service used to import reading lists.
  const raw_ref<ReadingListModel> reading_list_model_;

  // Stores pointer to `SyncService` instance.
  raw_ptr<syncer::SyncService> sync_service_;

  // The PrefService that this instance uses to read and write preferences.
  // Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Internal state

  // Stores the credit cards parsed from the "PaymentCards" JSON file.
  std::vector<autofill::CreditCard> cards_to_import_;

  // Bookmarks which have been parsed, but not yet committed to permanent
  // storage.
  std::vector<ImportedBookmarkEntry> pending_bookmarks_;

  // Reading List entries which have been parsed, but not yet committed to
  // permanent storage.
  std::vector<ImportedBookmarkEntry> pending_reading_list_;

  // Helper object which logs metrics about the import flow.
  ImporterMetricsRecorder metrics_recorder_;

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
