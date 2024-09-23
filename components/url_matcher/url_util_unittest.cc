// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_features.h"

namespace url_matcher {
namespace util {

namespace {

GURL GetEmbeddedURL(const std::string& url) {
  return url_matcher::util::GetEmbeddedURL(GURL(url));
}

// Parameters for the FilterToComponents test.
struct FilterTestParams {
 public:
  FilterTestParams(const std::string& filter,
                   const std::string& scheme,
                   const std::string& host,
                   bool match_subdomains,
                   uint16_t port,
                   const std::string& path)
      : filter_(filter),
        scheme_(scheme),
        host_(host),
        match_subdomains_(match_subdomains),
        port_(port),
        path_(path) {}

  FilterTestParams(const FilterTestParams& params)
      : filter_(params.filter_),
        scheme_(params.scheme_),
        host_(params.host_),
        match_subdomains_(params.match_subdomains_),
        port_(params.port_),
        path_(params.path_) {}

  const FilterTestParams& operator=(const FilterTestParams& params) {
    filter_ = params.filter_;
    scheme_ = params.scheme_;
    host_ = params.host_;
    match_subdomains_ = params.match_subdomains_;
    port_ = params.port_;
    path_ = params.path_;
    return *this;
  }

  const std::string& filter() const { return filter_; }
  const std::string& scheme() const { return scheme_; }
  const std::string& host() const { return host_; }
  bool match_subdomains() const { return match_subdomains_; }
  uint16_t port() const { return port_; }
  const std::string& path() const { return path_; }

 private:
  std::string filter_;
  std::string scheme_;
  std::string host_;
  bool match_subdomains_;
  uint16_t port_;
  std::string path_;
};

// Prints better debug information for Valgrind. Without this function, a
// generic one will print the raw bytes in FilterTestParams, which due to some
// likely padding will access uninitialized memory.
void PrintTo(const FilterTestParams& params, std::ostream* os) {
  *os << params.filter();
}

bool MatchFilters(const std::vector<std::string>& patterns,
                  const std::string& url) {
  // Add the pattern to the matcher.
  URLMatcher matcher;
  base::Value::List list;
  for (const auto& pattern : patterns)
    list.Append(pattern);
  AddAllowFilters(&matcher, list);
  return !matcher.MatchURL(GURL(url)).empty();
}

class FilterToComponentsTest : public testing::TestWithParam<FilterTestParams> {
 public:
  FilterToComponentsTest() = default;
  FilterToComponentsTest(const FilterToComponentsTest&) = delete;
  FilterToComponentsTest& operator=(const FilterToComponentsTest&) = delete;
};

class OnlyWildcardTest
    : public testing::TestWithParam<std::tuple<std::string /* scheme */,
                                               std::string /* opt_host */,
                                               std::string /* port */,
                                               std::string /* path */,
                                               std::string /* query*/>> {
 public:
  OnlyWildcardTest() = default;
  OnlyWildcardTest(const OnlyWildcardTest&) = delete;
  OnlyWildcardTest& operator=(const OnlyWildcardTest&) = delete;
};

}  // namespace

TEST(URLUtilTest, Normalize) {
  // Username is cleared.
  EXPECT_EQ(Normalize(GURL("http://dino@example/foo")),
            GURL("http://example/foo"));

  // Username and password are cleared.
  EXPECT_EQ(Normalize(GURL("http://dino:hunter2@example/")),
            GURL("http://example/"));

  // Query string is cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com/foo?widgetId=42")),
            GURL("http://example.com/foo"));
  EXPECT_EQ(Normalize(GURL("https://example.com/?widgetId=42&frobinate=true")),
            GURL("https://example.com/"));

  // Ref is cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com/foo#widgetSection")),
            GURL("http://example.com/foo"));

  // Port is NOT cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com:443/")),
            GURL("http://example.com:443/"));

  // All together now.
  EXPECT_EQ(
      Normalize(GURL("https://dino:hunter2@example.com:443/foo?widgetId=42")),
      GURL("https://example.com:443/foo"));
}

