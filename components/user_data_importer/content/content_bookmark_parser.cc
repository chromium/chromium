// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser.h"

namespace user_data_importer {

// Declared in bookmark_parser.h.
std::unique_ptr<BookmarkParser> MakeBookmarkParser() {
  return std::make_unique<ContentBookmarkParser>();
}

ContentBookmarkParser::ContentBookmarkParser() = default;
ContentBookmarkParser::~ContentBookmarkParser() = default;

void ContentBookmarkParser::Parse(
    const base::FilePath& file,
    BookmarkParser::BookmarkParsingCallback callback) {
  // Not yet implemented.
  std::move(callback).Run(
      base::unexpected(BookmarkParser::BookmarkParsingError::kOther));
}

}  // namespace user_data_importer
