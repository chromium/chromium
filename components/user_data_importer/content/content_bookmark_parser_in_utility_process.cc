// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser_in_utility_process.h"

#include <utility>

#include "components/user_data_importer/content/content_bookmark_parser_utils.h"
#include "components/user_data_importer/content/favicon_reencode.h"

namespace user_data_importer {

ContentBookmarkParserInUtilityProcess::ContentBookmarkParserInUtilityProcess(
    mojo::PendingReceiver<mojom::BookmarkHtmlParser> receiver)
    : receiver_(this, std::move(receiver)) {}

ContentBookmarkParserInUtilityProcess::
    ~ContentBookmarkParserInUtilityProcess() = default;

void ContentBookmarkParserInUtilityProcess::Parse(const std::string& raw_html,
                                                  ParseCallback callback) {
  std::move(callback).Run(ParseBookmarksUnsafe(raw_html));
}

void ContentBookmarkParserInUtilityProcess::ReencodeFavicons(
    const base::flat_map<uint32_t, std::vector<uint8_t>>& raw_favicons,
    ReencodeFaviconsCallback callback) {
  std::move(callback).Run(importer::ReencodeFavicons(raw_favicons));
}

}  // namespace user_data_importer
