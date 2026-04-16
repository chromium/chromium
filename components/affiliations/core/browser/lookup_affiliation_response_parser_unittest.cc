// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/lookup_affiliation_response_parser.h"

#include <vector>

#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
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

}  // namespace affiliations
