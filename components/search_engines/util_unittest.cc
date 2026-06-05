// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using SearchEngineUtilTest = testing::Test;

TEST_F(SearchEngineUtilTest, IsSearchEngineNameValidToUse) {
  EXPECT_TRUE(IsSearchEngineNameValidToUse(u"Google"));
  EXPECT_TRUE(IsSearchEngineNameValidToUse(u"  Google  "));
  EXPECT_FALSE(IsSearchEngineNameValidToUse(u""));
  EXPECT_FALSE(IsSearchEngineNameValidToUse(u"   "));
}

using SearchEngineUtilTemplateUrlTest = LoadedTemplateURLServiceUnitTestBase;

TEST_F(SearchEngineUtilTemplateUrlTest, IsSearchEngineKeywordValidToUse) {
  TemplateURLService* service = &template_url_service();

  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google");
  data.SetURL("http://www.google.com/");
  TemplateURL* t_url = service->Add(std::make_unique<TemplateURL>(data));

  EXPECT_TRUE(IsSearchEngineKeywordValidToUse(u"bing", service, nullptr));
  EXPECT_TRUE(IsSearchEngineKeywordValidToUse(u"yahoo", service, nullptr));
  EXPECT_FALSE(IsSearchEngineKeywordValidToUse(u"", service, nullptr));
  EXPECT_FALSE(IsSearchEngineKeywordValidToUse(u"   ", service, nullptr));

  // Keywords with whitespace are not allowed.
  EXPECT_FALSE(IsSearchEngineKeywordValidToUse(u"foo bar", service, nullptr));

  // Keyword already in use.
  EXPECT_FALSE(IsSearchEngineKeywordValidToUse(u"google", service, nullptr));

  // Keyword already in use (case-insensitive).
  EXPECT_FALSE(IsSearchEngineKeywordValidToUse(u"Google", service, nullptr));

  // Keyword in use, but by the same TemplateURL (editing).
  EXPECT_TRUE(IsSearchEngineKeywordValidToUse(u"google", service, t_url));

  // Keyword in use by the same TemplateURL (editing, case-insensitive).
  EXPECT_TRUE(IsSearchEngineKeywordValidToUse(u"Google", service, t_url));
}

TEST_F(SearchEngineUtilTemplateUrlTest, GetFixedUpSearchEngineUrl) {
  TemplateURLService* service = &template_url_service();
  const SearchTermsData& search_terms_data = service->search_terms_data();

  EXPECT_EQ(
      "http://www.google.com/",
      GetFixedUpSearchEngineUrl("http://www.google.com/", search_terms_data));
  EXPECT_EQ("http://www.google.com/",
            GetFixedUpSearchEngineUrl("www.google.com/", search_terms_data));
  EXPECT_EQ("http://google.com",
            GetFixedUpSearchEngineUrl("google.com", search_terms_data));
  EXPECT_EQ("", GetFixedUpSearchEngineUrl("", search_terms_data));
  EXPECT_EQ("", GetFixedUpSearchEngineUrl("   ", search_terms_data));
}

TEST_F(SearchEngineUtilTemplateUrlTest, IsSearchEngineURLValidToUse) {
  TemplateURLService* service = &template_url_service();

  EXPECT_TRUE(
      IsSearchEngineURLValidToUse("http://www.google.com/", service, nullptr));
  EXPECT_TRUE(IsSearchEngineURLValidToUse("www.google.com/", service, nullptr));
  EXPECT_TRUE(IsSearchEngineURLValidToUse("google.com", service, nullptr));
  EXPECT_TRUE(IsSearchEngineURLValidToUse(
      "http://google.com/search?q={searchTerms}", service, nullptr));
  EXPECT_FALSE(IsSearchEngineURLValidToUse("", service, nullptr));

  // Default search engine check.
  const TemplateURL* default_provider = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);

  // A URL that doesn't support replacement should fail if it's for the default
  // provider.
  EXPECT_FALSE(IsSearchEngineURLValidToUse("http://google.com/", service,
                                           default_provider));

  // A URL that supports replacement should pass.
  EXPECT_TRUE(IsSearchEngineURLValidToUse(
      "http://google.com/search?q={searchTerms}", service, default_provider));
}

TEST_F(SearchEngineUtilTest, IsAimZeroStateURL) {
  EXPECT_TRUE(IsAimZeroStateURL(GURL("https://www.google.com/?udm=50")));
  EXPECT_TRUE(IsAimZeroStateURL(GURL("https://google.com/?udm=50")));

  // Subdomain should be rejected.
  EXPECT_FALSE(IsAimZeroStateURL(GURL("https://amp.google.com/?udm=50")));
  EXPECT_FALSE(IsAimZeroStateURL(GURL("https://sub.google.com/?udm=50")));

  // Non-home page path should be rejected.
  EXPECT_FALSE(IsAimZeroStateURL(
      GURL("https://www.google.com/amp/s/attacker.com?udm=50")));
  EXPECT_FALSE(IsAimZeroStateURL(GURL("https://www.google.com/phish?udm=50")));

  // Search URL with a query should be rejected (handled by IsAimURL).
  EXPECT_FALSE(
      IsAimZeroStateURL(GURL("https://www.google.com/search?udm=50&q=test")));

  // Search URL without query should be caught as AimZeroStateURL.
  EXPECT_TRUE(IsAimZeroStateURL(GURL("https://www.google.com/search?udm=50")));

  // Missing udm=50 should be rejected.
  EXPECT_FALSE(IsAimZeroStateURL(GURL("https://www.google.com/")));
}

TEST_F(SearchEngineUtilTest, IsAimURL) {
  EXPECT_TRUE(IsAimURL(GURL("https://www.google.com/search?udm=50&q=test")));
  EXPECT_TRUE(IsAimURL(GURL("https://google.com/search?udm=50&q=test")));

  // Subdomain should be rejected.
  EXPECT_FALSE(IsAimURL(GURL("https://amp.google.com/search?udm=50&q=test")));

  // No query parameter q should be rejected (it's a zero state URL).
  EXPECT_FALSE(IsAimURL(GURL("https://www.google.com/search?udm=50")));

  // Missing udm=50 should be rejected.
  EXPECT_FALSE(IsAimURL(GURL("https://www.google.com/search?q=test")));
}

TEST_F(SearchEngineUtilTemplateUrlTest, GetUrlForAim_QsubtsFlag) {
  TemplateURLService* service = &template_url_service();

  // By default, flag is disabled, so qsubts should NOT be present.
  GURL url = GetUrlForAim(service, omnibox::ChromeAimEntryPoint(0),
                          base::Time::Now(), u"test", std::nullopt, {});
  std::string qsubts;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "qsubts", &qsubts));

  // Enable the flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      lens::features::kLensSendQuerySubmissionTime);

  url = GetUrlForAim(service, omnibox::ChromeAimEntryPoint(0),
                     base::Time::Now(), u"test", std::nullopt, {});
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "qsubts", &qsubts));
}
