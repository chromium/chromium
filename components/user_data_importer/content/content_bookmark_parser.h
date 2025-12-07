// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_

#include "base/sequence_checker.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class File;
class FilePath;
}

namespace user_data_importer {
namespace mojom {
class BookmarkHtmlParser;
}

// Content implementation of the BookmarkParser interface. This class reads the
// bookmarks HTML file contents and then launches, on the utility process, the
// actual parsing of the file contents, which are from an untrusted data source.
//
// Can be created on any sequence (e.g. on a UI thread), but there after, must
// be used and destroyed on the same background sequence.
class ContentBookmarkParser : public BookmarkParser {
 public:
  ContentBookmarkParser();
  ~ContentBookmarkParser() override;

  // BookmarkParser:
  // Reads the file contents and then launches the actual parsing on the utility
  // process. Preferably, this method should be called on the background thread.
  void Parse(const base::FilePath& file,
             BookmarkParsingCallback callback) override;

  // Same as the Parse() above, but reads from a base::File.
  void Parse(base::File file, BookmarkParsingCallback callback);

  void SetServiceForTesting(
      mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser>
          parser);

 private:
  void ParseImpl(std::string raw_html,
                 BookmarkParser::BookmarkParsingCallback callback);

  void OnParseFinished(
      BookmarkParser::BookmarkParsingCallback callback,
      user_data_importer::BookmarkParser::ParsedBookmarks parsed_bookmarks);

  // The utility process host used to run the parser.
  mojo::Remote<mojom::BookmarkHtmlParser> html_parser_remote_;

  // HTML parser to use for testing. If set, `html_parser_remote_` will be bound
  // to this instead of being launched in a utility process.
  mojo::PendingRemote<mojom::BookmarkHtmlParser> html_parser_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ContentBookmarkParser> weak_ptr_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_H_
