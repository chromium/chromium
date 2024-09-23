// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'.
//
// In addition, a separator placeholder at the end of the pattern can be matched
// by the end of |text|. This should be handled by the clients using the
// following utility functions.
//
// We define a fuzzy occurrence as an occurrence of a |subpattern| in |text|
// such that all its non-placeholder characters are equal to the corresponding
// characters of the |text|, whereas each '^' placeholder can correspond to any
// type of separator in |text|.

#ifndef COMPONENTS_URL_PATTERN_INDEX_FUZZY_PATTERN_MATCHING_H_
#define COMPONENTS_URL_PATTERN_INDEX_FUZZY_PATTERN_MATCHING_H_

#include <stddef.h>

#include <string_view>

namespace url_pattern_index {

constexpr char kSeparatorPlaceholder = '^';

inline bool IsAscii(char c) {
  return !(c & ~0x7F);
}

inline bool IsAlphaNumericAscii(char c) {
  if (c <= '9')
    return c >= '0';
  c |= 0x20;  // Puts all alphabetics (and only them) into the 'a'-'z' range.
  return c >= 'a' && c <= 'z';
}

// Returns whether |c| is a separator.
inline bool IsSeparator(char c) {
  switch (c) {
    case '_':
    case '-':
    case '.':
    case '%':
      return false;
    case kSeparatorPlaceholder:
      return true;
    default:
      return !IsAlphaNumericAscii(c) && IsAscii(c);
  }
}

// Returns whether |text| starts with a fuzzy occurrence of |subpattern|.
bool StartsWithFuzzy(std::string_view text, std::string_view subpattern);

// Returns whether |text| ends with a fuzzy occurrence of |subpattern|.
bool EndsWithFuzzy(std::string_view text, std::string_view subpattern);

// Returns the position of the leftmost fuzzy occurrence of a |subpattern| in
// the |text| starting no earlier than |from| the specified position.
size_t FindFuzzy(std::string_view text,
                 std::string_view subpattern,
                 size_t from = 0);

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_FUZZY_PATTERN_MATCHING_H_
