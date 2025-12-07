// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_UTILS_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_UTILS_H_

#include <string>

#include "components/user_data_importer/utility/bookmark_parser.h"

class GURL;

namespace user_data_importer {

// Parses the contents of a bookmark HTML file. Tries to recover as much data
// from `raw_html` as possible, even if the entire html is not valid or in the
// expected format. If the entire input is not valid, it returns an empty
// `ParsedBookmarks`. This function must be run in a sandboxed process.
BookmarkParser::ParsedBookmarks ParseBookmarksUnsafe(
    const std::string& raw_html);

// Returns true if `url` should be imported as a search engine, i.e. because it
// has replacement terms. Chrome treats such bookmarks as search engines rather
// than true bookmarks.
bool CanImportURLAsSearchEngine(const GURL& url,
                                std::string* search_engine_url);

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_UTILS_H_
