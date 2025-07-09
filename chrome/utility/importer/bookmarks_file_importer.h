// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_

#include <stdint.h>

#include "chrome/utility/importer/importer.h"
#include "components/user_data_importer/utility/bookmark_parser.h"

// Importer for bookmarks files.
class BookmarksFileImporter : public Importer {
 public:
  BookmarksFileImporter();

  BookmarksFileImporter(const BookmarksFileImporter&) = delete;
  BookmarksFileImporter& operator=(const BookmarksFileImporter&) = delete;

  void StartImport(const user_data_importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  // Receives the result of parsing bookmarks and search engines and notifies
  // the `bridge` of the necessary updates.
  void OnBookmarksParsed(
      user_data_importer::BookmarkParser::BookmarkParsingResult result);

  ~BookmarksFileImporter() override;
};

#endif  // CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