TEST(URLUtilTest, GetEmbeddedURLAmpCache) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/example.com"));
  // "s/" means "use https".
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/s/example.com"));
  // With path and query. Fragment is not extracted.
  EXPECT_EQ(GURL("https://example.com/path/to/file.html?q=asdf"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/s/example.com/path/to/"
                           "file.html?q=asdf#baz"));
  // Publish subdomain can be included but doesn't affect embedded URL.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://example-com.cdn.ampproject.org/c/example.com"));
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://example-org.cdn.ampproject.org/c/example.com"));

  // Different host is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.ampproject.org/c/example.com"));
  // Different TLD is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://cdn.ampproject.com/c/example.com"));
  // Content type ("c/") is missing.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://cdn.ampproject.org/example.com"));
  // Content type is mis-formatted, must be a single character.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://cdn.ampproject.org/cd/example.com"));
}

TEST(URLUtilTest, GetEmbeddedURLGoogleAmpViewer) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.com/amp/example.com"));
  // "s/" means "use https".
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://www.google.com/amp/s/example.com"));
  // Different Google TLDs are supported.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.de/amp/example.com"));
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.co.uk/amp/example.com"));
  // With path.
  EXPECT_EQ(GURL("http://example.com/path"),
            GetEmbeddedURL("https://www.google.com/amp/example.com/path"));
  // Query is *not* part of the embedded URL.
  EXPECT_EQ(
      GURL("http://example.com/path"),
      GetEmbeddedURL("https://www.google.com/amp/example.com/path?q=baz"));
  // Query and fragment in percent-encoded form *are* part of the embedded URL.
  EXPECT_EQ(
      GURL("http://example.com/path?q=foo#bar"),
      GetEmbeddedURL(
          "https://www.google.com/amp/example.com/path%3fq=foo%23bar?q=baz"));
  // "/" may also be percent-encoded.
  EXPECT_EQ(GURL("http://example.com/path?q=foo#bar"),
            GetEmbeddedURL("https://www.google.com/amp/"
                           "example.com%2fpath%3fq=foo%23bar?q=baz"));

  // Missing "amp/".
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.google.com/example.com"));
  // Path component before the "amp/".
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://www.google.com/foo/amp/example.com"));
  // Different host.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.other.com/amp/example.com"));
  // Different subdomain.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://mail.google.com/amp/example.com"));
  // Invalid TLD.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.google.nope/amp/example.com"));

  // Valid TLD that is not considered safe to display to the user by
  // UnescapeURLComponent(). Note that when UTF-8 characters appear in a domain
  // name, as is the case here, they're replaced by equivalent punycode by the
  // GURL constructor.
  EXPECT_EQ(GURL("http://www.xn--iv8h.com/"),
            GetEmbeddedURL("https://www.google.com/amp/www.%F0%9F%94%8F.com/"));
  // Invalid UTF-8 characters.
  EXPECT_EQ(GURL("http://example.com/%81%82%83"),
            GetEmbeddedURL("https://www.google.com/amp/example.com/%81%82%83"));
}

TEST(URLUtilTest, GetEmbeddedURLGoogleWebCache) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:ABCDEFGHI-JK:example.com/"));
  // With search query.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://webcache.googleusercontent.com/"
                     "search?q=cache:ABCDEFGHI-JK:example.com/+search_query"));
  // Without fingerprint.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:example.com/"));
  // With search query, without fingerprint.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:example.com/+search_query"));
  // Query params other than "q=" don't matter.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?a=b&q=cache:example.com/&c=d"));
  // With scheme.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:http://example.com/"));
  // Preserve https.
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:https://example.com/"));

  // Wrong host.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.googleusercontent.com/"
                                   "search?q=cache:example.com/"));
  // Wrong path.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "path?q=cache:example.com/"));
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "path/search?q=cache:example.com/"));
  // Missing "cache:".
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=example.com"));
  // Wrong fingerprint.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=cache:123:example.com/"));
  // Wrong query param.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?a=cache:example.com/"));
  // Invalid scheme.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=cache:abc://example.com/"));
}

TEST(URLUtilTest, GetEmbeddedURLTranslate) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://translate.google.com/path?u=example.com"));
  // Different TLD.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://translate.google.de/path?u=example.com"));
  // Alternate base URL.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL(
                "https://translate.googleusercontent.com/path?u=example.com"));
  // With scheme.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://translate.google.com/path?u=http://example.com"));
  // With https scheme.
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL(
                "https://translate.google.com/path?u=https://example.com"));
  // With other parameters.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL(
          "https://translate.google.com/path?a=asdf&u=example.com&b=fdsa"));

  // Different subdomain is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://translate.foo.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://translate.www.google.com/path?u=example.com"));
  EXPECT_EQ(
      GURL(),
      GetEmbeddedURL("https://translate.google.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://foo.translate.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://translate2.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(),
            GetEmbeddedURL(
                "https://translate2.googleusercontent.com/path?u=example.com"));
  // Different TLD is not supported for googleusercontent.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL(
                "https://translate.googleusercontent.de/path?u=example.com"));
  // Query parameter ("u=...") is missing.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://translate.google.com/path?t=example.com"));
}

