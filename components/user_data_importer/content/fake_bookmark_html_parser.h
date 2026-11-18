// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"

namespace user_data_importer {

// A wrapper on BookmarkHtmlParser that mimics the sandbox behaviour.
class FakeBookmarkHtmlParser : public mojom::BookmarkHtmlParser {
 public:
  FakeBookmarkHtmlParser();
  ~FakeBookmarkHtmlParser() override;

  void Parse(const std::string& raw_html, ParseCallback callback) override;
  void ReencodeFavicons(
      const base::flat_map<uint32_t, std::vector<uint8_t>>& raw_favicons,
      ReencodeFaviconsCallback callback) override;

  const std::string& last_html() const { return last_html_; }
  size_t reencode_favicons_call_count() const {
    return reencode_favicons_call_count_;
  }

  // If set, `ReencodeFavicons()` invokes this closure (e.g. to reset the
  // test's `mojo::Receiver`) instead of replying.
  void set_reencode_favicons_override_for_testing(
      base::OnceClosure override_closure) {
    reencode_favicons_override_ = std::move(override_closure);
  }

 private:
  std::string last_html_;
  size_t reencode_favicons_call_count_ = 0;
  base::OnceClosure reencode_favicons_override_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAKE_BOOKMARK_HTML_PARSER_H_
