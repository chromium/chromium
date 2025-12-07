// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_PARSER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"

namespace base {
class FilePath;
}

namespace user_data_importer {

// Interface for opening and parsing an HTML file containing bookmarks.
class BookmarkParser {
 public:
  // Result of a successful invocation of `ParseBookmarks` below.
  struct ParsedBookmarks {
    ParsedBookmarks();
    ~ParsedBookmarks();

    // Moveable, but not copyable.
    ParsedBookmarks(ParsedBookmarks&&);
    ParsedBookmarks& operator=(ParsedBookmarks&&);
    ParsedBookmarks(const ParsedBookmarks&) = delete;
    ParsedBookmarks& operator=(const ParsedBookmarks&) = delete;

    // List of standard bookmarks and folders.
    std::vector<ImportedBookmarkEntry> bookmarks;

    // Safari includes Reading List entries in bookmarks.html.
    std::vector<ImportedBookmarkEntry> reading_list;

    // Firefox includes Search Engines entries in bookmarks.html.
    std::vector<SearchEngineInfo> search_engines;

    // Favicons usage data list.
    favicon_base::FaviconUsageDataList favicons;
  };

  // Failure reason for an unsuccessful invocation of `ParseBookmarks` below.
  enum class BookmarkParsingError {
    // Failed to read file, e.g. no such file exists.
    kFailedToReadFile,

    // The file was larger than the maximum supported by this manager.
    kTooBig,

    // The file could not be parsed (e.g., bad syntax).
    kParsingFailed,

    // The operation did not complete within the allotted time.
    kTimedOut,

    // Generic error.
    kOther
  };

  virtual ~BookmarkParser() = default;

  using BookmarkParsingResult =
      base::expected<ParsedBookmarks, BookmarkParsingError>;
  using BookmarkParsingCallback =
      base::OnceCallback<void(BookmarkParsingResult)>;

  // Opens the file at the given FilePath, treating it as an HTML file matching
  // the Netscape bookmarks format:
  // https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa753582(v=vs.85)
  // Parses the document and extracts bookmarks and reading list entries.
  // Invokes `callback` with the result of parsing.
  virtual void Parse(const base::FilePath& bookmarks_html,
                     BookmarkParsingCallback callback) = 0;
};

// Returns a suitable concrete BookmarkParser instance. See implementations in
// ios_bookmark_parser.mm and content_bookmark_parser.cc.
std::unique_ptr<BookmarkParser> MakeBookmarkParser();

}  //  namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_PARSER_H_
