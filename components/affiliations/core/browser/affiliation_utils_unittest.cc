// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_utils.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace affiliations {

namespace {
const char kTestFacetURI1[] = "https://alpha.example.com/";
const char kTestFacetURI2[] = "https://beta.example.com/";
const char kTestFacetURI3[] = "https://gamma.example.com/";
}  // namespace

class AffiliationUtilsTest : public testing::Test {};

TEST_F(AffiliationUtilsTest, FacetBrandingInfoOperatorEq) {
  FacetBrandingInfo facet_1 = {"First Facet", GURL()};
  FacetBrandingInfo facet_1_with_icon = {
      "First Facet", GURL("https://example.com/icon_1.png")};
  FacetBrandingInfo facet_2 = {"Second Facet", GURL()};
  FacetBrandingInfo facet_2_with_icon = {
      "Second Facet", GURL("https://example.com/icon_2.png")};

  EXPECT_EQ(facet_1, facet_1);
  EXPECT_NE(facet_1, facet_1_with_icon);
  EXPECT_NE(facet_1, facet_2);
  EXPECT_NE(facet_1, facet_2_with_icon);

  EXPECT_NE(facet_1_with_icon, facet_1);
  EXPECT_EQ(facet_1_with_icon, facet_1_with_icon);
  EXPECT_NE(facet_1_with_icon, facet_2);
  EXPECT_NE(facet_1_with_icon, facet_2_with_icon);

  EXPECT_NE(facet_2, facet_1);
  EXPECT_NE(facet_2, facet_1_with_icon);
  EXPECT_EQ(facet_2, facet_2);
  EXPECT_NE(facet_2, facet_2_with_icon);

  EXPECT_NE(facet_2_with_icon, facet_1);
  EXPECT_NE(facet_2_with_icon, facet_1_with_icon);
  EXPECT_NE(facet_2_with_icon, facet_2);
  EXPECT_EQ(facet_2_with_icon, facet_2_with_icon);
}

TEST_F(AffiliationUtilsTest, FacetOperatorEq) {
  Facet facet_1(FacetURI::FromPotentiallyInvalidSpec(kTestFacetURI1));
  Facet facet_2(FacetURI::FromPotentiallyInvalidSpec(kTestFacetURI2));
  EXPECT_EQ(facet_1, facet_1);
  EXPECT_NE(facet_1, facet_2);
  EXPECT_NE(facet_2, facet_1);
  EXPECT_EQ(facet_2, facet_2);
}

TEST_F(AffiliationUtilsTest, ValidWebFacetURIs) {
  struct {
    const char* valid_facet_uri;
    const char* expected_canonical_facet_uri;
  } kTestCases[] = {
      {"https://www.example.com", "https://www.example.com"},
      {"HTTPS://www.EXAMPLE.com", "https://www.example.com"},
      {"https://0321.0x86.161.0043", "https://209.134.161.35"},
      {"https://www.%65xample.com", "https://www.example.com"},
      {"https://sz\xc3\xb3t\xc3\xa1r.example.com",
       "https://xn--sztr-7na0i.example.com"},
      {"https://www.example.com/", "https://www.example.com"},
      {"https://www.example.com:", "https://www.example.com"},
      {"https://@www.example.com", "https://www.example.com"},
      {"https://:@www.example.com", "https://www.example.com"},
      {"https://www.example.com:8080", "https://www.example.com:8080"},
      {"https://new-gtld", "https://new-gtld"}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("URI = ") << test_case.valid_facet_uri);
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(test_case.valid_facet_uri);
    ASSERT_TRUE(facet_uri.IsValidWebFacetURI());
    EXPECT_EQ(std::string(test_case.expected_canonical_facet_uri),
              facet_uri.canonical_spec());
    EXPECT_EQ(url::kHttpsScheme, facet_uri.scheme());
    EXPECT_EQ("", facet_uri.android_package_name());
  }
}

TEST_F(AffiliationUtilsTest, InvalidWebFacetURIs) {
  const char* kInvalidFacetURIs[]{
      // Invalid URL (actually, will be treated as having only a host part).
      "Does not look like a valid URL.",
      // Path is more than just the root path ('/').
      "https://www.example.com/path",
      // Empty scheme and not HTTPS scheme.
      "://www.example.com",
      "http://www.example.com/",
      // Forbidden non-empty components.
      "https://user@www.example.com/",
      "https://:password@www.example.com/",
      "https://www.example.com/?",
      "https://www.example.com/?query",
      "https://www.example.com/#",
      "https://www.example.com/#ref",
      // Valid Android facet URI.
      "android://hash@com.example.android"};
  for (const char* uri : kInvalidFacetURIs) {
    SCOPED_TRACE(testing::Message("URI = ") << uri);
    FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(uri);
    EXPECT_FALSE(facet_uri.IsValidWebFacetURI());
  }
}

