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
#include "base/unguessable_token.h"

namespace printing {

std::optional<PrintPreviewIdAndPageIndex> ParseDataPath(
    const std::string& path) {
  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (base::EndsWith(file_path, "/test.pdf", base::CompareCase::SENSITIVE)) {
    return PrintPreviewIdAndPageIndex{
        .ui_id = base::UnguessableToken::Deserialize(0x123456789abcdef0,
                                                     0x0fedcba987654321)
                     .value(),
        .page_index = 0,
    };
  }

  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3) {
    return std::nullopt;
  }

  std::optional<base::UnguessableToken> ui_id =
      base::UnguessableToken::DeserializeFromString(url_substr[0]);
  if (!ui_id.has_value()) {
    return std::nullopt;
  }

  int page_index;
  if (!base::StringToInt(url_substr[1], &page_index)) {
    return std::nullopt;
  }

  return PrintPreviewIdAndPageIndex{
      .ui_id = ui_id.value(),
      .page_index = page_index,
  };
}

}  // namespace printing
