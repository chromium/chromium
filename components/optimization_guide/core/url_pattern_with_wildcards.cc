// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/url_pattern_with_wildcards.h"

#include "base/check_op.h"

namespace {

// Splits |url_pattern| by wildcard, and returns the split patterns.
std::vector<std::string> SplitURLPattern(const std::string& url_pattern) {
  std::vector<std::string> split_subpatterns;
  size_t search_start_pos = 0;
  while (true) {
    size_t next_wildcard_pos = url_pattern.find('*', search_start_pos);
    if (next_wildcard_pos == std::string::npos) {
      // Add rest of the |url_pattern|, and return.
      split_subpatterns.push_back(url_pattern.substr(search_start_pos));
      return split_subpatterns;
    }
    if (next_wildcard_pos == search_start_pos) {
      // Skip empty subpatterns. This catches any explicit wildcards at the
      // front and back, as well as repeated consecutive wildcards.
      search_start_pos++;
      continue;
    }
    // Add the subpattern from |search_start_pos| (inclusive) to
    // |next_wildcard_pos| (not inclusive).
    split_subpatterns.push_back(url_pattern.substr(
        search_start_pos, next_wildcard_pos - search_start_pos));
    search_start_pos = next_wildcard_pos + 1;
  }
}

}  // namespace

namespace optimization_guide {

URLPatternWithWildcards::URLPatternWithWildcards(const std::string& url_pattern)
    : split_subpatterns_(SplitURLPattern(url_pattern)) {
  DCHECK(!url_pattern.empty());
  DCHECK(!split_subpatterns_.empty());
}

URLPatternWithWildcards::URLPatternWithWildcards(
    const URLPatternWithWildcards& other) = default;
URLPatternWithWildcards::~URLPatternWithWildcards() = default;

bool URLPatternWithWildcards::Matches(const std::string& url_string) const {
  // Determine if |url_string| matches |this| pattern. This determination is
  // made by searching all the subpatterns in |split_subpatterns_| while
  // traversing |url_string| . If all the subpatterns in |split_subpatterns_|
  // are found in |url_string|, then it's a match.

  // Note that each of the subpattern belonging in |split_subpatterns_| should
  // be located in |url_string| after the location of the previous subpattern.
  //
  // Example: If |split_subpatterns_| is {"example.com", "foo"}, and
  // |url_string| is example.com/pages/foo.jpg, then first "example.com" is
  // searched in |url_string| beginning at index 0. Then, "foo" is searched in
  // |url_string| beginning at index 10.

  size_t search_start_pos = 0;
  for (const auto& subpattern : split_subpatterns_) {
    DCHECK_GE(url_string.length(), search_start_pos);
    search_start_pos = url_string.find(subpattern, search_start_pos);
    // |url_string| does not match |this| pattern.
    if (search_start_pos == std::string::npos)
      return false;
    // Move the search position for next subpattern to be after where
    // |subpattern| ends in |url_string|.
    search_start_pos += subpattern.length();
  }
  return true;
}

}  // namespace optimization_guide
