// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/stable_portability_data_importer.h"

#include "base/check_deref.h"
#include "base/files/file.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_data_importer/utility/bookmark_util.h"
#include "components/user_data_importer/utility/history_callback_from_rust.h"
#include "components/user_data_importer/utility/parsing_ffi/lib.rs.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_data_importer {

namespace {

std::string_view RustStringToStringView(const rust::String& rust_string) {
  return std::string_view(rust_string.data(), rust_string.length());
}

std::u16string RustStringToUTF16(const rust::String& rust_string) {
  return base::UTF8ToUTF16(RustStringToStringView(rust_string));
}

std::optional<history::URLRow> ConvertToURLRow(
    const user_data_importer::StablePortabilityHistoryEntry& history_entry) {
  GURL gurl(RustStringToStringView(history_entry.url));
  if (!gurl.is_valid()) {
    return std::nullopt;
  }

  history::URLRow url_row(gurl);
  url_row.set_title(RustStringToUTF16(history_entry.title));
  url_row.set_visit_count(history_entry.visit_count);

  url_row.set_last_visit(
      base::Time::UnixEpoch() +
      base::Microseconds(history_entry.visit_time_unix_epoch_usec));
  url_row.set_typed_count(history_entry.typed_count);

  return url_row;
}

}  // namespace

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
  parsed_history_entries_count_ += history_entries->size();
  transfer_history_callback_.Run(std::move(*history_entries));

  if (completed && done_callback_) {
    std::move(done_callback_).Run(parsed_history_entries_count_);
  }
}

void StablePortabilityDataImporter::
    RustHistoryCallbackForStablePortabilityFormat::Fail() {
  if (done_callback_) {
    std::move(done_callback_).Run(-1);
  }
}

StablePortabilityDataImporter::BackgroundWorker::BackgroundWorker(
    std::unique_ptr<ContentBookmarkParser> bookmark_parser)
    : bookmark_parser_(std::move(bookmark_parser)) {}

StablePortabilityDataImporter::BackgroundWorker::~BackgroundWorker() = default;

void StablePortabilityDataImporter::BackgroundWorker::ParseBookmarks(
    base::File file,
    BookmarkParser::BookmarkParsingCallback bookmarks_callback) {
  bookmark_parser_->Parse(std::move(file), std::move(bookmarks_callback));
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
void StablePortabilityDataImporter::BackgroundWorker::ParseHistory(
    base::File file,
    std::unique_ptr<RustHistoryCallbackForStablePortabilityFormat> callback,
    size_t import_batch_size) {
  int owned_raw_fd = file.TakePlatformFile();
  user_data_importer::parse_stable_portability_history(
      owned_raw_fd, std::move(callback), import_batch_size);
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

StablePortabilityDataImporter::StablePortabilityDataImporter(
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model,
    std::unique_ptr<ContentBookmarkParser> bookmark_parser)
    : history_service_(history_service),
      bookmark_model_(bookmark_model),
      reading_list_model_(reading_list_model),
      metrics_recorder_(
          ImporterMetricsRecorder::Source::kStablePortabilityData),
      origin_sequence_task_runner_(
          base::SequencedTaskRunner::GetCurrentDefault()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      background_worker_(background_task_runner_, std::move(bookmark_parser)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

StablePortabilityDataImporter::~StablePortabilityDataImporter() = default;

void StablePortabilityDataImporter::ImportBookmarks(
    base::File file,
    ImportCallback bookmarks_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  metrics_recorder_.bookmark_metrics().OnPreparationStarted();

  if (!file.IsValid()) {
    metrics_recorder_.bookmark_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kNotPresent);
    PostCallback(std::move(bookmarks_callback), -1);
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

void StablePortabilityDataImporter::ImportReadingList(
    base::File file,
    ImportCallback reading_list_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  metrics_recorder_.reading_list_metrics().OnPreparationStarted();

  if (!file.IsValid()) {
    metrics_recorder_.reading_list_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kNotPresent);
    PostCallback(std::move(reading_list_callback), -1);
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

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
void StablePortabilityDataImporter::ImportHistory(
    base::File file,
    ImportCallback history_callback,
    const size_t import_batch_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  metrics_recorder_.history_metrics().OnImportStarted();

  if (!file.IsValid()) {
    metrics_recorder_.history_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kNotPresent);
    PostCallback(std::move(history_callback), -1);
    return;
  }

  if (!history_service_) {
    metrics_recorder_.history_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kFailure);
    PostCallback(std::move(history_callback), -1);
    return;
  }

  auto transfer_history_entries_callback = base::BindPostTask(
      origin_sequence_task_runner_,
      base::BindRepeating(
          &StablePortabilityDataImporter::TransferHistoryEntries,
          weak_factory_.GetWeakPtr()));

  auto done_callback =
      base::BindOnce(&StablePortabilityDataImporter::OnHistoryImportCompleted,
                     weak_factory_.GetWeakPtr(), std::move(history_callback));
  auto done_callback_on_thread = base::BindPostTask(
      origin_sequence_task_runner_, std::move(done_callback));

  auto rust_history_callback =
      std::make_unique<RustHistoryCallbackForStablePortabilityFormat>(
          std::move(transfer_history_entries_callback),
          std::move(done_callback_on_thread));

  background_worker_.AsyncCall(&BackgroundWorker::ParseHistory)
      .WithArgs(std::move(file), std::move(rust_history_callback),
                import_batch_size);
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

void StablePortabilityDataImporter::TransferHistoryEntries(
    std::vector<StablePortabilityHistoryEntry> history_entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(history_service_);

  history::URLRows url_rows;
  url_rows.reserve(history_entries.size());
  for (const auto& history_entry : history_entries) {
    std::optional<history::URLRow> opt_row = ConvertToURLRow(history_entry);
    if (opt_row) {
      url_rows.push_back(std::move(opt_row.value()));
    }
  }

  if (!url_rows.empty()) {
    history_service_->AddPagesWithDetails(
        url_rows, history::SOURCE_OS_MIGRATION_IMPORTED);
    imported_history_entries_count_ += url_rows.size();
  }
}

void StablePortabilityDataImporter::OnHistoryImportCompleted(
    ImportCallback history_callback,
    int parsed_history_entries_count) {
  if (parsed_history_entries_count < 0) {
    metrics_recorder_.history_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kFailure);
    PostCallback(std::move(history_callback), parsed_history_entries_count);
    return;
  }

  metrics_recorder_.history_metrics().OnImportFinished(
      imported_history_entries_count_);
  metrics_recorder_.history_metrics().LogOutcome(
      DataTypeMetrics::ImportOutcome::kSuccess);
  PostCallback(std::move(history_callback), imported_history_entries_count_);
}

void StablePortabilityDataImporter::OnBookmarksParsed(
    ImportCallback bookmarks_callback,
    BookmarkParser::BookmarkParsingResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!bookmark_model_) {
    metrics_recorder_.bookmark_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kFailure);
    PostCallback(std::move(bookmarks_callback), -1);
    return;
  }

  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   &StablePortabilityDataImporter::OnBookmarksParsingError,
                   this, std::move(bookmarks_callback));

  metrics_recorder_.bookmark_metrics().OnPreparationFinished(
      value.bookmarks.size());
  metrics_recorder_.bookmark_metrics().OnImportStarted();

  // Add the parsed bookmarks to the user's storage.
  size_t imported_count = ::user_data_importer::ImportBookmarks(
      bookmark_model_, std::move(value.bookmarks),
      l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER));

  PostCallback(std::move(bookmarks_callback), imported_count);
  metrics_recorder_.bookmark_metrics().OnImportFinished(imported_count);
  metrics_recorder_.bookmark_metrics().LogOutcome(
      DataTypeMetrics::ImportOutcome::kSuccess);
}

