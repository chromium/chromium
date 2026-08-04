// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_

#include <stdint.h>

#include "chrome/utility/importer/importer.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom-forward.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// Importer for bookmarks files.
class BookmarksFileImporter : public Importer {
 public:
  BookmarksFileImporter();

  BookmarksFileImporter(const BookmarksFileImporter&) = delete;
  BookmarksFileImporter& operator=(const BookmarksFileImporter&) = delete;

  void StartImport(const user_data_importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

  void SetBookmarkHtmlParser(
      mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser)
      override;

 private:
  ~BookmarksFileImporter() override;

  void OnBookmarksParsed(
      std::unique_ptr<mojo::Remote<
          user_data_importer::mojom::BookmarkHtmlParser>> html_parser,
      user_data_importer::BookmarkParser::ParsedBookmarks parsed_bookmarks);

  mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser>
      html_parser_remote_;
};

#endif  // CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
