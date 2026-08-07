// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/lookup_affiliation_response_parser.h"

#include <vector>

#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace affiliations {

TEST(LookupAffiliationResponseParserTest, ParseChangePasswordUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* change_password_info = facet->mutable_change_password_info();
  change_password_info->set_change_password_url("https://example.com/change");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.affiliations.size());
  ASSERT_EQ(1u, result.affiliations[0].size());
  EXPECT_EQ("https://example.com/change",
            result.affiliations[0][0].change_password_url.spec());
}

TEST(LookupAffiliationResponseParserTest, IgnoreInvalidChangePasswordUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* change_password_info = facet->mutable_change_password_info();
  change_password_info->set_change_password_url("invalid_url");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.affiliations.size());
  ASSERT_EQ(1u, result.affiliations[0].size());
  EXPECT_TRUE(result.affiliations[0][0].change_password_url.is_empty());
}

TEST(LookupAffiliationResponseParserTest, IgnoreNonHttpsChangePasswordUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* change_password_info = facet->mutable_change_password_info();
  change_password_info->set_change_password_url("http://example.com/change");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.affiliations.size());
  ASSERT_EQ(1u, result.affiliations[0].size());
  EXPECT_TRUE(result.affiliations[0][0].change_password_url.is_empty());
}

TEST(LookupAffiliationResponseParserTest, ParseChangePasswordPatterns) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* change_password_info = facet->mutable_change_password_info();

  auto* pattern1 = change_password_info->add_patterns();
  pattern1->set_url_pattern_re2("https://example.com/foo/.*");
  pattern1->set_change_password_url("https://example.com/change_foo");

  auto* pattern2 = change_password_info->add_patterns();
  pattern2->set_url_pattern_re2("https://example.com/bar/.*");
  pattern2->set_change_password_url("https://example.com/change_bar");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.affiliations.size());
  ASSERT_EQ(1u, result.affiliations[0].size());
  ASSERT_EQ(2u, result.affiliations[0][0].change_password_patterns.size());

  EXPECT_THAT(
      result.affiliations[0][0].change_password_patterns,
      testing::ElementsAre(
          ChangePasswordPattern{"https://example.com/foo/.*",
                                GURL("https://example.com/change_foo")},
          ChangePasswordPattern{"https://example.com/bar/.*",
                                GURL("https://example.com/change_bar")}));
}

TEST(LookupAffiliationResponseParserTest, ParseValidIconUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* branding_info = facet->mutable_branding_info();
  branding_info->set_icon_url("https://example.com/icon.png");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.affiliations.size());
  ASSERT_EQ(1u, result.affiliations[0].size());
  EXPECT_EQ("https://example.com/icon.png",
            result.affiliations[0][0].branding_info.icon_url.spec());
}

TEST(LookupAffiliationResponseParserTest, IgnoreNonCryptographicIconUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  auto* facet = affiliation->add_facet();
  facet->set_id("https://example.com");
  auto* branding_info = facet->mutable_branding_info();

  const char* kInvalidUrls[] = {
      "data:image/"
      "png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGA"
      "hKmMIQAAAABJRU5ErkJggg==",
      "chrome://settings/clearBrowserData",
      "javascript:alert(1)",
      "http://example.com/icon.png",
  };

  for (const char* url : kInvalidUrls) {
    branding_info->set_icon_url(url);
    AffiliationFetcherInterface::ParsedFetchResponse result;
    bool success =
        ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

    EXPECT_TRUE(success);
    ASSERT_EQ(1u, result.affiliations.size());
    ASSERT_EQ(1u, result.affiliations[0].size());
    EXPECT_TRUE(result.affiliations[0][0].branding_info.icon_url.is_empty())
        << "Failed for URL: " << url;
  }
}

TEST(LookupAffiliationResponseParserTest, ParseValidGroupIconUrl) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* group = response.add_groups();
  auto* facet = group->add_facet();
  facet->set_id("https://example.com");
  auto* branding_info = group->mutable_group_branding_info();
  branding_info->set_icon_url("https://example.com/icon.png");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(1u, result.groupings.size());
  EXPECT_EQ("https://example.com/icon.png",
            result.groupings[0].branding_info.icon_url.spec());
}

TEST(LookupAffiliationResponseParserTest,
     ParseMultipleEquivalenceClassesWithMultipleFacets) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://other.example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation1 = response.add_affiliations();
  affiliation1->add_facet()->set_id("https://example.com");
  affiliation1->add_facet()->set_id("https://example2.com");
  affiliation1->add_facet()->set_id("https://example3.com");

  auto* affiliation2 = response.add_affiliations();
  affiliation2->add_facet()->set_id("https://other.example.com");
  affiliation2->add_facet()->set_id("https://other2.example.com");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_TRUE(success);
  ASSERT_EQ(2u, result.affiliations.size());
  EXPECT_EQ(3u, result.affiliations[0].size());
  EXPECT_EQ(2u, result.affiliations[1].size());
}

TEST(LookupAffiliationResponseParserTest,
     RejectPartiallyOverlappingEquivalenceClasses) {
  std::vector<FacetURI> requested_facet_uris;
  requested_facet_uris.push_back(
      FacetURI::FromCanonicalSpec("https://example.com"));

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation1 = response.add_affiliations();
  affiliation1->add_facet()->set_id("https://example.com");
  affiliation1->add_facet()->set_id("https://shared.example.com");

  auto* affiliation2 = response.add_affiliations();
  affiliation2->add_facet()->set_id("https://shared.example.com");
  affiliation2->add_facet()->set_id("https://other.example.com");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  bool success =
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result);

  EXPECT_FALSE(success);
}

TEST(LookupAffiliationResponseParserTest, IgnoreDuplicateEquivalenceClasses) {
  std::vector<FacetURI> requested_facet_uris = {
      FacetURI::FromCanonicalSpec("https://example.com")};

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation1 = response.add_affiliations();
  affiliation1->add_facet()->set_id("https://example.com");
  affiliation1->add_facet()->set_id("https://example2.com");

  // Duplicate equivalence class in the same response.
  auto* affiliation2 = response.add_affiliations();
  affiliation2->add_facet()->set_id("https://example.com");
  affiliation2->add_facet()->set_id("https://example2.com");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  EXPECT_TRUE(
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result));
  EXPECT_EQ(1u, result.affiliations.size());
}

TEST(LookupAffiliationResponseParserTest, IgnoreUnrequestedEquivalenceClasses) {
  // Nothing is requested, so the equivalence class below should be excluded
  // and nothing should be synthesized either.
  std::vector<FacetURI> requested_facet_uris;

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  auto* affiliation = response.add_affiliations();
  affiliation->add_facet()->set_id("https://unrequested.example.com");
  affiliation->add_facet()->set_id("https://unrequested2.example.com");

  AffiliationFetcherInterface::ParsedFetchResponse result;
  EXPECT_TRUE(
      ParseLookupAffiliationResponse(requested_facet_uris, response, &result));
  EXPECT_TRUE(result.affiliations.empty());
}

}  // namespace affiliations
