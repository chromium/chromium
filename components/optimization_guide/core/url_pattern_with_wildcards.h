// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_URL_PATTERN_WITH_WILDCARDS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_URL_PATTERN_WITH_WILDCARDS_H_

#include <stddef.h>

#include <string>
#include <vector>

namespace optimization_guide {

// URLPatternWithWildcards parses and stores one URL pattern. A URL pattern is a
// single substring to match against a URL. A URL pattern may
// contain multiple wildcard characters ('*'), each of which can match more than
// one character. An implicit wildcard character ('*') is assumed to be present
// at the beginning and end of a pattern.
class URLPatternWithWildcards {
 public:
  explicit URLPatternWithWildcards(const std::string& url_pattern);
  URLPatternWithWildcards(const URLPatternWithWildcards& other);
  ~URLPatternWithWildcards();

  URLPatternWithWildcards& operator=(const URLPatternWithWildcards&) = default;

  // Returns true if |url_string| matches |this| pattern.
  bool Matches(const std::string& url_string) const;

 private:
  // A single pattern string is split into multiple strings (each separated by
  // '*'), and stored in |split_subpatterns_|.
  std::vector<std::string> split_subpatterns_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_URL_PATTERN_WITH_WILDCARDS_H_
