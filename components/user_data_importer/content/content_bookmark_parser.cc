// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"

namespace user_data_importer {

// Declared in bookmark_parser.h.
std::unique_ptr<BookmarkParser> MakeBookmarkParser() {
  return std::make_unique<ContentBookmarkParser>();
}

ContentBookmarkParser::ContentBookmarkParser() {
  // This class is created on the UI thread, but is used forever after from the
  // background thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ContentBookmarkParser::~ContentBookmarkParser() = default;

void ContentBookmarkParser::SetServiceForTesting(
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser) {
  CHECK(!html_parser_remote_);
  html_parser_for_testing_ = std::move(parser);
}

void ContentBookmarkParser::Parse(
    const base::FilePath& file_path,
    BookmarkParser::BookmarkParsingCallback callback) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string raw_html;
  // ReadFileToString can return false, but still populate something into
  // `raw_html`. In that case, try to recover as much data as possible.
  // (ParseImpl() will report an error if `raw_html` is empty, i.e. the
  // read failed entirely.)
  base::ReadFileToString(file_path, &raw_html);

  ParseImpl(std::move(raw_html), std::move(callback));
}

void ContentBookmarkParser::Parse(base::File file,
                                  BookmarkParsingCallback callback) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string raw_html;
  // ReadStreamToString can return false, but still populate something into
  // `raw_html`. In that case, try to recover as much data as possible.
  // (ParseImpl() will report an error if `raw_html` is empty, i.e. the
  // read failed entirely.)
  base::ReadStreamToString(base::FileToFILE(std::move(file), "rb"), &raw_html);

  ParseImpl(std::move(raw_html), std::move(callback));
}

void ContentBookmarkParser::ParseImpl(
    std::string raw_html,
    BookmarkParser::BookmarkParsingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (raw_html.empty()) {
    std::move(callback).Run(base::unexpected(
        BookmarkParser::BookmarkParsingError::kFailedToReadFile));
    return;
  }

  if (!html_parser_remote_) {
    if (html_parser_for_testing_) {
      html_parser_remote_.Bind(std::move(html_parser_for_testing_));
    } else {
      html_parser_remote_ = content::ServiceProcessHost::Launch<
          user_data_importer::mojom::BookmarkHtmlParser>(
          content::ServiceProcessHost::Options()
              .WithDisplayName(IDS_CONTENT_BOOKMARK_PARSER_SERVICE_DISPLAY_NAME)
              .Pass());
    }
    html_parser_remote_.reset_on_disconnect();
  }

  html_parser_remote_->Parse(
      std::move(raw_html),
      base::BindOnce(&ContentBookmarkParser::OnParseFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentBookmarkParser::OnParseFinished(
    user_data_importer::BookmarkParser::BookmarkParsingCallback callback,
    user_data_importer::BookmarkParser::ParsedBookmarks parsed_bookmarks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run(std::move(parsed_bookmarks));
}

}  // namespace user_data_importer
