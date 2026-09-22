// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "build/build_config.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search {

TEST(SearchTest, TemplateURLIsGoogle) {
  SearchTermsData search_terms_data;

  // Nullptr returns false.
  EXPECT_FALSE(TemplateURLIsGoogle(nullptr, search_terms_data));

  // Google search URL + Google suggestion URL returns true.
  TemplateURLData google_data;
  google_data.SetURL("https://www.google.com/search?q={searchTerms}");
  google_data.suggestions_url =
      "https://www.google.com/complete/search?q={searchTerms}";
  TemplateURL google_turl(google_data);
  EXPECT_TRUE(TemplateURLIsGoogle(&google_turl, search_terms_data));

  // Google search URL + empty suggestion URL returns true.
  TemplateURLData google_no_suggest_data;
  google_no_suggest_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  TemplateURL google_no_suggest_turl(google_no_suggest_data);
  EXPECT_TRUE(TemplateURLIsGoogle(&google_no_suggest_turl, search_terms_data));

  // Google search URL + non-Google suggestion URL (spoofed engine) returns
  // false.
  TemplateURLData spoofed_data;
  spoofed_data.SetURL("https://www.google.com/search?q={searchTerms}");
  spoofed_data.suggestions_url =
      "https://attacker.com/complete/search?q={searchTerms}";
  TemplateURL spoofed_turl(spoofed_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&spoofed_turl, search_terms_data));

  // Non-Google search URL + Google suggestion URL returns false.
  TemplateURLData non_google_data;
  non_google_data.SetURL("https://attacker.com/search?q={searchTerms}");
  non_google_data.suggestions_url =
      "https://www.google.com/complete/search?q={searchTerms}";
  TemplateURL non_google_turl(non_google_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&non_google_turl, search_terms_data));

  // Google search URL + invalid suggestion URL returns false.
  TemplateURLData invalid_suggest_data;
  invalid_suggest_data.SetURL("https://www.google.com/search?q={searchTerms}");
  invalid_suggest_data.suggestions_url = "invalid url with spaces";
  TemplateURL invalid_suggest_turl(invalid_suggest_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&invalid_suggest_turl, search_terms_data));

  // Google search URL + Google template placeholder ({google:baseSuggestURL})
  // returns true.
  TemplateURLData google_placeholder_data;
  google_placeholder_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_placeholder_data.suggestions_url =
      "{google:baseSuggestURL}search?q={searchTerms}";
  TemplateURL google_placeholder_turl(google_placeholder_data);
  EXPECT_TRUE(TemplateURLIsGoogle(&google_placeholder_turl, search_terms_data));

  // Google search URL + spoofed Google subdomain returns false (subdomains
  // disallowed).
  TemplateURLData subdomain_data;
  subdomain_data.SetURL("https://www.google.com/search?q={searchTerms}");
  subdomain_data.suggestions_url =
      "https://attacker.google.com/complete/search?q={searchTerms}";
  TemplateURL subdomain_turl(subdomain_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&subdomain_turl, search_terms_data));

  // Google search URL + non-cryptographic (HTTP) suggestion URL returns false.
  TemplateURLData http_data;
  http_data.SetURL("https://www.google.com/search?q={searchTerms}");
  http_data.suggestions_url =
      "http://www.google.com/complete/search?q={searchTerms}";
  TemplateURL http_turl(http_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&http_turl, search_terms_data));

  // Non-cryptographic (HTTP) search URL + Google suggestion URL returns false.
  TemplateURLData http_search_data;
  http_search_data.SetURL("http://www.google.com/search?q={searchTerms}");
  http_search_data.suggestions_url =
      "https://www.google.com/complete/search?q={searchTerms}";
  TemplateURL http_search_turl(http_search_data);
  EXPECT_FALSE(TemplateURLIsGoogle(&http_search_turl, search_terms_data));

  // Non-cryptographic (HTTP) search URL + empty suggestion URL returns false.
  TemplateURLData http_search_no_suggest_data;
  http_search_no_suggest_data.SetURL(
      "http://www.google.com/search?q={searchTerms}");
  TemplateURL http_search_no_suggest_turl(http_search_no_suggest_data);
  EXPECT_FALSE(
      TemplateURLIsGoogle(&http_search_no_suggest_turl, search_terms_data));
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST(SearchTest, InstantExtendedAPIEnabled) {
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
}

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace search
