// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kPackageName[] = "default.package.name";

class IntentFilterBuilder {
 public:
  IntentFilterBuilder() = default;

  IntentFilterBuilder& authority(const std::string& host) {
    return authority(host, -1);
  }

  IntentFilterBuilder& authority(const std::string& host, int port) {
    authorities_.emplace_back(host, port);
    return *this;
  }

  IntentFilterBuilder& path(const std::string& path,
                            const mojom::PatternType& type) {
    paths_.emplace_back(path, type);
    return *this;
  }

  operator IntentFilter() {
    return IntentFilter(kPackageName, std::move(authorities_),
                        std::move(paths_), std::vector<std::string>());
  }

 private:
  std::vector<IntentFilter::AuthorityEntry> authorities_;
  std::vector<IntentFilter::PatternMatcher> paths_;

  DISALLOW_COPY_AND_ASSIGN(IntentFilterBuilder);
};

}  // namespace

TEST(IntentFilterTest, TestAuthorityEntry_empty) {
  // Empty URL shouldn't match a filter with an authority.
  IntentFilter filter = IntentFilterBuilder()
      .authority("authority1");

  EXPECT_FALSE(filter.Match(GURL()));

  // Empty URL shouldn't match a filter with an authority and port.
  IntentFilter filter_port_100 = IntentFilterBuilder()
      .authority("authority1", 100);

  EXPECT_FALSE(filter_port_100.Match(GURL()));
}

TEST(IntentFilterTest, TestAuthorityEntry_simple) {
  // URL authority should match the filter authority.
  IntentFilter filter = IntentFilterBuilder()
      .authority("authority1");

  EXPECT_FALSE(filter.Match(GURL("http://authority2")));
  EXPECT_FALSE(filter.Match(GURL("https://authority2")));

  EXPECT_TRUE(filter.Match(GURL("http://authority1")));
  EXPECT_TRUE(filter.Match(GURL("https://authority1")));
}

TEST(IntentFilterTest, TestNoAuthorityEntry_simple) {
  // An empty authority will act as a wildcard, so any http(s) URL will match.
  IntentFilter filter = IntentFilterBuilder();

  EXPECT_TRUE(filter.Match(GURL("http://validscheme1")));
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1/path")));
  EXPECT_TRUE(filter.Match(GURL("https://validscheme2")));
  EXPECT_TRUE(filter.Match(GURL("https://validscheme2/path")));

  EXPECT_FALSE(filter.Match(GURL("ftp://wedontsupportallschemes")));
  EXPECT_FALSE(filter.Match(GURL("ftp://wedontsupportallschemes/path")));
}

TEST(IntentFilterTest, TestAuthorityEntry_no_port) {
  // A filter with no port should accept matching authority URLs with any port.
  IntentFilter filter_no_port = IntentFilterBuilder()
      .authority("authority1");

  EXPECT_TRUE(filter_no_port.Match(GURL("http://authority1:0")));
  EXPECT_TRUE(filter_no_port.Match(GURL("https://authority1:0")));
  EXPECT_TRUE(filter_no_port.Match(GURL("http://authority1:22")));
  EXPECT_TRUE(filter_no_port.Match(GURL("https://authority1:22")));
  EXPECT_TRUE(filter_no_port.Match(GURL("http://authority1:1024")));
  EXPECT_TRUE(filter_no_port.Match(GURL("https://authority1:1024")));
  EXPECT_TRUE(filter_no_port.Match(GURL("http://authority1:65535")));
  EXPECT_TRUE(filter_no_port.Match(GURL("https://authority1:65535")));
}

TEST(IntentFilterTest, TestNoAuthorityEntry_no_port) {
  // A filter with no port and no authority is still considered a wildcard.
  IntentFilter filter = IntentFilterBuilder();

  EXPECT_TRUE(filter.Match(GURL("http://validscheme1:0")));
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1:0/path")));
  EXPECT_TRUE(filter.Match(GURL("https://validscheme2:420")));
  EXPECT_TRUE(filter.Match(GURL("https://validscheme2:420/path")));

  EXPECT_FALSE(filter.Match(GURL("custom-scheme://unvalidscheme:0")));
  EXPECT_FALSE(filter.Match(GURL("custom-scheme://unvalidscheme:0/path")));
}

TEST(IntentFilterTest, TestAuthorityEntry_with_port) {
  // A filter with a specified port should only match URLs with that port.
  IntentFilter filter_port_100 = IntentFilterBuilder()
      .authority("authority1", 100);

  EXPECT_FALSE(filter_port_100.Match(GURL("http://authority1")));
  EXPECT_FALSE(filter_port_100.Match(GURL("https://authority1")));
  EXPECT_FALSE(filter_port_100.Match(GURL("http://authority1:0")));
  EXPECT_FALSE(filter_port_100.Match(GURL("https://authority1:0")));
  EXPECT_FALSE(filter_port_100.Match(GURL("http://authority1:22")));
  EXPECT_FALSE(filter_port_100.Match(GURL("https://authority1:22")));
  EXPECT_FALSE(filter_port_100.Match(GURL("http://authority1:1024")));
  EXPECT_FALSE(filter_port_100.Match(GURL("https://authority1:1024")));
  EXPECT_FALSE(filter_port_100.Match(GURL("http://authority1:65535")));
  EXPECT_FALSE(filter_port_100.Match(GURL("https://authority1:65535")));

  EXPECT_TRUE(filter_port_100.Match(GURL("http://authority1:100")));
  EXPECT_TRUE(filter_port_100.Match(GURL("https://authority1:100")));
}

