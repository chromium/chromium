// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/common/title_validator.h"

#include <string_view>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

namespace continuous_search {

namespace {

// Based on frame.mojom `kMaxTitleChars`.
constexpr size_t kMaxLength = 4096;

// A unicode control character is any character in the set:
// {[U0000, U0020), U007F}
// We explicitly permit U000B LINE TABULATION in keeping with the implementation
// in blink::Documents CanonicalizedTitle method.
bool IsUnicodeWhitespaceOrControl(wchar_t c) {
  return (c < 0x0020 || c == 0x007F || base::IsUnicodeWhitespace(c)) &&
         c != 0x000B;
}

template <typename T, typename CharT = typename T::value_type>
std::basic_string<CharT> ValidateTitleT(T input) {
  auto begin_it =
      base::ranges::find_if_not(input, &IsUnicodeWhitespaceOrControl);
  auto end_it = base::ranges::find_if_not(base::Reversed(input),
                                          &IsUnicodeWhitespaceOrControl);

  std::basic_string<CharT> output;
  if (input.empty() || begin_it == input.end()) {
    return output;
  }

  const size_t first = begin_it - input.begin();
  const size_t last = std::distance(input.begin(), end_it.base());
  DCHECK_GT(last, first);  // Invariant based on the find_if algorithm.
  const size_t length = last - first;
  const size_t max_output_size = std::min(length, kMaxLength);
  output.resize(max_output_size);

  size_t output_pos = 0;
  bool in_whitespace = false;
  for (auto c : input.substr(first, length)) {
    if (IsUnicodeWhitespaceOrControl(c)) {
      if (!in_whitespace) {
        in_whitespace = true;
        output[output_pos++] = L' ';
      }
    } else {
      in_whitespace = false;
      output[output_pos++] = c;
    }
    if (output_pos == kMaxLength) {
      break;
    }
  }

  output.resize(output_pos);
  return output;
}

}  // namespace

std::string ValidateTitleAscii(std::string_view title) {
  return ValidateTitleT(title);
}

std::u16string ValidateTitle(std::u16string_view title) {
  return ValidateTitleT(title);
}

}  // namespace continuous_search
