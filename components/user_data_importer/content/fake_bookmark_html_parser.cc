// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/fake_bookmark_html_parser.h"

namespace user_data_importer {

FakeBookmarkHtmlParser::FakeBookmarkHtmlParser() = default;

FakeBookmarkHtmlParser::~FakeBookmarkHtmlParser() = default;

void FakeBookmarkHtmlParser::Parse(const std::string& raw_html,
                                   ParseCallback callback) {
  parser_.Parse(raw_html, std::move(callback));
}

}  // namespace user_data_importer
