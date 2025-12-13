// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_

#include <string>

#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"

namespace user_data_importer {

// A wrapper on BookmarkHtmlParser that mimics the sandbox behaviour.
class FakeBookmarkHtmlParser : public mojom::BookmarkHtmlParser {
 public:
  FakeBookmarkHtmlParser();
  ~FakeBookmarkHtmlParser() override;

  void Parse(const std::string& raw_html, ParseCallback callback) override;

  const std::string& last_html() const { return last_html_; }

 private:
  std::string last_html_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_
