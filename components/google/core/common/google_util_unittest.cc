// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/common/google_util.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "components/google/core/common/google_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using google_util::IsGoogleDomainUrl;

// Helpers --------------------------------------------------------------------

namespace {

constexpr const char* kValidSearchSchemes[] = {"http", "https"};

constexpr const char* kValidSearchQueryParams[] = {
    "q",
    "as_q"  // Advanced search uses "as_q" instead of "q" as the query param.
};

// These functions merely provide brevity in the callers.

bool IsHomePage(const std::string& url) {
  return google_util::IsGoogleHomePageUrl(GURL(url));
}

bool IsSearch(const std::string& url) {
  return google_util::IsGoogleSearchUrl(GURL(url));
}

bool StartsWithBaseURL(const std::string& url) {
  return google_util::StartsWithCommandLineGoogleBaseURL(GURL(url));
}

}  // namespace

// Actual tests ---------------------------------------------------------------

TEST(GoogleUtilTest, GoodHomePagesNonSecure) {
  // Valid home page hosts.
  EXPECT_TRUE(IsHomePage(google_util::kGoogleHomepageURL));
  EXPECT_TRUE(IsHomePage("http://google.com"));
  EXPECT_TRUE(IsHomePage("http://www.google.com"));
  EXPECT_TRUE(IsHomePage("http://www.google.ca"));
  EXPECT_TRUE(IsHomePage("http://www.google.co.uk"));
  EXPECT_TRUE(IsHomePage("http://www.google.com:80/"));

  // Only the paths /, /webhp, and /ig.* are valid.  Query parameters are
  // ignored.
  EXPECT_TRUE(IsHomePage("http://www.google.com/"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/webhp"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/webhp?rlz=TEST"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/ig"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/ig/foo"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/ig?rlz=TEST"));
  EXPECT_TRUE(IsHomePage("http://www.google.com/ig/foo?rlz=TEST"));

  // Accepted subdomains.
  EXPECT_TRUE(IsHomePage("http://ipv4.google.com/"));
  EXPECT_TRUE(IsHomePage("http://ipv6.google.com/"));

  // Trailing dots.
  EXPECT_TRUE(IsHomePage("http://ipv4.google.com./"));
  EXPECT_TRUE(IsHomePage("http://google.com./"));
}

TEST(GoogleUtilTest, GoodHomePagesSecure) {
  // Valid home page hosts.
  EXPECT_TRUE(IsHomePage("https://google.com"));
  EXPECT_TRUE(IsHomePage("https://www.google.com"));
  EXPECT_TRUE(IsHomePage("https://www.google.ca"));
  EXPECT_TRUE(IsHomePage("https://www.google.co.uk"));
  EXPECT_TRUE(IsHomePage("https://www.google.com:443/"));

  // Only the paths /, /webhp, and /ig.* are valid.  Query parameters are
  // ignored.
  EXPECT_TRUE(IsHomePage("https://www.google.com/"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/webhp"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/webhp?rlz=TEST"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/ig"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/ig/foo"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/ig?rlz=TEST"));
  EXPECT_TRUE(IsHomePage("https://www.google.com/ig/foo?rlz=TEST"));
}

TEST(GoogleUtilTest, BadHomePages) {
  EXPECT_FALSE(IsHomePage(std::string()));

  // If specified, only the "www" subdomain is OK.
  EXPECT_FALSE(IsHomePage("http://maps.google.com"));
  EXPECT_FALSE(IsHomePage("http://foo.google.com"));

  // No non-standard port numbers.
  EXPECT_FALSE(IsHomePage("http://www.google.com:1234"));
  EXPECT_FALSE(IsHomePage("https://www.google.com:5678"));

  // Invalid TLDs.
  EXPECT_FALSE(IsHomePage("http://www.google.example"));
  EXPECT_FALSE(IsHomePage("http://www.google.com.example"));
  EXPECT_FALSE(IsHomePage("http://www.google.example.com"));
  EXPECT_FALSE(IsHomePage("http://www.google.ab.cd"));
  EXPECT_FALSE(IsHomePage("http://www.google.uk.qq"));

  // Must be http or https.
  EXPECT_FALSE(IsHomePage("ftp://www.google.com"));
  EXPECT_FALSE(IsHomePage("file://does/not/exist"));
  EXPECT_FALSE(IsHomePage("bad://www.google.com"));
  EXPECT_FALSE(IsHomePage("www.google.com"));

  // Only the paths /, /webhp, and /ig.* are valid.
  EXPECT_FALSE(IsHomePage("http://www.google.com/abc"));
  EXPECT_FALSE(IsHomePage("http://www.google.com/webhpabc"));
  EXPECT_FALSE(IsHomePage("http://www.google.com/webhp/abc"));
  EXPECT_FALSE(IsHomePage("http://www.google.com/abcig"));
  EXPECT_FALSE(IsHomePage("http://www.google.com/webhp/ig"));

  // A search URL should not be identified as a home page URL.
  EXPECT_FALSE(IsHomePage("http://www.google.com/search?q=something"));

  // Path is case sensitive.
  EXPECT_FALSE(IsHomePage("https://www.google.com/WEBHP"));

  // Only .com subdomain and no www.
  EXPECT_FALSE(IsHomePage("http://ipv4.google.co.uk"));
  EXPECT_FALSE(IsHomePage("http://www.ipv4.google.com"));
}

TEST(GoogleUtilTest, GoodSearches) {
  constexpr struct {
    const char* before_query_param;
    const char* after_query_param;
  } kPatterns[] = {
      // Queries with path "/search" need to have the query parameter in either
      // the url parameter or the hash fragment.
      {"www.google.com/search?", "=something"},
      {"www.google.com/search#", "=something"},
      {"www.google.com/search?name=bob&", "=something"},
      {"www.google.com/search?name=bob#", "=something"},
      {"www.google.com/search?name=bob#age=24&", "=thng"},
      {"www.google.co.uk/search?", "=something"},
      // It's actually valid for both to have the query parameter.
      {"www.google.com/search?", "=something#q=other"},
      // Also valid to have an empty query parameter
      {"www.google.com/search?", "="},

      // Queries with path "/webhp", "/" or "" need to have the query parameter
      // in the hash fragment.
      {"www.google.com/webhp#", "=something"},
      {"www.google.com/webhp#name=bob&", "=something"},
      {"www.google.com/webhp?name=bob#", "=something"},
      {"www.google.com/webhp?name=bob#age=24&", "=thing"},

      {"www.google.com/#", "=something"},
      {"www.google.com/#name=bob&", "=something"},
      {"www.google.com/?name=bob#", "=something"},
      {"www.google.com/?name=bob#age=24&", "=something"},

      {"www.google.com#", "=something"},
      {"www.google.com#name=bob&", "=something"},
      {"www.google.com?name=bob#", "=something"},
      {"www.google.com?name=bob#age=24&", "=something"},

      // Google subdomain queries.
      {"ipv4.google.com/search?", "=something"},
      {"ipv4.google.com#name=bob&", "=something"},
      {"ipv6.google.com?name=bob#", "=something"},
      {"ipv6.google.com?name=bob#age=24&", "=something"},

      // Trailing dots in the hosts.
      {"www.google.com./#", "=something"},
      {"www.google.de./#", "=something"},
      {"ipv4.google.com./#", "=something"},
      {"ipv6.google.com./#", "=something"}};

  for (const auto& pattern : kPatterns) {
    for (const char* scheme : kValidSearchSchemes) {
      for (const char* query_param : kValidSearchQueryParams) {
        EXPECT_TRUE(IsSearch(base::StrCat(
            {scheme, url::kStandardSchemeSeparator, pattern.before_query_param,
             query_param, pattern.after_query_param})));
      }
    }
  }
}

TEST(GoogleUtilTest, BadSearches) {
  // A home page URL should not be identified as a search URL.
  EXPECT_FALSE(IsSearch(google_util::kGoogleHomepageURL));

  // Must be http or https.
  EXPECT_FALSE(IsSearch("ftp://www.google.com/search?q=something"));
  EXPECT_FALSE(IsSearch("file://does/not/exist/search?q=something"));
  EXPECT_FALSE(IsSearch("bad://www.google.com/search?q=something"));
  EXPECT_FALSE(IsSearch("www.google.com/search?q=something"));

  // Empty URL is invalid.
  EXPECT_FALSE(IsSearch(std::string()));

  constexpr const char* kPatterns[] = {"google.com", "www.google.com",
                                       "www.google.com/search",
                                       "www.google.com/search?"};

  for (const char* pattern : kPatterns) {
    for (const char* scheme : kValidSearchSchemes) {
      EXPECT_FALSE(IsSearch(
          base::StrCat({scheme, url::kStandardSchemeSeparator, pattern})));
    }
  }

  constexpr struct {
    const char* before_query_param;
    const char* after_query_param;
  } kPatternsQ[] = {
      // Home page searches without a hash fragment query parameter are invalid.
      {"www.google.com/webhp?", "=something"},
      {"www.google.com/webhp?", "=something#no=good"},
      {"www.google.com/webhp?name=bob&", "=something"},
      {"www.google.com/?", "=something"},
      {"www.google.com?", "=something"},

      // Some paths are outright invalid as searches.
      {"www.google.com/notreal?", "=something"},
      {"www.google.com/chrome?", "=something"},
      {"www.google.com/search/nogood?", "=something"},
      {"www.google.com/webhp/nogood#", "=something"},

      // Case sensitive paths.
      {"www.google.com/SEARCH?", "=something"},
      {"www.google.com/WEBHP#", "=something"}};

  for (const auto& pattern : kPatternsQ) {
    for (const char* scheme : kValidSearchSchemes) {
      for (const char* query_param : kValidSearchQueryParams) {
        EXPECT_FALSE(IsSearch(base::StrCat(
            {scheme, url::kStandardSchemeSeparator, pattern.before_query_param,
             query_param, pattern.after_query_param})));
      }
    }
  }
}

TEST(GoogleUtilTest, GoogleDomains) {
  // Test some good Google domains (valid TLDs).
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://google.com"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.ca"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.off.ai"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com/search?q=thing"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com/webhp"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));

  // Test some bad Google domains (invalid/non-Google TLDs).
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.notrealtld"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.sd"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.faketld/search?q=q"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.yahoo.com"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.biz.tj"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));

  // Test subdomain checks.
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://images.google.com"),
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://images.google.com"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://google.com"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));

  // Port and scheme checks.
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com:80"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.com:123"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("https://www.google.com:443"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.google.com:123"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com:123"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("https://www.google.com:123"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.google.com:80"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("https://www.google.com:443"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("file://www.google.com"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("doesnotexist://www.google.com"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
}

TEST(GoogleUtilTest, GoogleBaseURLNotSpecified) {
  // When no command-line flag is specified, no input to
  // StartsWithCommandLineGoogleBaseURL() should return true.
  EXPECT_FALSE(StartsWithBaseURL(std::string()));
  EXPECT_FALSE(StartsWithBaseURL("http://www.foo.com/"));
  EXPECT_FALSE(StartsWithBaseURL("http://www.google.com/"));

  // By default, none of the IsGoogleXXX functions should return true for a
  // "foo.com" URL.
  EXPECT_FALSE(
      IsGoogleHostname("www.foo.com", google_util::DISALLOW_SUBDOMAIN));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("http://www.foo.com/xyz"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl(GURL("https://www.foo.com/"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsHomePage("https://www.foo.com/webhp"));
  EXPECT_FALSE(IsSearch("http://www.foo.com/search?q=a"));

  // Override the Google base URL on the command line.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");

  // Only URLs which start with exactly the string on the command line should
  // cause StartsWithCommandLineGoogleBaseURL() to return true.
  EXPECT_FALSE(StartsWithBaseURL(std::string()));
  EXPECT_TRUE(StartsWithBaseURL("http://www.foo.com/"));
  EXPECT_TRUE(StartsWithBaseURL("http://www.foo.com/abc"));
  EXPECT_FALSE(StartsWithBaseURL("https://www.foo.com/"));
  EXPECT_FALSE(StartsWithBaseURL("http://www.google.com/"));

  // The various IsGoogleXXX functions should respect the command-line flag.
  EXPECT_TRUE(IsGoogleHostname("www.foo.com", google_util::DISALLOW_SUBDOMAIN));
  EXPECT_FALSE(IsGoogleHostname("foo.com", google_util::ALLOW_SUBDOMAIN));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("http://www.foo.com/xyz"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl(GURL("https://www.foo.com/"),
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsHomePage("https://www.foo.com/webhp"));
  EXPECT_FALSE(IsHomePage("http://www.foo.com/xyz"));
  EXPECT_TRUE(IsSearch("http://www.foo.com/search?q=a"));
}

TEST(GoogleUtilTest, GoogleBaseURLDisallowQuery) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/?q=");
  EXPECT_FALSE(google_util::CommandLineGoogleBaseURL().is_valid());
}

TEST(GoogleUtilTest, GoogleBaseURLDisallowRef) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/#q=");
  EXPECT_FALSE(google_util::CommandLineGoogleBaseURL().is_valid());
}

TEST(GoogleUtilTest, GoogleBaseURLFixup) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "www.foo.com");
  ASSERT_TRUE(google_util::CommandLineGoogleBaseURL().is_valid());
  EXPECT_EQ("http://www.foo.com/",
            google_util::CommandLineGoogleBaseURL().spec());
}

TEST(GoogleUtilTest, YoutubeDomains) {
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://www.youtube.com"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://youtube.com"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://youtube.com/path/main.html"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://notyoutube.com"),
                                  google_util::ALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));

  // YouTube Kids is not a youtube domain as it does not use the standard Google
  // auth stack.
  //
  // Regression test for b/247647476
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://www.youtubekids.com"),
                                  google_util::ALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));

  // TLD checks.
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://www.youtube.ca"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://www.youtube.co.uk"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://www.youtube.notrealtld"),
                                  google_util::ALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));

  // Non-YouTube TLD.
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://www.youtube.biz.tj"),
                                  google_util::ALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));

  // Subdomain checks.
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://images.youtube.com"),
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://images.youtube.com"),
                                  google_util::DISALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));

  // Port and scheme checks.
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://www.youtube.com:80"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("https://www.youtube.com:443"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("http://www.youtube.com:123"),
                                  google_util::DISALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsYoutubeDomainUrl(GURL("http://www.youtube.com:123"),
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsYoutubeDomainUrl(GURL("file://www.youtube.com"),
                                  google_util::DISALLOW_SUBDOMAIN,
                                  google_util::DISALLOW_NON_STANDARD_PORTS));
}