INSTANTIATE_TEST_SUITE_P(URLUtilTest,
                         OnlyWildcardTest,
                         testing::Combine(testing::Values("", "https://"),
                                          testing::Values("", "dev."),
                                          testing::Values("", ":1234"),
                                          testing::Values("", "/path"),
                                          testing::Values("", "?query")));

TEST_P(OnlyWildcardTest, OnlyWildcard) {
  // Check wildcard filter works on any permutations of format
  // [scheme://][.]host[:port][/path][@query]
  const std::string scheme = std::get<0>(GetParam());
  const std::string opt_host = std::get<1>(GetParam());
  const std::string port = std::get<2>(GetParam());
  const std::string path = std::get<3>(GetParam());
  const std::string query = std::get<4>(GetParam());
  const std::string url =
      scheme + opt_host + "google.com" + port + path + query;
  EXPECT_TRUE(MatchFilters({"*"}, url));
}

// Non-special URLs behavior is affected by the
// StandardCompliantNonSpecialSchemeURLParsing feature.
// See https://crbug.com/40063064 for details.
class URLUtilParamTest : public ::testing::TestWithParam<bool> {
 public:
  URLUtilParamTest()
  : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        url::kStandardCompliantNonSpecialSchemeURLParsing,
        use_standard_compliant_non_special_scheme_url_parsing_);
  }

   protected:
    bool use_standard_compliant_non_special_scheme_url_parsing_;

   private:
    base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(URLUtilParamTest, SingleFilter) {
  // Match domain and all subdomains, for any filtered scheme.
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://google.com"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://google.com/"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://google.com/whatever"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "https://google.com/"));
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // When the feature is enabled, the host part in non-special URLs can be
    // recognized.
    EXPECT_TRUE(MatchFilters({"google.com"}, "bogus://google.com/"));
  } else {
    EXPECT_FALSE(MatchFilters({"google.com"}, "bogus://google.com/"));
  }
  EXPECT_FALSE(MatchFilters({"google.com"}, "http://notgoogle.com/"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://mail.google.com"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://x.mail.google.com"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "https://x.mail.google.com/"));
  EXPECT_TRUE(MatchFilters({"google.com"}, "http://x.y.google.com/a/b"));
  EXPECT_FALSE(MatchFilters({"google.com"}, "http://youtube.com/"));

  // Filter only http, ftp and ws schemes.
  EXPECT_TRUE(MatchFilters({"http://secure.com"}, "http://secure.com"));
  EXPECT_TRUE(
      MatchFilters({"http://secure.com"}, "http://secure.com/whatever"));
  EXPECT_TRUE(MatchFilters({"ftp://secure.com"}, "ftp://secure.com/"));
  EXPECT_TRUE(MatchFilters({"ws://secure.com"}, "ws://secure.com"));
  EXPECT_FALSE(MatchFilters({"http://secure.com"}, "https://secure.com/"));
  EXPECT_FALSE(MatchFilters({"ws://secure.com"}, "wss://secure.com"));
  EXPECT_TRUE(MatchFilters({"http://secure.com"}, "http://www.secure.com"));
  EXPECT_FALSE(MatchFilters({"http://secure.com"}, "https://www.secure.com"));
  EXPECT_FALSE(MatchFilters({"ws://secure.com"}, "wss://www.secure.com"));

  // Filter only a certain path prefix.
  EXPECT_TRUE(MatchFilters({"path.to/ruin"}, "http://path.to/ruin"));
  EXPECT_TRUE(MatchFilters({"path.to/ruin"}, "https://path.to/ruin"));
  EXPECT_TRUE(MatchFilters({"path.to/ruin"}, "http://path.to/ruins"));
  EXPECT_TRUE(MatchFilters({"path.to/ruin"}, "http://path.to/ruin/signup"));
  EXPECT_TRUE(MatchFilters({"path.to/ruin"}, "http://www.path.to/ruin"));
  EXPECT_FALSE(MatchFilters({"path.to/ruin"}, "http://path.to/fortune"));

  // Filter only a certain path prefix and scheme.
  EXPECT_TRUE(
      MatchFilters({"https://s.aaa.com/path"}, "https://s.aaa.com/path"));
  EXPECT_TRUE(
      MatchFilters({"https://s.aaa.com/path"}, "https://s.aaa.com/path/bbb"));
  EXPECT_FALSE(
      MatchFilters({"https://s.aaa.com/path"}, "http://s.aaa.com/path"));
  EXPECT_FALSE(
      MatchFilters({"https://s.aaa.com/path"}, "https://aaa.com/path"));
  EXPECT_FALSE(
      MatchFilters({"https://s.aaa.com/path"}, "https://x.aaa.com/path"));
  EXPECT_FALSE(
      MatchFilters({"https://s.aaa.com/path"}, "https://s.aaa.com/bbb"));
  EXPECT_FALSE(MatchFilters({"https://s.aaa.com/path"}, "https://s.aaa.com/"));

  // Filter only ws and wss schemes.
  EXPECT_TRUE(MatchFilters({"ws://ws.aaa.com"}, "ws://ws.aaa.com"));
  EXPECT_TRUE(MatchFilters({"wss://ws.aaa.com"}, "wss://ws.aaa.com"));
  EXPECT_FALSE(MatchFilters({"ws://ws.aaa.com"}, "http://ws.aaa.com"));
  EXPECT_FALSE(MatchFilters({"ws://ws.aaa.com"}, "https://ws.aaa.com"));
  EXPECT_FALSE(MatchFilters({"ws://ws.aaa.com"}, "ftp://ws.aaa.com"));

  // Match an ip address.
  EXPECT_TRUE(MatchFilters({"123.123.123.123"}, "http://123.123.123.123/"));
  EXPECT_FALSE(MatchFilters({"123.123.123.123"}, "http://123.123.123.124/"));

  // Open an exception.
  EXPECT_FALSE(MatchFilters({"plus.google.com"}, "http://google.com/"));
  EXPECT_FALSE(MatchFilters({"plus.google.com"}, "http://www.google.com/"));
  EXPECT_TRUE(MatchFilters({"plus.google.com"}, "http://plus.google.com/"));

  // Match exactly "google.com", only for http.
  EXPECT_TRUE(MatchFilters({"http://.google.com"}, "http://google.com/"));
  EXPECT_FALSE(MatchFilters({"http://.google.com"}, "https://google.com/"));
  EXPECT_FALSE(MatchFilters({"http://.google.com"}, "http://www.google.com/"));
}

TEST(URLUtilTest, MultipleFilters) {
  // Test exceptions to path prefixes, and most specific matches.
  std::vector<std::string> patterns = {"s.xxx.com/a/b",
                                       "https://s.xxx.com/a/b/c/d"};
  EXPECT_FALSE(MatchFilters(patterns, "http://s.xxx.com/a"));
  EXPECT_FALSE(MatchFilters(patterns, "http://s.xxx.com/a/x"));
  EXPECT_FALSE(MatchFilters(patterns, "https://s.xxx.com/a/x"));
  EXPECT_TRUE(MatchFilters(patterns, "http://s.xxx.com/a/b"));
  EXPECT_TRUE(MatchFilters(patterns, "https://s.xxx.com/a/b"));
  EXPECT_TRUE(MatchFilters(patterns, "http://s.xxx.com/a/b/x"));
  EXPECT_TRUE(MatchFilters(patterns, "http://s.xxx.com/a/b/c"));
  EXPECT_TRUE(MatchFilters(patterns, "https://s.xxx.com/a/b/c"));
  EXPECT_TRUE(MatchFilters(patterns, "https://s.xxx.com/a/b/c/x"));
  EXPECT_TRUE(MatchFilters(patterns, "https://s.xxx.com/a/b/c/d"));
  EXPECT_TRUE(MatchFilters(patterns, "http://s.xxx.com/a/b/c/d"));
  EXPECT_TRUE(MatchFilters(patterns, "https://s.xxx.com/a/b/c/d/x"));
  EXPECT_TRUE(MatchFilters(patterns, "http://s.xxx.com/a/b/c/d/x"));
  EXPECT_FALSE(MatchFilters(patterns, "http://xxx.com/a"));
  EXPECT_FALSE(MatchFilters(patterns, "http://xxx.com/a/b"));

  // Match queries.
  std::vector<std::string> queries = {"*?q=1234", "*?q=5678", "*?a=1&b=2",
                                      "youtube.com?foo=baz",
                                      "youtube.com?foo=bar*"};
  EXPECT_TRUE(MatchFilters(queries, "http://google.com?q=1234"));
  EXPECT_TRUE(MatchFilters(queries, "http://google.com?q=5678"));
  EXPECT_TRUE(MatchFilters(queries, "http://google.com?a=1&b=2"));
  EXPECT_TRUE(MatchFilters(queries, "http://google.com?b=2&a=1"));
  EXPECT_TRUE(MatchFilters(queries, "http://google.com?a=1&b=4&q=1234"));
  EXPECT_TRUE(MatchFilters(queries, "http://youtube.com?foo=baz"));
  EXPECT_TRUE(MatchFilters(queries, "http://youtube.com?foo=barbaz"));
  EXPECT_TRUE(MatchFilters(queries, "http://youtube.com?a=1&foo=barbaz"));
  EXPECT_FALSE(MatchFilters(queries, "http://google.com?r=1234"));
  EXPECT_FALSE(MatchFilters(queries, "http://google.com?r=5678"));
  EXPECT_FALSE(MatchFilters(queries, "http://google.com?a=2&b=1"));
  EXPECT_FALSE(MatchFilters(queries, "http://google.com?b=1&a=2"));
  EXPECT_FALSE(MatchFilters(queries, "http://google.com?a=1&b=3"));
  EXPECT_FALSE(MatchFilters(queries, "http://youtube.com?foo=meh"));
  EXPECT_FALSE(MatchFilters(queries, "http://youtube.com?foo=bazbar"));
  EXPECT_FALSE(MatchFilters(queries, "http://youtube.com?foo=ba"));
}

TEST_P(URLUtilParamTest, BasicCoverage) {
  // Tests to cover the documentation from
  // http://www.chromium.org/administrators/url-blocklist-filter-format

  // [scheme://][.]host[:port][/path][@query]
  // Scheme can be http, https, ftp, chrome, etc. This field is optional, and
  // must be followed by '://'.
  EXPECT_TRUE(MatchFilters({"file://*"}, "file:///abc.txt"));
  EXPECT_TRUE(MatchFilters({"file:*"}, "file:///usr/local/boot.txt"));
  EXPECT_TRUE(MatchFilters(
      {"data:*"},
      "data:image/svg+xml,<svg xmlns=\"http://www.w3.org/2000/svg\"/>"));
  EXPECT_TRUE(MatchFilters({"https://*"}, "https:///abc.txt"));
  EXPECT_TRUE(MatchFilters({"ftp://*"}, "ftp://ftp.txt"));
  EXPECT_TRUE(MatchFilters({"chrome://*"}, "chrome:policy"));
  EXPECT_TRUE(MatchFilters({"noscheme"}, "http://noscheme"));
  // Filter custom schemes.
  EXPECT_TRUE(MatchFilters({"custom://*"}, "custom://example_app"));
  EXPECT_TRUE(MatchFilters({"custom:*"}, "custom:example2_app"));
  EXPECT_FALSE(MatchFilters({"custom://*"}, "customs://example_apps"));
  EXPECT_FALSE(MatchFilters({"custom://*"}, "cust*://example_ap"));
  EXPECT_FALSE(MatchFilters({"custom://*"}, "ecustom:example_app"));
  EXPECT_TRUE(MatchFilters({"custom://*"}, "custom:///abc.txt"));
  // Tests for custom scheme patterns that are not supported.
  EXPECT_FALSE(MatchFilters({"wrong://app"}, "wrong://app"));
  EXPECT_FALSE(MatchFilters({"wrong ://*"}, "wrong ://app"));
  EXPECT_FALSE(MatchFilters({" wrong:*"}, " wrong://app"));

  // Omitting the scheme matches most standard schemes.
  EXPECT_TRUE(MatchFilters({"example.com"}, "chrome:example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "chrome://example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "file://example.com/"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "ftp://example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "http://example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "https://example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "ws://example.com"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "wss://example.com"));

  // Some schemes are not matched when the scheme is omitted.
  EXPECT_FALSE(MatchFilters({"example.com"}, "about:example.com"));
  EXPECT_FALSE(MatchFilters({"example.com/*"}, "filesystem:///something"));
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // When the feature is enabled, the host part in non-special URLs can be
    // recognized.
    EXPECT_TRUE(MatchFilters({"example.com"}, "about://example.com"));
    EXPECT_TRUE(MatchFilters({"example.com"}, "custom://example.com"));
    EXPECT_TRUE(MatchFilters({"example"}, "custom://example"));
  } else {
    EXPECT_FALSE(MatchFilters({"example.com"}, "about://example.com"));
    EXPECT_FALSE(MatchFilters({"example.com"}, "custom://example.com"));
    EXPECT_FALSE(MatchFilters({"example"}, "custom://example"));
  }

  // An optional '.' (dot) can prefix the host field to disable subdomain
  // matching, see below for details.
  EXPECT_TRUE(MatchFilters({".example.com"}, "http://example.com/path"));
  EXPECT_FALSE(MatchFilters({".example.com"}, "http://mail.example.com/path"));
  EXPECT_TRUE(MatchFilters({"example.com"}, "http://mail.example.com/path"));
  EXPECT_TRUE(MatchFilters({"ftp://.ftp.file"}, "ftp://ftp.file"));
  EXPECT_FALSE(MatchFilters({"ftp://.ftp.file"}, "ftp://sub.ftp.file"));

  // The host field is required, and is a valid hostname or an IP address. It
  // can also take the special '*' value, see below for details.
  EXPECT_TRUE(MatchFilters({"*"}, "http://anything"));
  EXPECT_TRUE(MatchFilters({"*"}, "ftp://anything"));
  EXPECT_TRUE(MatchFilters({"*"}, "custom://anything"));
  EXPECT_TRUE(MatchFilters({"host"}, "http://host:8080"));
  EXPECT_FALSE(MatchFilters({"host"}, "file:///host"));
  EXPECT_TRUE(MatchFilters({"10.1.2.3"}, "http://10.1.2.3:8080/path"));
  // No host, will match nothing.
  EXPECT_FALSE(MatchFilters({":8080"}, "http://host:8080"));
  EXPECT_FALSE(MatchFilters({":8080"}, "http://:8080"));

  // An optional port can come after the host. It must be a valid port value
  // from 1 to 65535.
  EXPECT_TRUE(MatchFilters({"host:8080"}, "http://host:8080/path"));
  EXPECT_TRUE(MatchFilters({"host:1"}, "http://host:1/path"));
  // Out of range port.
  EXPECT_FALSE(MatchFilters({"host:65536"}, "http://host:65536/path"));
  // Star is not allowed in port numbers.
  EXPECT_FALSE(MatchFilters({"example.com:*"}, "http://example.com"));
  EXPECT_FALSE(MatchFilters({"example.com:*"}, "http://example.com:8888"));

  // An optional path can come after port.
  EXPECT_TRUE(MatchFilters({"host/path"}, "http://host:8080/path"));
  EXPECT_TRUE(MatchFilters({"host/path/path2"}, "http://host/path/path2"));
  EXPECT_TRUE(MatchFilters({"host/path"}, "http://host/path/path2"));

  // An optional query can come in the end, which is a set of key-value and
  // key-only tokens delimited by '&'. The key-value tokens are separated
  // by '='. A query token can optionally end with a '*' to indicate prefix
  // match. Token order is ignored during matching.
  EXPECT_TRUE(MatchFilters({"host?q1=1&q2=2"}, "http://host?q2=2&q1=1"));
  EXPECT_FALSE(MatchFilters({"host?q1=1&q2=2"}, "http://host?q2=1&q1=2"));
  EXPECT_FALSE(MatchFilters({"host?q1=1&q2=2"}, "http://host?Q2=2&Q1=1"));
  EXPECT_TRUE(MatchFilters({"host?q1=1&q2=2"}, "http://host?q2=2&q1=1&q3=3"));
  EXPECT_TRUE(MatchFilters({"host?q1=1&q2=2*"}, "http://host?q2=21&q1=1&q3=3"));

  // user:pass fields can be included but will be ignored
  // (e.g. http://user:pass@ftp.example.com/pub/bigfile.iso).
  EXPECT_TRUE(
      MatchFilters({"host.com/path"}, "http://user:pass@host.com:8080/path"));
  EXPECT_TRUE(MatchFilters({"ftp://host.com/path"},
                           "ftp://user:pass@host.com:8080/path"));

  // Case sensitivity.
  // Scheme is case insensitive.
  EXPECT_TRUE(MatchFilters({"suPPort://*"}, "support:example"));
  EXPECT_TRUE(MatchFilters({"FILE://*"}, "file:example"));
  EXPECT_TRUE(MatchFilters({"FILE://*"}, "FILE://example"));
  EXPECT_TRUE(MatchFilters({"FtP:*"}, "ftp://example"));
  EXPECT_TRUE(MatchFilters({"http://example.com"}, "HTTP://example.com"));
  EXPECT_TRUE(MatchFilters({"HTTP://example.com"}, "http://example.com"));
  // Host is case insensitive.
  EXPECT_TRUE(MatchFilters({"http://EXAMPLE.COM"}, "http://example.com"));
  EXPECT_TRUE(MatchFilters({"Example.com"}, "http://examplE.com/Path?Query=1"));
  // Path is case sensitive.
  EXPECT_FALSE(MatchFilters({"example.com/Path"}, "http://example.com/path"));
  EXPECT_TRUE(MatchFilters({"http://example.com/aB"}, "http://example.com/aB"));
  EXPECT_FALSE(
      MatchFilters({"http://example.com/aB"}, "http://example.com/Ab"));
  EXPECT_FALSE(
      MatchFilters({"http://example.com/aB"}, "http://example.com/ab"));
  EXPECT_FALSE(
      MatchFilters({"http://example.com/aB"}, "http://example.com/AB"));
  // Query is case sensitive.
  EXPECT_FALSE(MatchFilters({"host/path?Query=1"}, "http://host/path?query=1"));
}

INSTANTIATE_TEST_SUITE_P(All, URLUtilParamTest, ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    URLUtilTest,
    FilterToComponentsTest,
    testing::Values(
        FilterTestParams("google.com",
                         std::string(),
                         ".google.com",
                         true,
                         0u,
                         std::string()),
        FilterTestParams(".google.com",
                         std::string(),
                         "google.com",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("http://google.com",
                         "http",
                         ".google.com",
                         true,
                         0u,
                         std::string()),
        FilterTestParams("google.com/",
                         std::string(),
                         ".google.com",
                         true,
                         0u,
                         "/"),
        FilterTestParams("http://google.com:8080/whatever",
                         "http",
                         ".google.com",
                         true,
                         8080u,
                         "/whatever"),
        FilterTestParams("http://user:pass@google.com:8080/whatever",
                         "http",
                         ".google.com",
                         true,
                         8080u,
                         "/whatever"),
        FilterTestParams("123.123.123.123",
                         std::string(),
                         "123.123.123.123",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("https://123.123.123.123",
                         "https",
                         "123.123.123.123",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("123.123.123.123/",
                         std::string(),
                         "123.123.123.123",
                         false,
                         0u,
                         "/"),
        FilterTestParams("http://123.123.123.123:123/whatever",
                         "http",
                         "123.123.123.123",
                         false,
                         123u,
                         "/whatever"),
        FilterTestParams("*",
                         std::string(),
                         std::string(),
                         true,
                         0u,
                         std::string()),
        FilterTestParams("ftp://*",
                         "ftp",
                         std::string(),
                         true,
                         0u,
                         std::string()),
        FilterTestParams("http://*/whatever",
                         "http",
                         std::string(),
                         true,
                         0u,
                         "/whatever"),
        FilterTestParams("data:image/png",
                         "data",
                         std::string(),
                         true,
                         0u,
                         "image/png")));

TEST_P(FilterToComponentsTest, FilterToComponents) {
  std::string scheme;
  std::string host;
  bool match_subdomains = true;
  uint16_t port = 42;
  std::string path;
  std::string query;

  FilterToComponents(GetParam().filter(), &scheme, &host, &match_subdomains,
                     &port, &path, &query);
  EXPECT_EQ(GetParam().scheme(), scheme);
  EXPECT_EQ(GetParam().host(), host);
  EXPECT_EQ(GetParam().match_subdomains(), match_subdomains);
  EXPECT_EQ(GetParam().port(), port);
  EXPECT_EQ(GetParam().path(), path);
}

}  // namespace util
}  // namespace url_matcher
