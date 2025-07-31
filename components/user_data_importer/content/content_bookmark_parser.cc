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
scoped_refptr<BookmarkParser> MakeBookmarkParser() {
  return base::MakeRefCounted<ContentBookmarkParser>();
}

ContentBookmarkParser::ContentBookmarkParser() = default;

ContentBookmarkParser::~ContentBookmarkParser() {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    // The remote must be destroyed on the sequence it was bound on.
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::DoNothingWithBoundArgs(std::move(html_parser_remote_)));
  }
}

void ContentBookmarkParser::SetServiceForTesting(
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser) {
  html_parser_remote_.Bind(std::move(parser));
}

void ContentBookmarkParser::Parse(
    const base::FilePath& file_path,
    BookmarkParser::BookmarkParsingCallback callback) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string raw_html;
  // ReadFileToString can return false, but still populate something into
  // `raw_html`. In that case, try to recover as much data as possible.
  // (ParseOnUIThread() will report an error if `raw_html` is empty, i.e. the
  // read failed entirely.)
  base::ReadFileToString(file_path, &raw_html);

  auto callback_on_thread = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));

  // It's not safe to use a weak pointer here because this call is made to a
  // different sequence. As such, using a wrap ref counted pointer.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ContentBookmarkParser::ParseOnUIThread,
                                base::WrapRefCounted(this), std::move(raw_html),
                                std::move(callback_on_thread)));
}

void ContentBookmarkParser::Parse(base::File file,
                                  BookmarkParsingCallback callback) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string raw_html;
  // ReadStreamToString can return false, but still populate something into
  // `raw_html`. In that case, try to recover as much data as possible.
  // (ParseOnUIThread() will report an error if `raw_html` is empty, i.e. the
  // read failed entirely.)
  base::ReadStreamToString(base::FileToFILE(std::move(file), "rb"), &raw_html);

  auto callback_on_thread = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));

  // It's not safe to use a weak pointer here because this call is made to a
  // different sequence. As such, using a wrap ref counted pointer.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ContentBookmarkParser::ParseOnUIThread,
                                base::WrapRefCounted(this), std::move(raw_html),
                                std::move(callback_on_thread)));
}

void ContentBookmarkParser::ParseOnUIThread(
    std::string raw_html,
    BookmarkParser::BookmarkParsingCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (raw_html.empty()) {
    std::move(callback).Run(base::unexpected(
        BookmarkParser::BookmarkParsingError::kFailedToReadFile));
    return;
  }

  if (!html_parser_remote_) {
    html_parser_remote_ = content::ServiceProcessHost::Launch<
        user_data_importer::mojom::BookmarkHtmlParser>(
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_CONTENT_BOOKMARK_PARSER_SERVICE_DISPLAY_NAME)
            .Pass());
    html_parser_remote_.reset_on_disconnect();
  }

  html_parser_remote_->Parse(
      std::move(raw_html),
      base::BindOnce(&ContentBookmarkParser::OnParseFinished,
                     base::WrapRefCounted(this), std::move(callback)));
}

void ContentBookmarkParser::OnParseFinished(
    user_data_importer::BookmarkParser::BookmarkParsingCallback callback,
    user_data_importer::BookmarkParser::ParsedBookmarks parsed_bookmarks) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(std::move(parsed_bookmarks));
}

}  // namespace user_data_importer
