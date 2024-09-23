// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser.h"

#include <string>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/test/with_feature_override.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace privacy_sandbox {

class PrivacySandboxAttestationsParserTest : public testing::Test {};

// A completely empty proto gets parsed as an empty map.
TEST_F(PrivacySandboxAttestationsParserTest, EmptyProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_TRUE(optional_map->empty());
}

// A malformed proto returns std::nullopt to represent an error.
TEST_F(PrivacySandboxAttestationsParserTest, InvalidProto) {
  std::string serialized_proto("invalid proto");
  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_FALSE(optional_map.has_value());
}

// A map with one API for each site should correctly map the APIs between proto
// and C++.
TEST_F(PrivacySandboxAttestationsParserTest, OneSitePerAPIProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site1 = "https://a.com";
  std::string site2 = "https://b.com";
  std::string site3 = "https://c.com";
  std::string site4 = "https://d.com";
  std::string site5 = "https://e.com";

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation1;
  attestation1.add_attested_apis(TOPICS);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation2;
  attestation2.add_attested_apis(PROTECTED_AUDIENCE);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation3;
  attestation3.add_attested_apis(PRIVATE_AGGREGATION);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation4;
  attestation4.add_attested_apis(ATTRIBUTION_REPORTING);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation5;
  attestation5.add_attested_apis(SHARED_STORAGE);

  (*proto.mutable_site_attestations())[site1] = attestation1;
  (*proto.mutable_site_attestations())[site2] = attestation2;
  (*proto.mutable_site_attestations())[site3] = attestation3;
  (*proto.mutable_site_attestations())[site4] = attestation4;
  (*proto.mutable_site_attestations())[site5] = attestation5;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_TRUE(optional_map->size() == 5UL);

  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(site1apis.size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site2apis =
      (*optional_map)[net::SchemefulSite(GURL(site2))];
  ASSERT_TRUE(
      site2apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(site2apis.size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site3apis =
      (*optional_map)[net::SchemefulSite(GURL(site3))];
  ASSERT_TRUE(
      site3apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(site3apis.size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site4apis =
      (*optional_map)[net::SchemefulSite(GURL(site4))];
  ASSERT_TRUE(
      site4apis.Has(PrivacySandboxAttestationsGatedAPI::kAttributionReporting));
  ASSERT_TRUE(site4apis.size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site5apis =
      (*optional_map)[net::SchemefulSite(GURL(site5))];
  ASSERT_TRUE(
      site5apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_TRUE(site5apis.size() == 1UL);
}

// Multiple attested APIs per site should work. Unknown APIs should be ignored.
TEST_F(PrivacySandboxAttestationsParserTest, MultipleAPIsPerSiteProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site1 = "https://a.com";
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation1;
  attestation1.add_attested_apis(TOPICS);
  // Add the default out of range value.
  attestation1.add_attested_apis(UNKNOWN);
  attestation1.add_attested_apis(PROTECTED_AUDIENCE);
  attestation1.add_attested_apis(PRIVATE_AGGREGATION);
  // Add an explicitly out of range value. (This static_cast is undefined...)
  attestation1.add_attested_apis(
      static_cast<PrivacySandboxAttestationsGatedAPIProto>(192));
  attestation1.add_attested_apis(ATTRIBUTION_REPORTING);
  attestation1.add_attested_apis(SHARED_STORAGE);

  (*proto.mutable_site_attestations())[site1] = attestation1;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_TRUE(optional_map->size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kAttributionReporting));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_TRUE(site1apis.size() == 5UL);
}

// Test basic functionality of `all_apis` and `sites_attested_for_all_apis`.
// Let "all APIs" be Topics, Protected Audience, and Private Aggregation.
// Have two sites attested for "all APIs", and one site attested for Private
// Aggregation and Shared Storage only.
TEST_F(PrivacySandboxAttestationsParserTest, AllAPIsProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  // Pretend that there are 3 APIs in the set of "all APIs".
  proto.add_all_apis(TOPICS);
  proto.add_all_apis(PROTECTED_AUDIENCE);
  proto.add_all_apis(PRIVATE_AGGREGATION);

  std::string site1 = "https://a.com";
  std::string site2 = "https://b.com";
  std::string site3 = "https://c.com";

  proto.add_sites_attested_for_all_apis(site1);
  proto.add_sites_attested_for_all_apis(site2);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation3;
  attestation3.add_attested_apis(PRIVATE_AGGREGATION);
  attestation3.add_attested_apis(SHARED_STORAGE);
  (*proto.mutable_site_attestations())[site3] = attestation3;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_TRUE(optional_map->size() == 3UL);

  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(site1apis.size() == 3UL);

  const PrivacySandboxAttestationsGatedAPISet& site2apis =
      (*optional_map)[net::SchemefulSite(GURL(site2))];
  ASSERT_TRUE(site2apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site2apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site2apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(site2apis.size() == 3UL);

  const PrivacySandboxAttestationsGatedAPISet& site3apis =
      (*optional_map)[net::SchemefulSite(GURL(site3))];
  ASSERT_TRUE(
      site3apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(
      site3apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_TRUE(site3apis.size() == 2UL);
}

// Test that nothing goes terribly wrong when the proto has multiple mappings
// for a single site (which should never happen in real use).
TEST_F(PrivacySandboxAttestationsParserTest, RepeatedSiteProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  // Pretend that there are 3 APIs in the set of "all APIs".
  proto.add_all_apis(TOPICS);
  proto.add_all_apis(PROTECTED_AUDIENCE);
  proto.add_all_apis(PRIVATE_AGGREGATION);

  std::string site1 = "https://a.com";

  proto.add_sites_attested_for_all_apis(site1);
  proto.add_sites_attested_for_all_apis(site1);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation3;
  attestation3.add_attested_apis(PRIVATE_AGGREGATION);
  attestation3.add_attested_apis(SHARED_STORAGE);
  (*proto.mutable_site_attestations())[site1] = attestation3;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  // The three mappings for the same site get deduplicated to 1.
  ASSERT_TRUE(optional_map->size() == 1UL);

  // The first mapping is the one that ends up being used, though this
  // isn't particularly important.
  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(site1apis.size() == 3UL);
}

// Test that invalid API enums in `all_apis` are ignored.
TEST_F(PrivacySandboxAttestationsParserTest, InvalidAllAPIsProto) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  // Pretend that there are 3 APIs in the set of "all APIs".
  proto.add_all_apis(TOPICS);
  // Add the default out of range value.
  proto.add_all_apis(UNKNOWN);
  proto.add_all_apis(PROTECTED_AUDIENCE);
  proto.add_all_apis(PRIVATE_AGGREGATION);
  // Add an explicitly out of range value. (This static_cast is undefined...)
  proto.add_all_apis(static_cast<PrivacySandboxAttestationsGatedAPIProto>(192));
  proto.add_all_apis(ATTRIBUTION_REPORTING);
  proto.add_all_apis(SHARED_STORAGE);

  std::string site1 = "https://a.com";
  proto.add_sites_attested_for_all_apis(site1);

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_TRUE(optional_map->size() == 1UL);

  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kAttributionReporting));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_TRUE(site1apis.size() == 5UL);
}

// When 'FencedFramesLocalUnpartitionedDataAccess' feature is enabled, there
// will be a new attestation API `LOCAL_UNPARTITIONED_DATA_ACCESS`.
class FencedFramesLocalUnpartitionedDataAccessAttestationTest
    : public base::test::WithFeatureOverride,
      public PrivacySandboxAttestationsParserTest {
 public:
  FencedFramesLocalUnpartitionedDataAccessAttestationTest()
      : base::test::WithFeatureOverride(
            blink::features::kFencedFramesLocalUnpartitionedDataAccess) {}
};

TEST_P(FencedFramesLocalUnpartitionedDataAccessAttestationTest,
       LocalUnpartitionedDataAccessAttestationEnum) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_EQ(proto.site_attestations_size(), 0);

  std::string site1 = "https://a.com";
  std::string site2 = "https://b.com";

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation1;
  attestation1.add_attested_apis(LOCAL_UNPARTITIONED_DATA_ACCESS);

  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto attestation2;
  attestation2.add_attested_apis(SHARED_STORAGE);
  attestation2.add_attested_apis(LOCAL_UNPARTITIONED_DATA_ACCESS);

  (*proto.mutable_site_attestations())[site1] = attestation1;
  (*proto.mutable_site_attestations())[site2] = attestation2;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_EQ(optional_map->size(), 2UL);

  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  EXPECT_EQ(
      site1apis.Has(
          PrivacySandboxAttestationsGatedAPI::kLocalUnpartitionedDataAccess),
      IsParamFeatureEnabled());
  EXPECT_EQ(site1apis.size(), IsParamFeatureEnabled() ? 1UL : 0UL);

  const PrivacySandboxAttestationsGatedAPISet& site2apis =
      (*optional_map)[net::SchemefulSite(GURL(site2))];
  ASSERT_TRUE(
      site2apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  EXPECT_EQ(
      site2apis.Has(
          PrivacySandboxAttestationsGatedAPI::kLocalUnpartitionedDataAccess),
      IsParamFeatureEnabled());
  EXPECT_EQ(site2apis.size(), IsParamFeatureEnabled() ? 2UL : 1UL);
}

TEST_P(FencedFramesLocalUnpartitionedDataAccessAttestationTest, AllAPIs) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_EQ(proto.site_attestations_size(), 0);

  // There were 5 attestation enums before the local unpartitioned data access
  // change.
  proto.add_all_apis(TOPICS);
  proto.add_all_apis(PROTECTED_AUDIENCE);
  proto.add_all_apis(PRIVATE_AGGREGATION);
  proto.add_all_apis(ATTRIBUTION_REPORTING);
  proto.add_all_apis(SHARED_STORAGE);

  std::string site1 = "https://a.com";
  proto.add_sites_attested_for_all_apis(site1);

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_EQ(optional_map->size(), 1UL);

  // The parsed attestation map should have the site attested for the 5 APIs,
  // regardless of the feature status.
  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kAttributionReporting));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_FALSE(site1apis.Has(
      PrivacySandboxAttestationsGatedAPI::kLocalUnpartitionedDataAccess));
  ASSERT_EQ(site1apis.size(), 5UL);
}

TEST_P(FencedFramesLocalUnpartitionedDataAccessAttestationTest,
       AllAPIsWithLocalUnpartitionedDataAccess) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_EQ(proto.site_attestations_size(), 0);

  // With the local unpartitioned data access change, all APIs will include the
  // new attestation enum.
  proto.add_all_apis(TOPICS);
  proto.add_all_apis(PROTECTED_AUDIENCE);
  proto.add_all_apis(PRIVATE_AGGREGATION);
  proto.add_all_apis(ATTRIBUTION_REPORTING);
  proto.add_all_apis(SHARED_STORAGE);
  proto.add_all_apis(LOCAL_UNPARTITIONED_DATA_ACCESS);

  std::string site1 = "https://a.com";
  proto.add_sites_attested_for_all_apis(site1);

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  std::optional<PrivacySandboxAttestationsMap> optional_map =
      ParseAttestationsFromString(serialized_proto);
  ASSERT_TRUE(optional_map.has_value());
  ASSERT_EQ(optional_map->size(), 1UL);

  // If feature enabled, the attestation map should have the site attested for
  // all 6 APIs. Otherwise, the site is attested for the 5 pre-existing APIs,
  // excluding `LOCAL_UNPARTITIONED_DATA_ACCESS`.
  const PrivacySandboxAttestationsGatedAPISet& site1apis =
      (*optional_map)[net::SchemefulSite(GURL(site1))];
  ASSERT_TRUE(site1apis.Has(PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kPrivateAggregation));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kAttributionReporting));
  ASSERT_TRUE(
      site1apis.Has(PrivacySandboxAttestationsGatedAPI::kSharedStorage));
  ASSERT_EQ(
      site1apis.Has(
          PrivacySandboxAttestationsGatedAPI::kLocalUnpartitionedDataAccess),
      IsParamFeatureEnabled());
  ASSERT_EQ(site1apis.size(), IsParamFeatureEnabled() ? 6UL : 5UL);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    FencedFramesLocalUnpartitionedDataAccessAttestationTest);

}  // namespace privacy_sandbox