TEST(IntentFilterTest, TestAuthorityEntry_default_port) {
  // Intent filters with explicit default ports match URLs with or without
  // explicit ports.  This diverges from android's intent filter behaviour.  See
  // the IntentFilter::AuthorityEntry::match code for details.
  IntentFilter filter_default_port = IntentFilterBuilder()
      .authority("authority1", 80)
      .authority("authority1", 443);

  EXPECT_TRUE(filter_default_port.Match(GURL("http://authority1")));
  EXPECT_TRUE(filter_default_port.Match(GURL("https://authority1")));
  EXPECT_TRUE(filter_default_port.Match(GURL("http://authority1:80")));
  EXPECT_TRUE(filter_default_port.Match(GURL("https://authority1:443")));
}

TEST(IntentFilterTest, TestAuthorityEntry_multiple) {
  // A filter with multiple authorities should match URLs that match any of
  // those authorities.
  IntentFilter filter = IntentFilterBuilder()
      .authority("authority1", 100)
      .authority("authority2");

  EXPECT_FALSE(filter.Match(GURL("http://authority1")));
  EXPECT_FALSE(filter.Match(GURL("http://authority3")));

  EXPECT_TRUE(filter.Match(GURL("http://authority1:100")));
  EXPECT_TRUE(filter.Match(GURL("http://authority2")));
}

TEST(IntentFilterTest, TestAuthorityEntry_substring) {
  // Make sure substrings don't match in non-wildcard cases.
  IntentFilter filter = IntentFilterBuilder()
      .authority("authority1");

  EXPECT_FALSE(filter.Match(GURL("http://authority")));
  EXPECT_FALSE(filter.Match(GURL("http://authority12")));
}

TEST(IntentFilterTest, TestAuthorityEntry_wild) {
  // Make sure wildcards work
  IntentFilter filter = IntentFilterBuilder()
      .authority("*.authority1");

  EXPECT_FALSE(filter.Match(GURL("http://.authority")));
  EXPECT_FALSE(filter.Match(GURL("http://.authority12")));

  EXPECT_TRUE(filter.Match(GURL("http://.authority1")));
  EXPECT_TRUE(filter.Match(GURL("http://foo.authority1")));
  EXPECT_TRUE(filter.Match(GURL("http://bar.authority1")));
  EXPECT_TRUE(filter.Match(GURL("http://foo.bar.authority1")));
  EXPECT_TRUE(filter.Match(GURL("http://foo.authority1.authority1")));
}

TEST(IntentFilterTest, TestDataPath_literal) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/path1", mojom::PatternType::PATTERN_LITERAL);

  // Empty paths, prefix-, and substring-matches should fail.
  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/path")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/path12")));

  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1")));
}

TEST(IntentFilterTest, TestNoAuthorityDataPath_literal) {
  IntentFilter filter =
      IntentFilterBuilder().path("/path", mojom::PatternType::PATTERN_LITERAL);

  // A filter with no authority and a custom path must still match our URL.
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1")));
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1:0")));
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1:0/path")));
  EXPECT_TRUE(filter.Match(GURL("http://validscheme1:10/other/path")));
}

TEST(IntentFilterTest, TestDataPath_prefix) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/path1", mojom::PatternType::PATTERN_PREFIX);

  // Empty paths and substring-matches should fail.
  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/path")));

  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path12")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globSuffix) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/path1.*", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  // Empty paths and substring-matches should fail.
  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/path")));

  // Glob should match any substring including the empty susbstring.
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path11")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path112345")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1.")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1.....")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/path1path")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globInfix) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/a.*b", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  // Empty paths and substring-matches should fail.
  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a")));

  // Extra junk on the end should fail.
  EXPECT_FALSE(filter.Match(GURL("http://host.com/abc")));

  // Glob should match any substring including the empty susbstring.
  EXPECT_TRUE(filter.Match(GURL("http://host.com/ab")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a1b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a12345b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a.b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a.....b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/aab")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/aaaaab")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a/foo/b")));

  // TODO(kenobi): These cases don't work correctly.  However, the chrome-side
  // intent filter matching code needs to replicate the results of the
  // android-side pattern matcher, otherwise chrome will attempt to send URLs to
  // android that won't successfully match with any installed app.  See
  // b/30160040.
  // EXPECT_TRUE(filter.Match(GURL("http://host.com/abb")));
  // EXPECT_TRUE(filter.Match(GURL("http://host.com/abbbbb")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globOnly) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/.*", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  // Empty URLs should fail.
  EXPECT_FALSE(filter.Match(GURL()));

  // Any path should match.
  EXPECT_TRUE(filter.Match(GURL("http://host.com")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/aaa")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/aaa/bbb")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/.")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globSingleChar) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/a1*b", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a12b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/ab12b")));

  EXPECT_TRUE(filter.Match(GURL("http://host.com/ab")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a1b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a111b")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globEscapedChar) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/a\\.*b", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a1b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a111b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/abc")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a.bc")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a...bc")));

  EXPECT_TRUE(filter.Match(GURL("http://host.com/ab")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a.b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a...b")));
}

// Glob tests based loosely on android's CTS IntentFilterTest#testPaths.
TEST(IntentFilterTest, TestDataPath_globEscapedStar) {
  IntentFilter filter = IntentFilterBuilder()
      .authority("host.com")
      .path("/a.\\*b", mojom::PatternType::PATTERN_SIMPLE_GLOB);

  EXPECT_FALSE(filter.Match(GURL()));
  EXPECT_FALSE(filter.Match(GURL("http://host.com")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/ab")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a.b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a*b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a1b")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a.*bc")));
  EXPECT_FALSE(filter.Match(GURL("http://host.com/a1*bc")));

  EXPECT_TRUE(filter.Match(GURL("http://host.com/a.*b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a1*b")));
  EXPECT_TRUE(filter.Match(GURL("http://host.com/a2*b")));
}

}  // namespace arc
