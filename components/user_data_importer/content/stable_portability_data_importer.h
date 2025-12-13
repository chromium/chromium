// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "components/user_data_importer/utility/history_callback_from_rust.h"
#include "components/user_data_importer/utility/importer_metrics_recorder.h"

namespace base {
class File;
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

struct StablePortabilityHistoryEntry;

// Main model-layer object for extracting the data exported by browsers in the
// stable portability data format. The data is received through a system API in
// several files each containing a single data type.
class StablePortabilityDataImporter {
 public:
  // A callback used by the caller to obtain the number of successfully imported
  // bookmarks, reading list items, or history entries. In case of an error, the
  // reported count will be -1.
  using ImportCallback = base::OnceCallback<void(int)>;

  // Default batch size when importing history. Different values can be passed
  // to ImportHistory() for testing purposes.
  static constexpr size_t kHistoryBatchSize = 1000;

  // `history_service`, `bookmark_model`, and `reading_list_model` may be null,
  // but if non-null must outlive this class. `bookmark_parser` must not be
  // null.
  StablePortabilityDataImporter(
      history::HistoryService* history_service,
      bookmarks::BookmarkModel* bookmark_model,
      ReadingListModel* reading_list_model,
      std::unique_ptr<ContentBookmarkParser> bookmark_parser);
  ~StablePortabilityDataImporter();

  // Attempts to import bookmarks from the given `file`. `bookmarks_callback` is
  // called at the end of the import process to notify the caller about the
  // number of items successfully imported.
  void ImportBookmarks(base::File file, ImportCallback bookmarks_callback);

  // Attempts to import bookmarks from the given `file`. `reading_list_callback`
  // is called at the end of the import process to notify the caller about the
  // number of items successfully imported.
  void ImportReadingList(base::File file, ImportCallback reading_list_callback);

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // Attempts to import history from the given `file`. `history_callback` is
  // called at the end of the import process to notify the caller about the
  // number of items successfully imported.
  void ImportHistory(base::File file,
                     ImportCallback history_callback,
                     const size_t import_batch_size = kHistoryBatchSize);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

 private:
  // Object used to allow Rust History import pipeline to communicate results
  // back to this importer.
  class RustHistoryCallbackForStablePortabilityFormat final
      : public user_data_importer::HistoryCallbackFromRust<
            StablePortabilityHistoryEntry> {
   public:
    using TransferHistoryCallback = base::RepeatingCallback<void(
        std::vector<StablePortabilityHistoryEntry>)>;

    explicit RustHistoryCallbackForStablePortabilityFormat(
        TransferHistoryCallback transfer_history_callback,
        user_data_importer::StablePortabilityDataImporter::ImportCallback
            done_callback);

    ~RustHistoryCallbackForStablePortabilityFormat() override;

    // Called from Rust when a batch of history entries has been parsed.
    void ImportHistoryEntries(
        std::unique_ptr<std::vector<
            user_data_importer::StablePortabilityHistoryEntry>> history_entries,
        bool completed) override;

    // Calls `done_callback_` with 0 to signal that parsing has failed.
    void Fail() override;

   private:
    TransferHistoryCallback transfer_history_callback_;
    user_data_importer::StablePortabilityDataImporter::ImportCallback
        done_callback_;
    size_t parsed_history_entries_count_ = 0;
  };

  // Encapsulates work which must occur in the background thread.
  class BackgroundWorker {
   public:
    explicit BackgroundWorker(
        std::unique_ptr<ContentBookmarkParser> bookmark_parser);
    ~BackgroundWorker();

    void ParseBookmarks(
        base::File file,
        user_data_importer::BookmarkParser::BookmarkParsingCallback
            bookmarks_callback);

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    void ParseHistory(
        base::File file,
        std::unique_ptr<RustHistoryCallbackForStablePortabilityFormat> callback,
        size_t import_batch_size);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

   private:
    std::unique_ptr<ContentBookmarkParser> bookmark_parser_;
  };

  friend class StablePortabilityDataImporterTest;

  // Transfers the history entries to the importer. This is used by the Rust
  // History import pipeline to communicate results back to this importer.
  void TransferHistoryEntries(
      std::vector<StablePortabilityHistoryEntry> history_entries);

  // Logs metrics related to history importing and invokes `history_callback`
  // with the number of history entries imported. A negative
  // `parsed_history_entries_count` is interpreted as an error having occurred
  // during the importing.
  void OnHistoryImportCompleted(ImportCallback history_callback,
                                int parsed_history_entries_count);

  // Receives the result of parsing bookmarks, and invokes `bookmarks_callback`
  // with the number of parsed bookmarks.
  void OnBookmarksParsed(ImportCallback bookmarks_callback,
                         BookmarkParser::BookmarkParsingResult result);

  // Logs an error and invokes `bookmarks_callback` with -1, indicating an
  // error occurred.
  void OnBookmarksParsingError(ImportCallback bookmarks_callback,
                               BookmarkParser::BookmarkParsingError error);

  // Receives the result of parsing the reading list, and invokes
  // `reading_list_callback` with the number of parsed reading list items.
  void OnReadingListParsed(ImportCallback reading_list_callback,
                           BookmarkParser::BookmarkParsingResult result);

  // Logs an error and invokes `reading_list_callback` with -1, indicating an
  // error occurred.
  void OnReadingListParsingError(ImportCallback reading_list_callback,
                                 BookmarkParser::BookmarkParsingError error);

  // Posts a task on `origin_sequence_task_runner` to call the provided
  // callbacks in `StablePortabilityDataImporter::Import`
  void PostCallback(auto callback, auto results);

  // Number of URLs imported during the history import process.
  size_t imported_history_entries_count_ = 0;

  // Service used to import history URLs.
  const raw_ptr<history::HistoryService> history_service_;

  // Service used to import bookmarks.
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // Service used to import reading list items.
  const raw_ptr<ReadingListModel> reading_list_model_;

  // Helper object which logs metrics about the import flow.
  ImporterMetricsRecorder metrics_recorder_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The task runner from which the import task was launched. The purpose of
  // this task runner is to post tasks on the thread where the importer lives,
  // which we have to do for all import callbacks.
  scoped_refptr<base::SequencedTaskRunner> origin_sequence_task_runner_;

  // A queue for tasks which run on the background thread and may block.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // An instance of BackgroundWorker which is bound to
  // `background_task_runner_`.
  base::SequenceBound<BackgroundWorker> background_worker_;

  // Creates WeakPtr to this. Use with caution across sequence boundaries.
  base::WeakPtrFactory<StablePortabilityDataImporter> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_
