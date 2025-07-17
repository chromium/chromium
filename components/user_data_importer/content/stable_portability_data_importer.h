// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_

#include "components/user_data_importer/utility/bookmark_parser.h"

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

// Main model-layer object for extracting the data exported by browsers in the
// stable portability data format. The data is received through a system API in
// several files each containing a single data type.
class StablePortabilityDataImporter {
 public:
  // A callback used by the caller to obtain the number of successfully imported
  // bookmarks, reading list items, or urls (for history import).
  using ImportCallback = base::OnceCallback<void(int)>;

  StablePortabilityDataImporter(history::HistoryService& history_service,
                                bookmarks::BookmarkModel& bookmark_model,
                                ReadingListModel& reading_list_model);
  ~StablePortabilityDataImporter();

  // Attempts to import the `bookmarks_filename`. `bookmarks_callback` is called
  // at the end of the import process to notify the caller with the number of
  // successful items imported.
  void ImportBookmarks(const base::FilePath& bookmarks_filename,
                       ImportCallback bookmarks_callback);

  // Attempts to import the `reading_list_filename`. `reading_list_callback` is
  // called at the end of the import process to notify the caller with the
  // number of successful items imported.
  void ImportReadingList(const base::FilePath& reading_list_filename,
                         ImportCallback reading_list_callback);

  // Attempts to import the `history_filename`. `history_callback` is called at
  // the end of the import process to notify the caller with the number of
  // successful items imported.
  void ImportHistory(const base::FilePath& history_filename,
                     ImportCallback history_callback);

 private:
  friend class StablePortabilityDataImporterTest;

  // Receives the result of parsing bookmarks, stores them for later use, and
  // invokes `bookmarks_callback` with the number of parsed bookmarks.
  void OnBookmarksParsed(ImportCallback bookmarks_callback,
                         BookmarkParser::BookmarkParsingResult result);

  // Receives the result of parsing the reading list, stores them for later use,
  // and invokes `callback` with the number of parsed reading list items.
  void OnReadingListParsed(ImportCallback reading_list_callback,
                           BookmarkParser::BookmarkParsingResult result);

  // Posts a task on `origin_sequence_task_runner` to call the provided
  // callbacks in `StablePortabilityDataImporter::Import`
  void PostCallback(auto callback, auto results);

  // Service used to import history URLs.
  const raw_ref<history::HistoryService> history_service_;

  // Service used to import bookmarks.
  const raw_ref<bookmarks::BookmarkModel> bookmark_model_;

  // Service used to import reading list items.
  const raw_ref<ReadingListModel> reading_list_model_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Bookmarks which have been parsed, but not yet committed to permanent
  // storage.
  std::vector<ImportedBookmarkEntry> pending_bookmarks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Reading List items which have been parsed, but not yet committed to
  // permanent storage.
  std::vector<ImportedBookmarkEntry> pending_reading_list_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The task runner from which the import task was launched. The purpose of
  // this task runner is to post tasks on the thread where the importer lives,
  // which we have to do for all import callbacks.
  scoped_refptr<base::SequencedTaskRunner> origin_sequence_task_runner;

  // Creates WeakPtr to this. Use with caution across sequence boundaries.
  base::WeakPtrFactory<StablePortabilityDataImporter> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_STABLE_PORTABILITY_DATA_IMPORTER_H_
