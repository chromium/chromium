// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/fuzzy_pattern_matching.h"

#include <algorithm>
#include <string_view>

#include "base/check_op.h"

namespace url_pattern_index {

namespace {

bool StartsWithFuzzyImpl(std::string_view text, std::string_view subpattern) {
  DCHECK_LE(subpattern.size(), text.size());

  for (size_t i = 0; i != subpattern.size(); ++i) {
    const char text_char = text[i];
    const char pattern_char = subpattern[i];
    if (text_char != pattern_char &&
        (pattern_char != kSeparatorPlaceholder || !IsSeparator(text_char))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool StartsWithFuzzy(std::string_view text, std::string_view subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text, subpattern);
}

bool EndsWithFuzzy(std::string_view text, std::string_view subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text.substr(text.size() - subpattern.size()),
                             subpattern);
}

size_t FindFuzzy(std::string_view text,
                 std::string_view subpattern,
                 size_t from) {
  if (from > text.size())
    return std::string_view::npos;
  if (subpattern.empty())
    return from;

  auto fuzzy_compare = [](char text_char, char subpattern_char) {
    return text_char == subpattern_char ||
           (subpattern_char == kSeparatorPlaceholder && IsSeparator(text_char));
  };

  std::string_view::const_iterator found =
      std::search(text.begin() + from, text.end(), subpattern.begin(),
                  subpattern.end(), fuzzy_compare);
  return found == text.end() ? std::string_view::npos : found - text.begin();
}

}  // namespace url_pattern_index
