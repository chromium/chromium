// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/url_utils.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history {

namespace {

TEST(HistoryUrlUtilsTest, CanonicalURLStringCompare) {
  // Comprehensive test by comparing each pair in sorted list. O(n^2).
  const char* sorted_list[] = {
    "http://www.gogle.com/redirects_to_google",
    "http://www.google.com",
    "http://www.google.com/",
    "http://www.google.com/?q",
    "http://www.google.com/A",
    "http://www.google.com/index.html",
    "http://www.google.com/test",
    "http://www.google.com/test?query",
    "http://www.google.com/test?r=3",
    "http://www.google.com/test#hash",
    "http://www.google.com/test/?query",
    "http://www.google.com/test/#hash",
    "http://www.google.com/test/zzzzz",
    "http://www.google.com/test$dollar",
    "http://www.google.com/test%E9%9B%80",
    "http://www.google.com/test-case",
    "http://www.google.com:80/",
    "https://www.google.com",
  };
  for (size_t i = 0; i < std::size(sorted_list); ++i) {
    EXPECT_FALSE(CanonicalURLStringCompare(sorted_list[i], sorted_list[i]))
        << " for \"" << sorted_list[i] << "\" < \"" << sorted_list[i] << "\"";
    // Every disjoint pair-wise comparison.
    for (size_t j = i + 1; j < std::size(sorted_list); ++j) {
      EXPECT_TRUE(CanonicalURLStringCompare(sorted_list[i], sorted_list[j]))
          << " for \"" << sorted_list[i] << "\" < \"" << sorted_list[j] << "\"";
      EXPECT_FALSE(CanonicalURLStringCompare(sorted_list[j], sorted_list[i]))
          << " for \"" << sorted_list[j] << "\" < \"" << sorted_list[i] << "\"";
    }
  }
}

TEST(HistoryUrlUtilsTest, HaveSameSchemeHostAndPort) {
  struct {
    const char* s1;
    const char* s2;
  } true_cases[] = {
    {"http://www.google.com", "http://www.google.com"},
    {"http://www.google.com/a/b", "http://www.google.com/a/b"},
    {"http://www.google.com?test=3", "http://www.google.com/"},
    {"http://www.google.com/#hash", "http://www.google.com/?q"},
    {"http://www.google.com/", "http://www.google.com/test/with/dir/"},
    {"http://www.google.com:360", "http://www.google.com:360/?q=1234"},
    {"http://www.google.com:80", "http://www.google.com/gurl/is/smart"},
    {"http://www.google.com/test", "http://www.google.com/test/with/dir/"},
    {"http://www.google.com/test?", "http://www.google.com/test/with/dir/"},
  };
  for (size_t i = 0; i < std::size(true_cases); ++i) {
    EXPECT_TRUE(HaveSameSchemeHostAndPort(GURL(true_cases[i].s1),
                               GURL(true_cases[i].s2)))
        << " for true_cases[" << i << "]";
  }
  struct {
    const char* s1;
    const char* s2;
  } false_cases[] = {
    {"http://www.google.co", "http://www.google.com"},
    {"http://google.com", "http://www.google.com"},
    {"http://www.google.com", "https://www.google.com"},
    {"http://www.google.com/path", "http://www.google.com:137/path"},
    {"http://www.google.com/same/dir", "http://www.youtube.com/same/dir"},
  };
  for (size_t i = 0; i < std::size(false_cases); ++i) {
    EXPECT_FALSE(HaveSameSchemeHostAndPort(GURL(false_cases[i].s1),
                                GURL(false_cases[i].s2)))
        << " for false_cases[" << i << "]";
  }
}

TEST(HistoryUrlUtilsTest, IsPathPrefix) {
  struct {
    const char* p1;
    const char* p2;
  } true_cases[] = {
    {"", ""},
    {"", "/"},
    {"/", "/"},
    {"/a/b", "/a/b"},
    {"/", "/test/with/dir/"},
    {"/test", "/test/with/dir/"},
    {"/test/", "/test/with/dir"},
  };
  for (size_t i = 0; i < std::size(true_cases); ++i) {
    EXPECT_TRUE(IsPathPrefix(true_cases[i].p1, true_cases[i].p2))
        << " for true_cases[" << i << "]";
  }
  struct {
    const char* p1;
    const char* p2;
  } false_cases[] = {
    {"/test", ""},
    {"/", ""},  // Arguable.
    {"/a/b/", "/a/b"},  // Arguable.
    {"/te", "/test"},
    {"/test", "/test-bed"},
    {"/test-", "/test"},
  };
  for (size_t i = 0; i < std::size(false_cases); ++i) {
    EXPECT_FALSE(IsPathPrefix(false_cases[i].p1, false_cases[i].p2))
        << " for false_cases[" << i << "]";
  }
}

TEST(HistoryUrlUtilsTest, ToggleHTTPAndHTTPS) {
  EXPECT_EQ(GURL("http://www.google.com/test?q#r"),
            ToggleHTTPAndHTTPS(GURL("https://www.google.com/test?q#r")));
  EXPECT_EQ(GURL("https://www.google.com:137/"),
            ToggleHTTPAndHTTPS(GURL("http://www.google.com:137/")));
  EXPECT_EQ(GURL(), ToggleHTTPAndHTTPS(GURL("ftp://www.google.com/")));
}

TEST(HistoryUrlUtilsTest, HostForTopHosts) {
  EXPECT_EQ("foo.com", HostForTopHosts(GURL("https://foo.com/bar")));
  EXPECT_EQ("foo.com", HostForTopHosts(GURL("http://foo.com:999/bar")));
  EXPECT_EQ("foo.com", HostForTopHosts(GURL("http://www.foo.com/bar")));
  EXPECT_EQ("foo.com", HostForTopHosts(GURL("HtTP://WWw.FoO.cOM/BAR")));
}

}  // namespace

}  // namespace history
