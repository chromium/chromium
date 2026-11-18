// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/fake_bookmark_html_parser.h"

#include <utility>

#include "components/user_data_importer/content/content_bookmark_parser_utils.h"
#include "components/user_data_importer/content/favicon_reencode.h"

namespace user_data_importer {

FakeBookmarkHtmlParser::FakeBookmarkHtmlParser() = default;

FakeBookmarkHtmlParser::~FakeBookmarkHtmlParser() = default;

void FakeBookmarkHtmlParser::Parse(const std::string& raw_html,
                                   ParseCallback callback) {
  last_html_ = raw_html;
  std::move(callback).Run(ParseBookmarksUnsafe(raw_html));
}

void FakeBookmarkHtmlParser::ReencodeFavicons(
    const base::flat_map<uint32_t, std::vector<uint8_t>>& raw_favicons,
    ReencodeFaviconsCallback callback) {
  ++reencode_favicons_call_count_;
  if (reencode_favicons_override_) {
    std::move(reencode_favicons_override_).Run();
    return;
  }
  std::move(callback).Run(importer::ReencodeFavicons(raw_favicons));
}

}  // namespace user_data_importer
