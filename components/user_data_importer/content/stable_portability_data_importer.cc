// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/stable_portability_data_importer.h"

#include "base/check_deref.h"
#include "base/files/file.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "components/user_data_importer/utility/history_callback_from_rust.h"
#include "components/user_data_importer/utility/parsing_ffi/lib.rs.h"
#include "content/public/browser/browser_thread.h"

namespace user_data_importer {

StablePortabilityDataImporter::RustHistoryCallbackForStablePortabilityFormat::
    RustHistoryCallbackForStablePortabilityFormat(
        TransferHistoryCallback transfer_history_callback,
        user_data_importer::StablePortabilityDataImporter::ImportCallback
            done_callback)
    : transfer_history_callback_(std::move(transfer_history_callback)),
      done_callback_(std::move(done_callback)) {}

StablePortabilityDataImporter::RustHistoryCallbackForStablePortabilityFormat::
    ~RustHistoryCallbackForStablePortabilityFormat() = default;

void StablePortabilityDataImporter::
    RustHistoryCallbackForStablePortabilityFormat::ImportHistoryEntries(
        std::unique_ptr<std::vector<StablePortabilityHistoryEntry>>
            history_entries,
        bool completed) {
  total_imported_count_ += history_entries->size();
  transfer_history_callback_.Run(std::move(*history_entries));

  if (completed && done_callback_) {
    std::move(done_callback_).Run(total_imported_count_);
  }
}

void StablePortabilityDataImporter::
    RustHistoryCallbackForStablePortabilityFormat::Fail() {
  if (done_callback_) {
    std::move(done_callback_).Run(0);
  }
}

StablePortabilityDataImporter::StablePortabilityDataImporter(
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model,
    scoped_refptr<ContentBookmarkParser> bookmark_parser)
    : history_service_(history_service),
      bookmark_model_(bookmark_model),
      reading_list_model_(reading_list_model),
      origin_sequence_task_runner_(
          base::SequencedTaskRunner::GetCurrentDefault()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      background_worker_(background_task_runner_, std::move(bookmark_parser)){
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

StablePortabilityDataImporter::~StablePortabilityDataImporter() = default;

void StablePortabilityDataImporter::ImportBookmarks(
    base::File file,
    ImportCallback bookmarks_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!file.IsValid()) {
    PostCallback(std::move(bookmarks_callback), 0);
    return;
  }

  // Safe to bind to WeakPtr to this callback, because it will run on the same
  // sequence, `origin_thread_task_runner`, where the importer was created.
  auto bookmarks_parser_callback =
      base::BindOnce(&StablePortabilityDataImporter::OnBookmarksParsed,
                     weak_factory_.GetWeakPtr(), std::move(bookmarks_callback));
  auto bookmarks_parser_callback_on_thread = base::BindPostTask(
      origin_sequence_task_runner_, std::move(bookmarks_parser_callback));

  // Post to the thread pool the task for parsing the file. Adding the actual
  // data to the user's storage should still be done on the origin sequence by
  // `OnBookmarksParsed`, as that's where the `bookmark_model_` lives.
  background_worker_.AsyncCall(&BackgroundWorker::ParseBookmarks)
      .WithArgs(std::move(file),
                std::move(bookmarks_parser_callback_on_thread));
}

void StablePortabilityDataImporter::OnBookmarksParsed(
    ImportCallback bookmarks_callback,
    BookmarkParser::BookmarkParsingResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   [this, &bookmarks_callback](auto) {
                     // TODO(crbug.com/414604427): Log error to UMA.
                     PostCallback(std::move(bookmarks_callback), 0);
                   });

  // TODO(crbug.com/414604427): Add the parsed bookmarks to the user's storage.
  pending_bookmarks_ = std::move(value.bookmarks);

  PostCallback(std::move(bookmarks_callback), pending_bookmarks_.size());
}

void StablePortabilityDataImporter::ImportReadingList(
    base::File file,
    ImportCallback reading_list_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!file.IsValid()) {
    PostCallback(std::move(reading_list_callback), 0);
    return;
  }