TEST_F(AffiliationUtilsTest, ValidAndroidFacetURIs) {
  struct {
    const char* valid_facet_uri;
    const char* expected_canonical_facet_uri;
    const char* expected_package_name;
  } kTestCases[] = {
      {"android://"
       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
       "@com.example.android",
       "android://"
       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
       "@com.example.android",
       "com.example.android"},
      {"ANDROID://"
       "hash@abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ",
       "android://"
       "hash@abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ",
       "abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
      {"android://needpadding@com.example.android",
       "android://needpadding=@com.example.android",
       "com.example.android"},
      {"android://needtounescape%3D%3D@com.%65xample.android",
       "android://needtounescape==@com.example.android",
       "com.example.android"},
      {"ANDROID://hash@com.example.android",
       "android://hash@com.example.android",
       "com.example.android"},
      {"android://hash@com.example.android/",
       "android://hash@com.example.android",
       "com.example.android"},
      {"android://hash:@com.example.android",
       "android://hash@com.example.android",
       "com.example.android"}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("URI = ") << test_case.valid_facet_uri);
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(test_case.valid_facet_uri);
    ASSERT_TRUE(facet_uri.IsValidAndroidFacetURI());
    EXPECT_EQ(test_case.expected_canonical_facet_uri,
              facet_uri.canonical_spec());
    EXPECT_EQ("android", facet_uri.scheme());
    EXPECT_EQ(test_case.expected_package_name,
              facet_uri.android_package_name());
  }
}

TEST_F(AffiliationUtilsTest, InvalidAndroidFacetURIs) {
  const char* kInvalidFacetURIs[]{
      // Invalid URL (actually, will be treated as having only a host part).
      "Does not look like a valid URL.",
      // Path is more than just the root path ('/').
      "android://hash@com.example.android/path",
      // Empty scheme or not "android" scheme.
      "://hash@com.example.android",
      "http://hash@com.example.android",
      // Package name with illegal characters.
      "android://hash@com.$example.android",
      "android://hash@com-example-android",
      "android://hash@com%2Dexample.android",             // Escaped '-'.
      "android://hash@com.example.sz\xc3\xb3t\xc3\xa1r",  // UTF-8 o' and a'.
      // Empty, non-existent and malformed hash part.
      "android://@com.example.android",
      "android://com.example.android",
      "android://badpadding=a@com.example.android",
      "android://toolongpaddin===@com.example.android",
      "android://invalid+characters@com.example.android",
      "android://invalid%2Fcharacters@com.example.android",  // Escaped '/'.
      // Forbidden non-empty components.
      "android://hash:password@com.example.android",
      "android://hash@com.example.android:port",
      "android://hash@com.example.android/?",
      "android://hash@com.example.android/?query",
      "android://hash@com.example.android/#",
      "android://hash@com.example.android/#ref",
      // Valid Web facet URI.
      "https://www.example.com/"};
  for (const char* uri : kInvalidFacetURIs) {
    SCOPED_TRACE(testing::Message("URI = ") << uri);
    FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(uri);
    EXPECT_FALSE(facet_uri.IsValidAndroidFacetURI());
    EXPECT_EQ("", facet_uri.android_package_name());
  }
}

TEST_F(AffiliationUtilsTest, EqualEquivalenceClasses) {
  AffiliatedFacets a = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };

  AffiliatedFacets b = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
  };

  EXPECT_TRUE(AreEquivalenceClassesEqual(a, a));
  EXPECT_TRUE(AreEquivalenceClassesEqual(b, b));
  EXPECT_TRUE(AreEquivalenceClassesEqual(b, a));
  EXPECT_TRUE(AreEquivalenceClassesEqual(a, b));
}

TEST_F(AffiliationUtilsTest, NotEqualEquivalenceClasses) {
  AffiliatedFacets a = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
  };

  AffiliatedFacets b = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };

  AffiliatedFacets c;

  EXPECT_FALSE(AreEquivalenceClassesEqual(a, b));
  EXPECT_FALSE(AreEquivalenceClassesEqual(a, c));
  EXPECT_FALSE(AreEquivalenceClassesEqual(b, a));
  EXPECT_FALSE(AreEquivalenceClassesEqual(b, c));
  EXPECT_FALSE(AreEquivalenceClassesEqual(c, a));
  EXPECT_FALSE(AreEquivalenceClassesEqual(c, b));
}

