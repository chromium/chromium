// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_pattern.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_pattern_index {

namespace {

constexpr proto::AnchorType kAnchorNone = proto::ANCHOR_TYPE_NONE;
constexpr proto::AnchorType kBoundary = proto::ANCHOR_TYPE_BOUNDARY;
constexpr proto::AnchorType kSubdomain = proto::ANCHOR_TYPE_SUBDOMAIN;
constexpr UrlPattern::MatchCase kMatchCase = UrlPattern::MatchCase::kTrue;
constexpr UrlPattern::MatchCase kDonotMatchCase = UrlPattern::MatchCase::kFalse;

}  // namespace

TEST(UrlPatternTest, MatchesUrl) {
  const struct {
    UrlPattern url_pattern;
    const char* url;
    bool expect_match;
  } kTestCases[] = {
      {{"", proto::URL_PATTERN_TYPE_SUBSTRING}, "http://ex.com/", true},
      {{"", proto::URL_PATTERN_TYPE_WILDCARDED}, "http://ex.com/", true},
      {{"", kBoundary, kAnchorNone}, "http://ex.com/", true},
      {{"", kSubdomain, kAnchorNone}, "http://ex.com/", true},
      {{"", kSubdomain, kAnchorNone}, "http://ex.com/", true},
      {{"^", kSubdomain, kAnchorNone}, "http://ex.com/", false},
      {{".", kSubdomain, kAnchorNone}, "http://ex.com/", false},
      {{"", kAnchorNone, kBoundary}, "http://ex.com/", true},
      {{"^", kAnchorNone, kBoundary}, "http://ex.com/", true},
      {{".", kAnchorNone, kBoundary}, "http://ex.com/", false},
      {{"", kBoundary, kBoundary}, "http://ex.com/", false},
      {{"", kSubdomain, kBoundary}, "http://ex.com/", false},
      {{"com/", kSubdomain, kBoundary}, "http://ex.com/", true},

      {{"xampl", proto::URL_PATTERN_TYPE_SUBSTRING},
       "http://example.com",
       true},
      {{"example", proto::URL_PATTERN_TYPE_SUBSTRING},
       "http://example.com",
       true},
      {{"/a?a"}, "http://ex.com/a?a", true},
      {{"^abc"}, "http://ex.com/abc?a", true},
      {{"^abc"}, "http://ex.com/a?abc", true},
      {{"^abc"}, "http://ex.com/abc?abc", true},
      {{"^abc^abc"}, "http://ex.com/abc?abc", true},
      {{"^com^abc^abc"}, "http://ex.com/abc?abc", false},

      {{"http://ex", kBoundary, kAnchorNone}, "http://example.com", true},
      {{"http://ex", kAnchorNone, kAnchorNone}, "http://example.com", true},
      {{"mple.com/", kAnchorNone, kBoundary}, "http://example.com", true},
      {{"mple.com/", kAnchorNone, kAnchorNone}, "http://example.com", true},
      {{"mple.com/", kSubdomain, kAnchorNone}, "http://example.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://hex.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://ex.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://hex.ex.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://hex.hex.com", false},

      // Note: "example.com" will be normalized into "example.com/".
      {{"example.com^", kSubdomain, kAnchorNone},
       "http://www.example.com",
       true},
      {{"http://*mpl", kBoundary, kAnchorNone}, "http://example.com", true},
      {{"mpl*com/", kAnchorNone, kBoundary}, "http://example.com", true},
      {{"example^com"}, "http://example.com", false},
      {{"example^com"}, "http://example/com", true},
      {{"example.com^"}, "http://example.com:8080", true},
      {{"http*.com/", kBoundary, kBoundary}, "http://example.com", true},
      {{"http*.org/", kBoundary, kBoundary}, "http://example.com", false},

      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path/bbb?k=v&p1=0&p2=1", false},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&p1=0&p2=1", true},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&k=v&p1=0&p2=1", true},
      {{"/path?*&p1=*&p2="},
       "http://ex.com/aaa/path?k=v&p1=0&p3=10&p2=1",
       true},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path&p1=0&p2=1", false},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&p2=0&p1=1", false},

      {{"abc*def*ghijk*xyz"},
       "http://example.com/abcdeffffghijkmmmxyzzz",
       true},
      {{"abc*cdef"}, "http://example.com/abcdef", false},

      {{"^^a^^"}, "http://ex.com/?a=/", true},
      {{"^^a^^"}, "http://ex.com/?a=/&b=0", true},
      {{"^^a^^"}, "http://ex.com/?a=x", false},
      // The last ^ matches the end of the url.
      {{"^^a^^"}, "http://ex.com/?a=", true},

      {{"ex.com^path^*k=v^"}, "http://ex.com/path/?k1=v1&ak=v&kk=vv", true},
      {{"ex.com^path^*k=v^"}, "http://ex.com/p/path/?k1=v1&ak=v&kk=vv", false},
      {{"a^a&a^a&"}, "http://ex.com/a/a/a/a/?a&a&a&a&a", true},

      {{"abc*def^"}, "http://ex.com/abc/a/ddef/", true},

      {{"https://example.com/"}, "http://example.com/", false},
      {{"example.com/", kSubdomain, kAnchorNone}, "http://example.com/", true},
      {{"examp", kSubdomain, kAnchorNone}, "http://example.com/", true},
      {{"xamp", kSubdomain, kAnchorNone}, "http://example.com/", false},
      {{"examp", kSubdomain, kAnchorNone}, "http://test.example.com/", true},
      {{"t.examp", kSubdomain, kAnchorNone}, "http://test.example.com/", false},
      {{"com^", kSubdomain, kAnchorNone}, "http://test.example.com/", true},
      {{"com^x", kSubdomain, kBoundary}, "http://a.com/x", true},
      {{"x.com", kSubdomain, kAnchorNone}, "http://ex.com/?url=x.com", false},
      {{"ex.com/", kSubdomain, kBoundary}, "http://ex.com/", true},
      {{"ex.com^", kSubdomain, kBoundary}, "http://ex.com/", true},
      {{"ex.co", kSubdomain, kBoundary}, "http://ex.com/", false},
      {{"ex.com", kSubdomain, kBoundary}, "http://rex.com.ex.com/", false},
      {{"ex.com/", kSubdomain, kBoundary}, "http://rex.com.ex.com/", true},
      {{"http", kSubdomain, kBoundary}, "http://http.com/", false},
      {{"http", kSubdomain, kAnchorNone}, "http://http.com/", true},
      {{"/example.com", kSubdomain, kBoundary}, "http://example.com/", false},
      {{"/example.com/", kSubdomain, kBoundary}, "http://example.com/", false},
      {{".", kSubdomain, kAnchorNone}, "http://a..com/", true},
      {{"^", kSubdomain, kAnchorNone}, "http://a..com/", false},
      {{".", kSubdomain, kAnchorNone}, "http://a.com./", false},
      {{"^", kSubdomain, kAnchorNone}, "http://a.com./", true},
      {{".", kSubdomain, kAnchorNone}, "http://a.com../", true},
      {{"^", kSubdomain, kAnchorNone}, "http://a.com../", true},
      {{"/path", kSubdomain, kAnchorNone}, "http://a.com./path/to/x", true},
      {{"^path", kSubdomain, kAnchorNone}, "http://a.com./path/to/x", true},
      {{"/path", kSubdomain, kBoundary}, "http://a.com./path", true},
      {{"^path", kSubdomain, kBoundary}, "http://a.com./path", true},
      {{"path", kSubdomain, kBoundary}, "http://a.com./path", false},
      // Case-sensitivity tests.
      {{"path", proto::URL_PATTERN_TYPE_SUBSTRING, kDonotMatchCase},
       "http://a.com/PaTh",
       true},
      {{"path", proto::URL_PATTERN_TYPE_SUBSTRING, kMatchCase},
       "http://a.com/PaTh",
       false},
      {{"path", proto::URL_PATTERN_TYPE_SUBSTRING, kDonotMatchCase},
       "http://a.com/path",
       true},
      {{"path", proto::URL_PATTERN_TYPE_SUBSTRING, kMatchCase},
       "http://a.com/path",
       true},
      {{"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kMatchCase},
       "http://a.com/abcxAdef/vo",
       true},
      {{"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kMatchCase},
       "http://a.com/aBcxAdeF/vo",
       false},
      {{"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kDonotMatchCase},
       "http://a.com/aBcxAdeF/vo",
       true},
      {{"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kDonotMatchCase},
       "http://a.com/abcxAdef/vo",
       true},
      {{"abc^", kAnchorNone, kAnchorNone}, "https://xyz.com/abc/123", true},
      {{"abc^", kAnchorNone, kAnchorNone}, "https://xyz.com/abc", true},
      {{"abc^", kAnchorNone, kAnchorNone}, "https://abc.com", false},
      {{"abc^", kAnchorNone, kBoundary}, "https://xyz.com/abc/", true},
      {{"abc^", kAnchorNone, kBoundary}, "https://xyz.com/abc", true},
      {{"abc^", kAnchorNone, kBoundary}, "https://xyz.com/abc/123", false},
      {{"http://abc.com/x^", kBoundary, kAnchorNone}, "http://abc.com/x", true},
      {{"http://abc.com/x^", kBoundary, kAnchorNone},
       "http://abc.com/x/",
       true},
      {{"http://abc.com/x^", kBoundary, kAnchorNone},
       "http://abc.com/x/123",
       true},
      {{"http://abc.com/x^", kBoundary, kBoundary}, "http://abc.com/x", true},
      {{"http://abc.com/x^", kBoundary, kBoundary}, "http://abc.com/x/", true},
      {{"http://abc.com/x^", kBoundary, kBoundary},
       "http://abc.com/x/123",
       false},
      {{"abc.com^", kSubdomain, kAnchorNone}, "http://xyz.abc.com/123", true},
      {{"abc.com^", kSubdomain, kAnchorNone}, "http://xyz.abc.com", true},
      {{"abc.com^", kSubdomain, kAnchorNone},
       "http://abc.com.xyz.com?q=abc.com",
       false},
      {{"abc.com^", kSubdomain, kBoundary}, "http://xyz.abc.com/123", false},
      {{"abc.com^", kSubdomain, kBoundary}, "http://xyz.abc.com", true},
      {{"abc.com^", kSubdomain, kBoundary},
       "http://abc.com.xyz.com?q=abc.com/",
       false},
      {{"abc*^", kAnchorNone, kAnchorNone}, "https://abc.com", true},
      {{"abc*^", kAnchorNone, kAnchorNone}, "https://abc.com?q=123", true},
      {{"abc*^", kAnchorNone, kBoundary}, "https://abc.com", true},
      {{"abc*^", kAnchorNone, kBoundary}, "https://abc.com?q=123", true},
      {{"abc*", kAnchorNone, kBoundary}, "https://a.com/abcxyz", true},
      {{"*google.com", kBoundary, kAnchorNone}, "https://www.google.com", true},
      {{"*", kBoundary, kBoundary}, "https://example.com", true},
      {{"", kBoundary, kBoundary}, "https://example.com", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.url_pattern
                                    << "; URL: " << GURL(test_case.url));

    GURL url(test_case.url);
    const bool is_match =
        test_case.url_pattern.MatchesUrl(UrlPattern::UrlInfo(url));
    EXPECT_EQ(test_case.expect_match, is_match);
  }
}

}  // namespace url_pattern_index