  // Safe to bind to WeakPtr to this callback, because it will run on the same
  // sequence, `origin_thread_task_runner`, where the importer was created.
  auto reading_list_parser_callback = base::BindOnce(
      &StablePortabilityDataImporter::OnReadingListParsed,
      weak_factory_.GetWeakPtr(), std::move(reading_list_callback));
  auto reading_list_parser_callback_on_thread = base::BindPostTask(
      origin_sequence_task_runner_, std::move(reading_list_parser_callback));

  // Post to the thread pool the task for parsing the file. Adding the actual
  // data to the user's storage should still be done on the origin sequence by
  // `OnBookmarksParsed`, as that's where the `reading_list_model_` lives.
  background_worker_.AsyncCall(&BackgroundWorker::ParseBookmarks)
      .WithArgs(std::move(file),
                std::move(reading_list_parser_callback_on_thread));
}

void StablePortabilityDataImporter::OnReadingListParsed(
    ImportCallback reading_list_callback,
    BookmarkParser::BookmarkParsingResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   [this, &reading_list_callback](auto) {
                     // TODO(crbug.com/414604427): Log error to UMA.
                     PostCallback(std::move(reading_list_callback), 0);
                   });

  // TODO(crbug.com/414604427): Add the parsed reading list to the user's
  // storage.
  pending_reading_list_ = std::move(value.bookmarks);

  PostCallback(std::move(reading_list_callback), pending_reading_list_.size());
}

void StablePortabilityDataImporter::TransferHistoryEntries(
    std::vector<StablePortabilityHistoryEntry> history_entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/431204966): Add the history entries to the user's storage.
  pending_history_entries_.insert(
      pending_history_entries_.end(),
      std::make_move_iterator(history_entries.begin()),
      std::make_move_iterator(history_entries.end()));
}

void StablePortabilityDataImporter::ImportHistory(
    const base::FilePath& history_filename,
    ImportCallback history_callback,
    const size_t import_batch_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto transfer_history_entries_callback = base::BindPostTask(
      origin_sequence_task_runner_,
      base::BindRepeating(
          &StablePortabilityDataImporter::TransferHistoryEntries,
          weak_factory_.GetWeakPtr()));
  auto callback =
      std::make_unique<RustHistoryCallbackForStablePortabilityFormat>(
          std::move(transfer_history_entries_callback),
          base::BindPostTask(origin_sequence_task_runner_,
                             std::move(history_callback)));
  if (history_filename.empty()) {
    callback->Fail();
    return;
  }

  // Convert the base::FilePath to a UTF-8 string and then to a Rust slice.
  std::string history_filename_utf8 = history_filename.AsUTF8Unsafe();

  background_worker_.AsyncCall(&BackgroundWorker::ParseHistory)
      .WithArgs(std::move(history_filename_utf8), std::move(callback),
                import_batch_size);
}

void StablePortabilityDataImporter::PostCallback(auto callback, auto results) {
  origin_sequence_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

StablePortabilityDataImporter::BackgroundWorker::BackgroundWorker(
    scoped_refptr<ContentBookmarkParser> bookmark_parser)
    : bookmark_parser_(std::move(bookmark_parser)) {}

StablePortabilityDataImporter::BackgroundWorker::~BackgroundWorker() = default;

void StablePortabilityDataImporter::BackgroundWorker::ParseBookmarks(
    base::File file,
    BookmarkParser::BookmarkParsingCallback bookmarks_callback) {
  bookmark_parser_->Parse(std::move(file), std::move(bookmarks_callback));
}

void StablePortabilityDataImporter::BackgroundWorker::ParseHistory(
    const std::string& history_filename,
    std::unique_ptr<RustHistoryCallbackForStablePortabilityFormat> callback,
    size_t import_batch_size) {
  rust::Slice<const uint8_t> history_filename_slice(
      reinterpret_cast<const uint8_t*>(history_filename.data()),
      history_filename.length());
  user_data_importer::parse_stable_portability_history(
      history_filename_slice, std::move(callback), import_batch_size);
}

}  // namespace user_data_importer
