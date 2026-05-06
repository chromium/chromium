// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_

#include <string>

#include "base/threading/sequence_bound.h"
#include "base/values.h"
#import "components/user_data_importer/utility/bookmark_parser.h"

namespace base {
class FilePath;
}

namespace user_data_importer {

class WebViewRunner;

// iOS implementation of the BookmarkParser interface. Uses WKWebView as a
// JavaScript environment where parsing can occur in a memory-safe language.
class IOSBookmarkParser : public BookmarkParser {
 public:
  IOSBookmarkParser();
  ~IOSBookmarkParser() override;

  void Parse(const base::FilePath& file,
             BookmarkParser::BookmarkParsingCallback callback) override;

 private:
  // Invoked on this object's default sequence when the WebViewRunner has
  // completed parsing in JavaScript.
  void OnJSResult(BookmarkParser::BookmarkParsingCallback callback,
                  std::string result,
                  NSError* error);

  // Encapsulates the parts of this flow that must be run on the main (UI)
  // thread.
  base::SequenceBound<WebViewRunner> runner_;

  base::WeakPtrFactory<IOSBookmarkParser> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_