TEST_F(AffiliationUtilsTest, GetAndroidPackageDisplayName) {
  const struct {
    const char* input;
    const char* output;
  } kTestCases[] = {
      {"android://hash@com.example.android", "android.example.com"},
      {"android://hash@com.example.mobile", "mobile.example.com"},
      {"android://hash@net.example.subdomain", "subdomain.example.net"}};
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output,
              FacetURI::FromPotentiallyInvalidSpec(test_case.input)
                  .GetAndroidPackageDisplayName());
  }
}

struct MainDomainTestCase {
  std::string url;
  std::string expected_result;
};

class AffiliationUtilsMainDomainTest
    : public testing::Test,
      public testing::WithParamInterface<MainDomainTestCase> {
 protected:
  const base::flat_set<std::string>& psl_extension_list() {
    return psl_extension_list_;
  }

 private:
  base::flat_set<std::string> psl_extension_list_ = {
      "app.link",
      "bttn.io",
      "test-app.link",
      "smart.link",
      "page.link",
      "onelink.me",
      "goo.gl",
      "app.goo.gl",
      "more.app.goo.gl",
      // Missing domain.goo.gl on purpose to show all levels need to be included
      // for multi-level extended main domain (see b/196013199#comment4 for more
      // context)
      "included.domain.goo.gl",
  };
};

TEST_P(AffiliationUtilsMainDomainTest, ParamTest) {
  const MainDomainTestCase& tc = GetParam();
  EXPECT_THAT(GetExtendedTopLevelDomain(GURL(tc.url), psl_extension_list()),
              testing::Eq(tc.expected_result));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AffiliationUtilsMainDomainTest,
    ::testing::Values(
        // error cases
        MainDomainTestCase(),                         // empty string
        MainDomainTestCase{"some arbitrary string"},  // not parsable
        MainDomainTestCase{"amazon.com"},             // no schema
        MainDomainTestCase{"https://"},               // empty host
        MainDomainTestCase{"https://.com"},  // Not under psl, too short
        MainDomainTestCase{"https://192.168.100.1"},  // ip as hostname
        // In PSL list or unknown domain
        MainDomainTestCase{"https://main.unknown",
                           "main.unknown"},  // unknown domain
        // Blogspot.com, special case which is in PSL
        MainDomainTestCase{"https://foo.blogspot.com", "foo.blogspot.com"},
        // different url depths
        MainDomainTestCase{"https://f.com", "f.com"},
        MainDomainTestCase{"https://facebook.com", "facebook.com"},
        MainDomainTestCase{"https://www.facebook.com", "facebook.com"},
        MainDomainTestCase{"https://many.many.many.facebook.com",
                           "facebook.com"},
        // different url schemas and non tld parts
        MainDomainTestCase{"http://www.twitter.com", "twitter.com"},
        MainDomainTestCase{"https://mobile.twitter.com", "twitter.com"},
        MainDomainTestCase{"android://blabla@com.twitter.android"},
        // additional URI components, see
        // https://tools.ietf.org/html/rfc3986#section-3
        MainDomainTestCase{"https://facebook.com/", "facebook.com"},
        MainDomainTestCase{"https://facebook.com/path/", "facebook.com"},
        MainDomainTestCase{"https://facebook.com?queryparam=value",
                           "facebook.com"},
        MainDomainTestCase{"https://facebook.com#fragment", "facebook.com"},
        MainDomainTestCase{"https://userinfo@facebook.com", "facebook.com"},
        // public suffix with more than one component
        MainDomainTestCase{"https://facebook.co.uk", "facebook.co.uk"},
        MainDomainTestCase{"https://www.some.trentinosuedtirol.it",
                           "some.trentinosuedtirol.it"},
        MainDomainTestCase{"https://www.some.ac.gov.br", "some.ac.gov.br"},
        // extended top level domains
        MainDomainTestCase{"https://app.link", "app.link"},
        MainDomainTestCase{"https://user1.app.link", "user1.app.link"},
        MainDomainTestCase{"https://user1.test-app.link",
                           "user1.test-app.link"},
        MainDomainTestCase{"https://many.many.many.user1.app.link",
                           "user1.app.link"},
        // multi level extended top level domains (see b/196013199 and
        // http://doc/1LlPX9DxrCZxsuB_b52vCdiGavVupaI9zjiibdQb9v24)
        MainDomainTestCase{"https://goo.gl", "goo.gl"},
        MainDomainTestCase{"https://app.goo.gl", "app.goo.gl"},
        MainDomainTestCase{"https://user1.app.goo.gl", "user1.app.goo.gl"},
        MainDomainTestCase{"https://many.many.many.user1.app.goo.gl",
                           "user1.app.goo.gl"},
        MainDomainTestCase{"https://one.more.app.goo.gl",
                           "one.more.app.goo.gl"},
        // PSL_EXTENSION_LIST contains included.domain.goo.gl but missing
        // domain.goo.gl due to this multi level extension does not extend
        // beyond this level.
        MainDomainTestCase{"https://levels.not.included.domain.goo.gl",
                           "domain.goo.gl"},
        // Http schema
        MainDomainTestCase{"http://f.com", "f.com"},
        MainDomainTestCase{"http://facebook.com", "facebook.com"},
        MainDomainTestCase{"http://www.facebook.com", "facebook.com"},
        MainDomainTestCase{"http://many.many.many.facebook.com",
                           "facebook.com"}));

