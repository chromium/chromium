// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/js_in_process_fuzzer_utils.h"

#include "base/strings/string_util.h"

namespace js_in_process_fuzzer {

std::string_view GetFirstNonCommentLine(std::string_view js) {
  size_t idx = 0;
  std::string_view line;
  while (idx < js.length()) {
    size_t line_end = js.find('\n', idx);
    if (line_end == std::string_view::npos) {
      line_end = js.length();
    }
    line = base::TrimWhitespaceASCII(js.substr(idx, line_end - idx),
                                     base::TRIM_ALL);
    idx = line_end + 1;

    if (!line.starts_with("//")) {
      return line;
    }
  }
  return {};
}

}  // namespace js_in_process_fuzzer
