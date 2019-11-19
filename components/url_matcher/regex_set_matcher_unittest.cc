// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/regex_set_matcher.h"

#include <set>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_matcher {

TEST(RegexSetMatcherTest, MatchRegexes) {
  StringPattern pattern_1("ab.*c", 42);
  StringPattern pattern_2("f*f", 17);
  StringPattern pattern_3("c(ar|ra)b|brac", 239);
  std::vector<const StringPattern*> regexes;
  regexes.push_back(&pattern_1);
  regexes.push_back(&pattern_2);
  regexes.push_back(&pattern_3);
  RegexSetMatcher matcher;
  matcher.AddPatterns(regexes);

  std::set<StringPattern::ID> result1;
  matcher.Match("http://abracadabra.com", &result1);
  EXPECT_EQ(2U, result1.size());
  EXPECT_TRUE(base::Contains(result1, 42));
  EXPECT_TRUE(base::Contains(result1, 239));

  std::set<StringPattern::ID> result2;
  matcher.Match("https://abfffffffffffffffffffffffffffffff.fi/cf", &result2);
  EXPECT_EQ(2U, result2.size());
  EXPECT_TRUE(base::Contains(result2, 17));
  EXPECT_TRUE(base::Contains(result2, 42));

  std::set<StringPattern::ID> result3;
  matcher.Match("http://nothing.com/", &result3);
  EXPECT_EQ(0U, result3.size());
}

TEST(RegexSetMatcherTest, CaseSensitivity) {
  StringPattern pattern_1("AAA", 51);
  StringPattern pattern_2("aaA", 57);
  std::vector<const StringPattern*> regexes;
  regexes.push_back(&pattern_1);
  regexes.push_back(&pattern_2);
  RegexSetMatcher matcher;
  matcher.AddPatterns(regexes);

  std::set<StringPattern::ID> result1;
  matcher.Match("http://aaa.net/", &result1);
  EXPECT_EQ(0U, result1.size());

  std::set<StringPattern::ID> result2;
  matcher.Match("http://aaa.net/quaaACK", &result2);
  EXPECT_EQ(1U, result2.size());
  EXPECT_TRUE(base::Contains(result2, 57));
}

}  // namespace url_matcher
