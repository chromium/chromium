// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_

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
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
