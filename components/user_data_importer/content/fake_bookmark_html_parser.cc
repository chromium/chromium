// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/fake_bookmark_html_parser.h"

#include "components/user_data_importer/content/content_bookmark_parser_utils.h"

namespace user_data_importer {

FakeBookmarkHtmlParser::FakeBookmarkHtmlParser() = default;

FakeBookmarkHtmlParser::~FakeBookmarkHtmlParser() = default;

void FakeBookmarkHtmlParser::Parse(const std::string& raw_html,
                                   ParseCallback callback) {
  last_html_ = raw_html;
  std::move(callback).Run(ParseBookmarksUnsafe(std::move(raw_html)));
}

}  // namespace user_data_importer