struct MergeRelatedGroupsTestCase {
  std::vector<std::vector<std::string>> input_groups;
  std::vector<std::vector<std::string>> output_groups;
  std::vector<std::string> psl_extensions;
};

class AffiliationUtilsMergeRelatedGroupsTest
    : public testing::Test,
      public testing::WithParamInterface<MergeRelatedGroupsTestCase> {
 protected:
  std::vector<GroupedFacets> GetGroups(
      const std::vector<std::vector<std::string>>& groups) {
    std::vector<GroupedFacets> results;
    for (const auto& group : groups) {
      GroupedFacets result;
      for (const auto& facet : group) {
        result.facets.emplace_back(
            FacetURI::FromPotentiallyInvalidSpec("https://" + facet));
      }
      results.push_back(std::move(result));
    }
    return results;
  }

  void SortFacets(std::vector<GroupedFacets>& groups) {
    for (auto& group : groups) {
      std::sort(group.facets.begin(), group.facets.end(),
                [](const auto& lhs, const auto& rhs) {
                  return base::CompareCaseInsensitiveASCII(
                      lhs.uri.potentially_invalid_spec(),
                      rhs.uri.potentially_invalid_spec());
                });
    }
  }

  base::flat_set<std::string> GetPSLExtensions() {
    return base::flat_set<std::string>(GetParam().psl_extensions);
  }
};

TEST_P(AffiliationUtilsMergeRelatedGroupsTest, ParamTest) {
  std::vector<GroupedFacets> expected_groups =
      GetGroups(GetParam().output_groups);
  std::vector<GroupedFacets> actual_groups = MergeRelatedGroups(
      GetPSLExtensions(), GetGroups(GetParam().input_groups));

  // Sort facets to simplify testing as their order doesn'r matter
  SortFacets(expected_groups);
  SortFacets(actual_groups);

  EXPECT_THAT(actual_groups,
              testing::UnorderedElementsAreArray(expected_groups));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AffiliationUtilsMergeRelatedGroupsTest,
    ::testing::Values(
        MergeRelatedGroupsTestCase{{{"a.com"}, {"b.com"}, {"c.com"}},
                                   {{"a.com"}, {"b.com"}, {"c.com"}},
                                   {}},
        MergeRelatedGroupsTestCase{
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {{"a.com", "test1.a.com", "test2.a.com"}},
            {}},
        // When a.com is extended to be a public suffix the groups no longer
        // merge together.
        MergeRelatedGroupsTestCase{
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {"a.com"}},
        MergeRelatedGroupsTestCase{{{"a.com", "b.com"}, {"www.b.com", "c.com"}},
                                   {{"a.com", "b.com", "www.b.com", "c.com"}},
                                   {}},
        MergeRelatedGroupsTestCase{
            {{"a.com", "b.com"}, {"www.b.com", "c.com"}, {"d.org"}},
            {{"a.com", "b.com", "www.b.com", "c.com"}, {"d.org"}},
            {}},
        MergeRelatedGroupsTestCase{
            {{"a.com", "b.com", "c.com"},
             {"www.b.com"},
             {"d.org", "www.c.com"}},
            {{"a.com", "b.com", "c.com", "www.b.com", "d.org", "www.c.com"}},
            {}}

        ));

}  // namespace affiliations
