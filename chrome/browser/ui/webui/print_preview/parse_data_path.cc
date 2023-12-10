// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/parse_data_path.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace printing {

std::optional<PrintPreviewIdAndPageIndex> ParseDataPath(
    const std::string& path) {
  PrintPreviewIdAndPageIndex parsed = {
      .ui_id = -1,
      .page_index = 0,
  };

  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (base::EndsWith(file_path, "/test.pdf", base::CompareCase::SENSITIVE)) {
    return parsed;
  }

  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3) {
    return std::nullopt;
  }

  if (!base::StringToInt(url_substr[0], &parsed.ui_id) || parsed.ui_id < 0) {
    return std::nullopt;
  }

  if (!base::StringToInt(url_substr[1], &parsed.page_index)) {
    return std::nullopt;
  }

  return parsed;
}

}  // namespace printing
