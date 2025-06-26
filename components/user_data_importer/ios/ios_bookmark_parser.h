// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_

#import "components/user_data_importer/utility/bookmark_parser.h"

@class LocalNavigationForwarder;
@class WKWebView;

namespace base {
class FilePath;
}

namespace user_data_importer {

// iOS implementation of the BookmarkParser interface. Uses WKWebView as a
// JavaScript environment where parsing can occur in a memory-safe language.
class IOSBookmarkParser : public BookmarkParser {
 public:
  IOSBookmarkParser();
  ~IOSBookmarkParser() override;

  void Parse(const base::FilePath& file,
             BookmarkParser::BookmarkParsingCallback callback) override;

 private:
  // Injects JS into the WebView to cause parsing of the currently loaded
  // content.
  void TriggerParseInJS(BookmarkParser::BookmarkParsingCallback callback);

  // Delegate used to observe loading in `web_view_` and trigger parsing at the
  // appropriate time. Declared as a member here because it is not retained
  // by the WebView once set.
  LocalNavigationForwarder* forwarder_;

  // Environment where the bookmarks HTML file is loaded and JS is executed
  // to parse the contents.
  WKWebView* web_view_;

  base::WeakPtrFactory<IOSBookmarkParser> weak_factory_{this};
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_IOS_IOS_BOOKMARK_PARSER_H_
