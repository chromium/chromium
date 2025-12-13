// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_IN_UTILITY_PROCESS_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_IN_UTILITY_PROCESS_H_

#include <string>

#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace user_data_importer {

// This class should be run in a sandboxed process and parse the contents of a
// bookmark HTML file. It implements the mojom::BookmarkHtmlParser interface to
// that effect.
class ContentBookmarkParserInUtilityProcess : public mojom::BookmarkHtmlParser {
 public:
  explicit ContentBookmarkParserInUtilityProcess(
      mojo::PendingReceiver<mojom::BookmarkHtmlParser> receiver);
  ~ContentBookmarkParserInUtilityProcess() override;

  ContentBookmarkParserInUtilityProcess(
      const ContentBookmarkParserInUtilityProcess&) = delete;
  ContentBookmarkParserInUtilityProcess& operator=(
      const ContentBookmarkParserInUtilityProcess&) = delete;

  // mojom::BookmarkHtmlParser:
  void Parse(const std::string& raw_html, ParseCallback callback) override;

 private:
  mojo::Receiver<mojom::BookmarkHtmlParser> receiver_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_CONTENT_BOOKMARK_PARSER_IN_UTILITY_PROCESS_H_
