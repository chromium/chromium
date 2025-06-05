// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_MANAGER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_MANAGER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"

namespace base {
class FilePath;
}

namespace user_data_importer {

// Interface for providing platform-specific implementations of certain
// model-layer logic (e.g., some parsing).
class SafariDataImportManager {
 public:
  // Result of a successful invocation of `ParseBookmarks` below.
  struct ParsedBookmarks {
    ParsedBookmarks();
    ~ParsedBookmarks();

    // List of standard bookmarks and folders.
    std::vector<ImportedBookmarkEntry> bookmarks;

    // Safari includes Reading List entries in bookmarks.html.
    std::vector<ImportedBookmarkEntry> reading_list;
  };

  // Failure reason for an unsuccessful invocation of `ParseBookmarks` below.
  enum class BookmarkParsingError {
    // The file was larger than the maximum supported by this manager.
    kTooBig,

    // The file could not be parsed (e.g., bad syntax).
    kParsingFailed,

    // The operation did not complete within the allotted time.
    kTimedOut
  };

  virtual ~SafariDataImportManager() = default;

  using BookmarkParsingResult =
      base::expected<ParsedBookmarks, BookmarkParsingError>;

  // Opens the file at the given FilePath, treating it as an HTML file matching
  // the Netscape bookmarks format:
  // https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa753582(v=vs.85)
  // Parses the document and extracts bookmarks and reading list entries.
  // Invokes `callback` with the result of parsing.
  virtual void ParseBookmarks(
      const base::FilePath& bookmarks_html,
      base::OnceCallback<void(BookmarkParsingResult)> callback) = 0;
};

}  //  namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_MANAGER_H_