TEST(GoogleUtilTest, GoogleAssociatedDomains) {
  EXPECT_FALSE(google_util::IsGoogleAssociatedDomainUrl(GURL()));

  EXPECT_FALSE(google_util::IsGoogleAssociatedDomainUrl(GURL("invalid")));

  EXPECT_FALSE(
      google_util::IsGoogleAssociatedDomainUrl(GURL("https://myblog.com.au")));

  // A typical URL for a Google production API.
  EXPECT_TRUE(google_util::IsGoogleAssociatedDomainUrl(
      GURL("https://myapi-pa.googleapis.com/v1/myservice")));

  // A typical URL for a test instance of a Google API.
  EXPECT_TRUE(google_util::IsGoogleAssociatedDomainUrl(
      GURL("https://daily0-myapi-pa.sandbox.googleapis.com/v1/myservice")));

  // A Google production API with parameters.
  EXPECT_TRUE(google_util::IsGoogleAssociatedDomainUrl(
      GURL("https://myapi-pa.googleapis.com/v1/myservice?k1=v1&k2=v2")));

  // A Google test API with parameters.
  EXPECT_TRUE(google_util::IsGoogleAssociatedDomainUrl(
      GURL("https://daily0-myapi-pa.sandbox.googleapis.com/v1/"
           "myservice?k1=v1&k2=v2")));
}

