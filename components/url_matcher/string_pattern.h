// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_STRING_PATTERN_H_
#define COMPONENTS_URL_MATCHER_STRING_PATTERN_H_

#include <string>

#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {

// An individual pattern of a substring or regex matcher. A pattern consists of
// a string (interpreted as individual bytes, no character encoding) and an
// identifier.
// IDs are returned to the caller of SubstringSetMatcher::Match() or
// RegexMatcher::MatchURL() to help the caller to figure out what
// patterns matched a string. All patterns registered to a matcher
// need to contain unique IDs.
class URL_MATCHER_EXPORT StringPattern {
 public:
  typedef int ID;

  // An invalid ID value. Clients must not use this as the id.
  static constexpr ID kInvalidId = -1;

  StringPattern(std::string pattern, ID id);

  StringPattern(const StringPattern&) = delete;
  StringPattern& operator=(const StringPattern&) = delete;

  ~StringPattern();
  StringPattern(StringPattern&&);
  StringPattern& operator=(StringPattern&&);
  const std::string& pattern() const { return pattern_; }
  ID id() const { return id_; }

  bool operator<(const StringPattern& rhs) const;

 private:
  std::string pattern_;
  ID id_;
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_STRING_PATTERN_H_
