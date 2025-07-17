// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/stable_portability_data_importer.h"

#include "base/check_deref.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "content/public/browser/browser_thread.h"

namespace {
// Parses the provided `bookmarks_html` file.
void ParseBookmarks(const base::FilePath& bookmarks_html,
                    user_data_importer::BookmarkParser::BookmarkParsingCallback
                        bookmarks_callback) {
  CHECK(!bookmarks_html.empty());

  auto bookmark_parser = user_data_importer::MakeBookmarkParser();
  bookmark_parser->Parse(bookmarks_html, std::move(bookmarks_callback));
}

}  // namespace

namespace user_data_importer {

StablePortabilityDataImporter::StablePortabilityDataImporter(
    history::HistoryService& history_service,
    bookmarks::BookmarkModel& bookmark_model,
    ReadingListModel& reading_list_model)
    : history_service_(history_service),
      bookmark_model_(bookmark_model),
      reading_list_model_(reading_list_model),
      origin_sequence_task_runner(
          base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

StablePortabilityDataImporter::~StablePortabilityDataImporter() = default;

void StablePortabilityDataImporter::ImportBookmarks(
    const base::FilePath& bookmarks_filename,
    ImportCallback bookmarks_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (bookmarks_filename.empty()) {
    PostCallback(std::move(bookmarks_callback), 0);
    return;
  }

  // Safe to bind to WeakPtr to this callback, because it will run on the same
  // sequence, `origin_thread_task_runner`, where the importer was created.
  auto bookmarks_parser_callback =
      base::BindOnce(&StablePortabilityDataImporter::OnBookmarksParsed,
                     weak_factory_.GetWeakPtr(), std::move(bookmarks_callback));
  auto bookmarks_parser_callback_on_thread = base::BindPostTask(
      origin_sequence_task_runner, std::move(bookmarks_parser_callback));

  // Post to the thread pool the task for parsing the file. Adding the actual
  // data to the user's storage should still be done on the origin sequence by
  // `OnBookmarksParsed`, as that's where the `bookmark_model_` lives.
  // TODO(crnug.com/432010608): Sandbox parsing the bookmarks file.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ParseBookmarks, std::move(bookmarks_filename),
                     std::move(bookmarks_parser_callback_on_thread)));
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
    const base::FilePath& reading_list_filename,
    ImportCallback reading_list_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (reading_list_filename.empty()) {
    PostCallback(std::move(reading_list_callback), 0);
    return;
  }

  // Safe to bind to WeakPtr to this callback, because it will run on the same
  // sequence, `origin_thread_task_runner`, where the importer was created.
  auto reading_list_parser_callback = base::BindOnce(
      &StablePortabilityDataImporter::OnReadingListParsed,
      weak_factory_.GetWeakPtr(), std::move(reading_list_callback));
  auto reading_list_parser_callback_on_thread = base::BindPostTask(
      origin_sequence_task_runner, std::move(reading_list_parser_callback));

  // Post to the thread pool the task for parsing the file. Adding the actual
  // data to the user's storage should still be done on the origin sequence by
  // `OnBookmarksParsed`, as that's where the `reading_list_model_` lives.
  // TODO(crnug.com/432010608): Sandbox parsing the reading list file.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ParseBookmarks, std::move(reading_list_filename),
                     std::move(reading_list_parser_callback_on_thread)));
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

void StablePortabilityDataImporter::ImportHistory(
    const base::FilePath& history_filename,
    ImportCallback history_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (history_filename.empty()) {
    PostCallback(std::move(history_callback), 0);
    return;
  }

  // TODO(crbug.com/414604427): Implement parsing of history from the stable
  // portability data format.
  PostCallback(std::move(history_callback), 0);
}

void StablePortabilityDataImporter::PostCallback(auto callback, auto results) {
  origin_sequence_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

}  // namespace user_data_importer
