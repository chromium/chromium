// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/regex_set_matcher.h"

#include <set>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::MatcherStringPattern;
using base::SubstringSetMatcher;

namespace url_matcher {

TEST(RegexSetMatcherTest, MatchRegexes) {
  MatcherStringPattern pattern_1("ab.*c", 42);
  MatcherStringPattern pattern_2("f*f", 17);
  MatcherStringPattern pattern_3("c(ar|ra)b|brac", 239);
  std::vector<const MatcherStringPattern*> regexes;
  regexes.push_back(&pattern_1);
  regexes.push_back(&pattern_2);
  regexes.push_back(&pattern_3);
  RegexSetMatcher matcher;
  matcher.AddPatterns(regexes);

  std::set<MatcherStringPattern::ID> result1;
  matcher.Match("http://abracadabra.com", &result1);
  EXPECT_EQ(2U, result1.size());
  EXPECT_TRUE(base::Contains(result1, 42));
  EXPECT_TRUE(base::Contains(result1, 239));

  std::set<MatcherStringPattern::ID> result2;
  matcher.Match("https://abfffffffffffffffffffffffffffffff.fi/cf", &result2);
  EXPECT_EQ(2U, result2.size());
  EXPECT_TRUE(base::Contains(result2, 17));
  EXPECT_TRUE(base::Contains(result2, 42));

  std::set<MatcherStringPattern::ID> result3;
  matcher.Match("http://nothing.com/", &result3);
  EXPECT_EQ(0U, result3.size());
}

TEST(RegexSetMatcherTest, CaseSensitivity) {
  MatcherStringPattern pattern_1("AAA", 51);
  MatcherStringPattern pattern_2("aaA", 57);
  std::vector<const MatcherStringPattern*> regexes;
  regexes.push_back(&pattern_1);
  regexes.push_back(&pattern_2);
  RegexSetMatcher matcher;
  matcher.AddPatterns(regexes);

  std::set<MatcherStringPattern::ID> result1;
  matcher.Match("http://aaa.net/", &result1);
  EXPECT_EQ(0U, result1.size());

  std::set<MatcherStringPattern::ID> result2;
  matcher.Match("http://aaa.net/quaaACK", &result2);
  EXPECT_EQ(1U, result2.size());
  EXPECT_TRUE(base::Contains(result2, 57));
}

}  // namespace url_matcher