void StablePortabilityDataImporter::OnBookmarksParsingError(
    ImportCallback bookmarks_callback,
    BookmarkParser::BookmarkParsingError error) {
  metrics_recorder_.LogBookmarksError(ConvertBookmarkError(error));
  metrics_recorder_.bookmark_metrics().LogOutcome(
      DataTypeMetrics::ImportOutcome::kFailure);
  PostCallback(std::move(bookmarks_callback), -1);
}

void StablePortabilityDataImporter::OnReadingListParsed(
    ImportCallback reading_list_callback,
    BookmarkParser::BookmarkParsingResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reading_list_model_) {
    metrics_recorder_.reading_list_metrics().LogOutcome(
        DataTypeMetrics::ImportOutcome::kFailure);
    PostCallback(std::move(reading_list_callback), -1);
    return;
  }

  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   &StablePortabilityDataImporter::OnReadingListParsingError,
                   this, std::move(reading_list_callback));

  metrics_recorder_.reading_list_metrics().OnPreparationFinished(
      value.bookmarks.size());
  metrics_recorder_.reading_list_metrics().OnImportStarted();

  // Add the parsed reading list entries to the user's storage.
  size_t imported_count = ::user_data_importer::ImportReadingList(
      reading_list_model_, std::move(value.bookmarks));

  metrics_recorder_.reading_list_metrics().OnImportFinished(imported_count);
  metrics_recorder_.reading_list_metrics().LogOutcome(
      DataTypeMetrics::ImportOutcome::kSuccess);
  PostCallback(std::move(reading_list_callback), imported_count);
}

void StablePortabilityDataImporter::OnReadingListParsingError(
    ImportCallback reading_list_callback,
    BookmarkParser::BookmarkParsingError error) {
  metrics_recorder_.LogReadingListError(ConvertBookmarkError(error));
  metrics_recorder_.reading_list_metrics().LogOutcome(
      DataTypeMetrics::ImportOutcome::kFailure);
  PostCallback(std::move(reading_list_callback), -1);
}

void StablePortabilityDataImporter::PostCallback(auto callback, auto results) {
  origin_sequence_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

}  // namespace user_data_importer
