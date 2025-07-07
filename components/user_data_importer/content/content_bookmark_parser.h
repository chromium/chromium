// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_

#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/utility/bookmark_parser.h"

namespace base {
class FilePath;
}

namespace user_data_importer {

// Content implementation of the BookmarkParser interface.
class ContentBookmarkParser : public BookmarkParser {
 public:
  ContentBookmarkParser();
  ~ContentBookmarkParser() override;

  void Parse(const base::FilePath& file,
             BookmarkParser::BookmarkParsingCallback callback) override;

  // Imports the bookmarks from the specified file.
  //
  // |cancellation_callback| is polled to query if the import should be
  // cancelled; if it returns |true| at any time the import will be cancelled.
  // If |cancellation_callback| is a null callback the import will run to
  // completion.
  //
  // |valid_url_callback| is called to determine if a specified URL is valid for
  // import; it returns |true| if it is. If |valid_url_callback| is a null
  // callback, all URLs are considered to be valid.
  //
  // |file_path| is the path of the file on disk to import.
  //
  // |bookmarks| is a pointer to a vector, which is filled with the imported
  // bookmarks. It may not be NULL.
  //
  // |search_engines| is a pointer to a vector, which is filled with the
  // imported search engines.
  //
  // |favicons| is a pointer to a vector, which is filled with the favicons of
  // imported bookmarks. It may be NULL, in which case favicons are not
  // imported.
  void Parse(base::RepeatingCallback<bool(void)> cancellation_callback,
             base::RepeatingCallback<bool(const GURL&)> valid_url_callback,
             const base::FilePath& file_path,
             std::vector<user_data_importer::ImportedBookmarkEntry>* bookmarks,
             std::vector<user_data_importer::SearchEngineInfo>* search_engines,
             favicon_base::FaviconUsageDataList* favicons);
};

// Returns true if |url| should be imported as a search engine, i.e. because it
// has replacement terms. Chrome treats such bookmarks as search engines rather
// than true bookmarks.
bool CanImportURLAsSearchEngine(const GURL& url,
                                std::string* search_engine_url);

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