TEST(GoogleUtilTest, AppendToAsyncQueryParam) {
  // Append to plain URL.
  EXPECT_EQ(GURL("https://foo.com?async=bar:baz"),
            google_util::AppendToAsyncQueryParam(GURL("https://foo.com"), "bar",
                                                 "baz"));

  // Append to async param.
  EXPECT_EQ(GURL("https://foo.com?async=bar:baz,hello:world"),
            google_util::AppendToAsyncQueryParam(
                GURL("https://foo.com?async=bar:baz"), "hello", "world"));

  // Append to same async param.
  EXPECT_EQ(GURL("https://foo.com?async=bar:baz,bar:buz"),
            google_util::AppendToAsyncQueryParam(
                GURL("https://foo.com?async=bar:baz"), "bar", "buz"));
}

TEST(GoogleUtilTest, GoogleSearchMode) {
  EXPECT_EQ(
      google_util::GoogleSearchModeFromUrl(GURL("https://www.google.com/")),
      google_util::GoogleSearchMode::kWeb);
  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo")),
            google_util::GoogleSearchMode::kWeb);
  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=web")),
            google_util::GoogleSearchMode::kWeb);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=isch")),
            google_util::GoogleSearchMode::kImages);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=vid")),
            google_util::GoogleSearchMode::kVideos);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=shop")),
            google_util::GoogleSearchMode::kShopping);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=flm")),
            google_util::GoogleSearchMode::kFlights);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=bks")),
            google_util::GoogleSearchMode::kBooks);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=nws")),
            google_util::GoogleSearchMode::kNews);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=lcl")),
            google_util::GoogleSearchMode::kLocal);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=invalid")),
            google_util::GoogleSearchMode::kUnknown);

  EXPECT_EQ(google_util::GoogleSearchModeFromUrl(
                GURL("https://www.google.com/search?q=foo&tbm=lcl&tbm=nws")),
            google_util::GoogleSearchMode::kUnknown);
}
