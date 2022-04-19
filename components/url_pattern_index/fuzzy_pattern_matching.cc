// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/fuzzy_pattern_matching.h"

#include <algorithm>

namespace url_pattern_index {

namespace {

bool StartsWithFuzzyImpl(base::StringPiece text, base::StringPiece subpattern) {
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

bool StartsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text, subpattern);
}

bool EndsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text.substr(text.size() - subpattern.size()),
                             subpattern);
}

size_t FindFuzzy(base::StringPiece text,
                 base::StringPiece subpattern,
                 size_t from) {
  if (from > text.size())
    return base::StringPiece::npos;
  if (subpattern.empty())
    return from;

  auto fuzzy_compare = [](char text_char, char subpattern_char) {
    return text_char == subpattern_char ||
           (subpattern_char == kSeparatorPlaceholder && IsSeparator(text_char));
  };

  base::StringPiece::const_iterator found =
      std::search(text.begin() + from, text.end(), subpattern.begin(),
                  subpattern.end(), fuzzy_compare);
  return found == text.end() ? base::StringPiece::npos : found - text.begin();
}

}  // namespace url_pattern_index
