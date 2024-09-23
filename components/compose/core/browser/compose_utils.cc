// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_utils.h"

#include "base/strings/string_tokenizer.h"
#include "base/third_party/icu/icu_utf.h"

namespace compose {

namespace {

std::string RemoveLastCharIfInvalid(std::string str) {
  std::string trimmed_value;
  base::TruncateUTF8ToByteSize(str, str.size(), &trimmed_value);
  return trimmed_value;
}

const std::string kWhitespace = " ,.\r\n\t\f\v";
}

bool IsWordCountWithinBounds(const std::string& prompt,
                             unsigned int minimum,
                             unsigned int maximum) {
  base::StringTokenizer tokenizer(
      prompt, kWhitespace, base::StringTokenizer::WhitespacePolicy::kSkipOver);

  unsigned int word_count = 0;
  while (tokenizer.GetNext()) {
    ++word_count;
    if (word_count > maximum) {
      return false;
    }
  }

  if (word_count < minimum) {
    return false;
  }
  return true;
}

std::string GetTrimmedPageText(std::string inner_text,
                               int max_length,
                               int element_offset,
                               int header_length) {
  std::string separator = "...\n";
  max_length = max_length - separator.length();
  header_length = std::min(int(inner_text.length()), header_length);
  element_offset = std::min(int(inner_text.length()), element_offset);

  int header_end = header_length;
  int local_start = element_offset - (max_length - header_length);
  int local_end = std::max(max_length, element_offset);
  if (local_start <= header_end) {
    local_end = std::min(int(inner_text.length()),
                         max_length + int(separator.length()));
    if (local_end == int(inner_text.length())) {
      return inner_text;
    }
    return inner_text.substr(0, local_end);
  }
  if (local_end >= int(inner_text.length())) {
    local_end = inner_text.length();
  }
  std::string header =
      RemoveLastCharIfInvalid(inner_text.substr(0, header_end));
  std::string local = RemoveLastCharIfInvalid(
      inner_text.substr(local_start, local_end - local_start));
  return header.append(separator).append(local);
}

}  // namespace compose
