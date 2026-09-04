// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"

#include <string>
#include <vector>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing::v5_search_hashes_util {

namespace {

V5::FullHash CreateFullHash(
    const std::string& full_hash_str,
    std::vector<V5::FullHash::FullHashDetail> threat_details = {}) {
  V5::FullHash full_hash_object;
  full_hash_object.set_full_hash(full_hash_str);
  for (const auto& detail : threat_details) {
    *full_hash_object.add_full_hash_details() = detail;
  }
  return full_hash_object;
}

V5::FullHash::FullHashDetail CreateHashDetail(
    V5::ThreatType threat_type,
    std::vector<V5::ThreatAttribute> threat_attributes = {}) {
  V5::FullHash::FullHashDetail detail;
  detail.set_threat_type(threat_type);
  for (const auto& attribute : threat_attributes) {
    detail.add_attributes(attribute);
  }
  return detail;
}

std::string CreateResponseWithoutCacheDuration() {
  V5::SearchHashesResponse response;
  return response.SerializeAsString();
}

std::string CreateResponseWithBadHashLength() {
  V5::SearchHashesResponse response;
  response.mutable_cache_duration()->set_seconds(300);
  auto* full_hash = response.add_full_hashes();
  full_hash->set_full_hash("bad_length");
  return response.SerializeAsString();
}

}  // namespace

class V5SearchHashesUtilTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(V5SearchHashesUtilTest, GetResourceUrl) {
  V5::SearchHashesRequest request;
  request.add_hash_prefixes("aaaa");
  std::string expected_url =
      "https://safebrowsing.googleapis.com/v5/hashes:search"
      "?$req=CgRhYWFh&$ct=application/x-protobuf";
  auto api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(
        &expected_url, "&key=%s",
        base::EscapeQueryParamValue(api_key, /*use_plus=*/true).c_str());
  }
  EXPECT_EQ(GetResourceUrl(&request), expected_url);
}

TEST_F(V5SearchHashesUtilTest, SearchCache_NullCache) {
  std::vector<std::string> misses;
  std::vector<V5::FullHash> hits;
  SearchCache(/*cache=*/nullptr, {"aaaa"}, &misses, &hits);
  EXPECT_EQ(misses, std::vector<std::string>({"aaaa"}));
  EXPECT_TRUE(hits.empty());
}

TEST_F(V5SearchHashesUtilTest, SearchCache_HitsAndMisses) {
  auto cache = std::make_unique<V5SearchHashesCache>(
      /*history_service=*/nullptr);
  V5::Duration cache_duration;
  cache_duration.set_seconds(300);
  cache->CacheSearchHashesResponse(
      {"aaaa"},
      {CreateFullHash("aaaa1111111111111111111111111111",
                      {CreateHashDetail(V5::ThreatType::MALWARE)})},
      cache_duration);

  std::vector<std::string> misses;
  std::vector<V5::FullHash> hits;
  SearchCache(cache.get(), {"aaaa", "bbbb"}, &misses, &hits);

  EXPECT_EQ(misses, std::vector<std::string>({"bbbb"}));
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].full_hash(), "aaaa1111111111111111111111111111");
}

TEST_F(V5SearchHashesUtilTest, ParseResponseErrors) {
  struct TestCase {
    int net_error;
    int response_code;
    std::string response_body;
    std::vector<std::string> requested_hash_prefixes;
    ParseFailure expected_error;
  } test_cases[] = {
      {net::ERR_FAILED, 0, "", {}, ParseFailure::kNetworkError},
      {net::ERR_NETWORK_CHANGED, 0, "", {}, ParseFailure::kRetriableError},
      {net::OK,
       net::HTTP_INTERNAL_SERVER_ERROR,
       "",
       {},
       ParseFailure::kHttpError},
      {net::OK, net::HTTP_OK, "\x80", {}, ParseFailure::kParseError},
      {net::OK,
       net::HTTP_OK,
       CreateResponseWithoutCacheDuration(),
       {},
       ParseFailure::kNoCacheDurationError},
      {net::OK,
       net::HTTP_OK,
       CreateResponseWithBadHashLength(),
       {"aaaa"},
       ParseFailure::kIncorrectFullHashLengthError},
  };
  int idx = 0;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Test case index: %d", idx++));
    base::expected<ParseResultSuccess, ParseFailure> result = ParseResponse(
        test_case.net_error, test_case.response_code, test_case.response_body,
        test_case.requested_hash_prefixes);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), test_case.expected_error);
  }
}

TEST_F(V5SearchHashesUtilTest, ParseResponseSuccess_SanitizesResponse) {
  V5::SearchHashesResponse response;
  response.mutable_cache_duration()->set_seconds(300);

  // 1. Valid matched full hash
  auto* hash1 = response.add_full_hashes();
  hash1->set_full_hash("aaaa1111111111111111111111111111");
  auto* detail1 = hash1->add_full_hash_details();
  detail1->set_threat_type(V5::ThreatType::MALWARE);

  // 2. Full hash with one valid and one invalid threat type detail (only the
  // invalid detail should be stripped)
  auto* hash2 = response.add_full_hashes();
  hash2->set_full_hash("aaaa2222222222222222222222222222");
  auto* detail2_valid = hash2->add_full_hash_details();
  detail2_valid->set_threat_type(V5::ThreatType::SOCIAL_ENGINEERING);
  auto* detail2_invalid_type = hash2->add_full_hash_details();
  detail2_invalid_type->set_threat_type(static_cast<V5::ThreatType>(999));

  // 3. Full hash with one valid and one invalid threat attribute detail (only
  // the detail with the invalid attribute should be stripped)
  auto* hash3 = response.add_full_hashes();
  hash3->set_full_hash("aaaa3333333333333333333333333333");
  auto* detail3_valid = hash3->add_full_hash_details();
  detail3_valid->set_threat_type(V5::ThreatType::SOCIAL_ENGINEERING);
  auto* detail3_invalid_attr = hash3->add_full_hash_details();
  detail3_invalid_attr->set_threat_type(V5::ThreatType::MALWARE);
  detail3_invalid_attr->add_attributes(static_cast<V5::ThreatAttribute>(999));

  // 4. Full hash with only invalid details (all details should be stripped,
  // but the full hash container itself remains)
  auto* hash4 = response.add_full_hashes();
  hash4->set_full_hash("aaaa4444444444444444444444444444");
  auto* detail4_invalid_type = hash4->add_full_hash_details();
  detail4_invalid_type->set_threat_type(static_cast<V5::ThreatType>(999));
  auto* detail4_invalid_attr = hash4->add_full_hash_details();
  detail4_invalid_attr->set_threat_type(V5::ThreatType::SOCIAL_ENGINEERING);
  detail4_invalid_attr->add_attributes(static_cast<V5::ThreatAttribute>(999));

  base::expected<ParseResultSuccess, ParseFailure> result = ParseResponse(
      net::OK, net::HTTP_OK, response.SerializeAsString(), {"aaaa"});

  ASSERT_TRUE(result.has_value());

  const auto& sanitized_response = result->response;
  // Should have hash1, hash2, hash3, and hash4.
  ASSERT_EQ(sanitized_response.full_hashes_size(), 4);

  EXPECT_EQ(sanitized_response.full_hashes(0).full_hash(),
            "aaaa1111111111111111111111111111");
  EXPECT_EQ(sanitized_response.full_hashes(0).full_hash_details_size(), 1);

  EXPECT_EQ(sanitized_response.full_hashes(1).full_hash(),
            "aaaa2222222222222222222222222222");
  // Should only have detail2_valid. detail2_invalid_type should be removed.
  ASSERT_EQ(sanitized_response.full_hashes(1).full_hash_details_size(), 1);
  EXPECT_EQ(
      sanitized_response.full_hashes(1).full_hash_details(0).threat_type(),
      V5::ThreatType::SOCIAL_ENGINEERING);

  EXPECT_EQ(sanitized_response.full_hashes(2).full_hash(),
            "aaaa3333333333333333333333333333");
  // Should only have detail3_valid. detail3_invalid_attr should be removed.
  ASSERT_EQ(sanitized_response.full_hashes(2).full_hash_details_size(), 1);
  EXPECT_EQ(
      sanitized_response.full_hashes(2).full_hash_details(0).threat_type(),
      V5::ThreatType::SOCIAL_ENGINEERING);

  EXPECT_EQ(sanitized_response.full_hashes(3).full_hash(),
            "aaaa4444444444444444444444444444");
  // All details should be removed.
  EXPECT_EQ(sanitized_response.full_hashes(3).full_hash_details_size(), 0);
}

TEST_F(V5SearchHashesUtilTest, ParseResponse_PreservesThreatAttributes) {
  V5::SearchHashesResponse response;
  response.mutable_cache_duration()->set_seconds(300);

  auto* hash = response.add_full_hashes();
  hash->set_full_hash("aaaa1111111111111111111111111111");
  auto* detail = hash->add_full_hash_details();
  detail->set_threat_type(V5::ThreatType::BETTER_ADS_VIOLATION);
  detail->add_attributes(V5::ThreatAttribute::CANARY);

  base::expected<ParseResultSuccess, ParseFailure> result = ParseResponse(
      net::OK, net::HTTP_OK, response.SerializeAsString(), {"aaaa"});

  ASSERT_TRUE(result.has_value());
  const auto& sanitized_response = result->response;
  ASSERT_EQ(sanitized_response.full_hashes_size(), 1);
  EXPECT_EQ(sanitized_response.full_hashes(0).full_hash(),
            "aaaa1111111111111111111111111111");
  ASSERT_EQ(sanitized_response.full_hashes(0).full_hash_details_size(), 1);
  EXPECT_EQ(
      sanitized_response.full_hashes(0).full_hash_details(0).threat_type(),
      V5::ThreatType::BETTER_ADS_VIOLATION);
  ASSERT_EQ(
      sanitized_response.full_hashes(0).full_hash_details(0).attributes_size(),
      1);
  EXPECT_EQ(
      sanitized_response.full_hashes(0).full_hash_details(0).attributes(0),
      V5::ThreatAttribute::CANARY);
}

TEST_F(V5SearchHashesUtilTest, ParseResponseSuccess_UnmatchedHashes) {
  // 1. With unmatched hashes in response.
  {
    V5::SearchHashesResponse response;
    response.mutable_cache_duration()->set_seconds(300);

    // Valid matched full hash
    auto* hash1 = response.add_full_hashes();
    hash1->set_full_hash("aaaa1111111111111111111111111111");
    hash1->add_full_hash_details()->set_threat_type(V5::ThreatType::MALWARE);

    // Unmatched full hash (prefix bbbb is not in requested list {"aaaa"})
    auto* hash2 = response.add_full_hashes();
    hash2->set_full_hash("bbbb1111111111111111111111111111");
    hash2->add_full_hash_details()->set_threat_type(V5::ThreatType::MALWARE);

    base::expected<ParseResultSuccess, ParseFailure> result = ParseResponse(
        net::OK, net::HTTP_OK, response.SerializeAsString(), {"aaaa"});

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->found_unmatched_full_hashes);
    // hash2 should be filtered out
    EXPECT_EQ(result->response.full_hashes_size(), 1);
    EXPECT_EQ(result->response.full_hashes(0).full_hash(),
              "aaaa1111111111111111111111111111");
  }

  // 2. Without unmatched hashes in response.
  {
    V5::SearchHashesResponse response;
    response.mutable_cache_duration()->set_seconds(300);

    auto* hash1 = response.add_full_hashes();
    hash1->set_full_hash("aaaa1111111111111111111111111111");
    hash1->add_full_hash_details()->set_threat_type(V5::ThreatType::MALWARE);

    base::expected<ParseResultSuccess, ParseFailure> result = ParseResponse(
        net::OK, net::HTTP_OK, response.SerializeAsString(), {"aaaa"});

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->found_unmatched_full_hashes);
    EXPECT_EQ(result->response.full_hashes_size(), 1);
    EXPECT_EQ(result->response.full_hashes(0).full_hash(),
              "aaaa1111111111111111111111111111");
  }
}

TEST_F(V5SearchHashesUtilTest, DetermineMostSevereThreat_MultipleDetails) {
  V5::FullHash::FullHashDetail detail_low =
      CreateHashDetail(V5::ThreatType::TRICK_TO_BILL);  // Severity 15
  V5::FullHash::FullHashDetail detail_med = CreateHashDetail(
      V5::ThreatType::SOCIAL_ENGINEERING,
      {V5::ThreatAttribute::CANARY});  // Severity 4 (Suspicious Site)
  V5::FullHash::FullHashDetail detail_high =
      CreateHashDetail(V5::ThreatType::MALWARE);  // Severity 0 (most severe)

  // 1. High severity wins when placed at different positions in a 3-item list
  EXPECT_EQ(DetermineMostSevereThreat({&detail_low, &detail_med, &detail_high}),
            DetermineMostSevereThreat({&detail_high}));
  EXPECT_EQ(DetermineMostSevereThreat({&detail_high, &detail_med, &detail_low}),
            DetermineMostSevereThreat({&detail_high}));
  EXPECT_EQ(DetermineMostSevereThreat({&detail_low, &detail_high, &detail_med}),
            DetermineMostSevereThreat({&detail_high}));

  // 2. Tie breaker: first one wins if severities are equal (using 3-item lists)
  V5::FullHash::FullHashDetail detail_high2 =
      CreateHashDetail(V5::ThreatType::SOCIAL_ENGINEERING);  // Severity 0

  // both are severity 0, but detail_high is first
  EXPECT_EQ(
      DetermineMostSevereThreat({&detail_low, &detail_high, &detail_high2}),
      DetermineMostSevereThreat({&detail_high}));
  // detail_high2 is first
  EXPECT_EQ(
      DetermineMostSevereThreat({&detail_low, &detail_high2, &detail_high}),
      DetermineMostSevereThreat({&detail_high2}));
}

TEST_F(V5SearchHashesUtilTest,
       DetermineMostSevereThreat_ExhaustiveMappingAndSeverity) {
  auto create_metadata = [](SubresourceFilterType type,
                            SubresourceFilterLevel level) {
    ThreatMetadata meta;
    meta.subresource_filter_match[type] = level;
    return meta;
  };

  struct ThreatConfig {
    V5::ThreatType v5_type;
    std::vector<V5::ThreatAttribute> attributes;
    SBThreatType expected_sb_type;
    int severity_rank;
    ThreatMetadata expected_metadata;
  };

  std::vector<ThreatConfig> configs = {
      {V5::ThreatType::MALWARE,
       {},
       SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
       0,
       ThreatMetadata()},
      {V5::ThreatType::SOCIAL_ENGINEERING,
       {},
       SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
       0,
       ThreatMetadata()},
      {V5::ThreatType::MALICIOUS_BINARY,
       {},
       SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE,
       0,
       ThreatMetadata()},
      {V5::ThreatType::UNWANTED_SOFTWARE,
       {},
       SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
       1,
       ThreatMetadata()},
      {V5::ThreatType::BETTER_ADS_VIOLATION,
       {},
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       2,
       create_metadata(SubresourceFilterType::BETTER_ADS,
                       SubresourceFilterLevel::ENFORCE)},
      {V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       {},
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       3,
       create_metadata(SubresourceFilterType::ABUSIVE,
                       SubresourceFilterLevel::ENFORCE)},
      {V5::ThreatType::BETTER_ADS_VIOLATION,
       {V5::ThreatAttribute::CANARY},
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       4,
       create_metadata(SubresourceFilterType::BETTER_ADS,
                       SubresourceFilterLevel::WARN)},
      {V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       {V5::ThreatAttribute::CANARY},
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       5,
       create_metadata(SubresourceFilterType::ABUSIVE,
                       SubresourceFilterLevel::WARN)},
      {V5::ThreatType::NOTIFICATION_ABUSE,
       {},
       SBThreatType::SB_THREAT_TYPE_API_ABUSE,
       2,
       ThreatMetadata()},
      {V5::ThreatType::SOCIAL_ENGINEERING,
       {V5::ThreatAttribute::CANARY},
       SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE,
       7,
       ThreatMetadata()},
      {V5::ThreatType::TRICK_TO_BILL,
       {},
       SBThreatType::SB_THREAT_TYPE_BILLING,
       15,
       ThreatMetadata()},
      // Canary variants that shouldn't change mapping/severity (except social
      // engineering and subresource filter)
      {V5::ThreatType::MALWARE,
       {V5::ThreatAttribute::CANARY},
       SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
       0,
       ThreatMetadata()},
  };

  // Verify mapping and severity.
  for (const auto& config1 : configs) {
    V5::FullHash::FullHashDetail detail1 =
        CreateHashDetail(config1.v5_type, config1.attributes);
    ThreatResult res1 = DetermineMostSevereThreat({&detail1});
    EXPECT_EQ(res1.threat_type, config1.expected_sb_type);
    EXPECT_EQ(res1.metadata, config1.expected_metadata);
    for (const auto& config2 : configs) {
      SCOPED_TRACE(base::StringPrintf(
          "Severity/Mapping test: config1 (v5_type=%d, rank=%d) vs config2 "
          "(v5_type=%d, "
          "rank=%d)",
          static_cast<int>(config1.v5_type), config1.severity_rank,
          static_cast<int>(config2.v5_type), config2.severity_rank));

      // Pairs of subresource filter threats merge metadata and are tested
      // in DetermineMostSevereThreat_SubresourceFilterMetadataMerging.
      if (config1.expected_sb_type ==
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER &&
          config2.expected_sb_type ==
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
        continue;
      }

      V5::FullHash::FullHashDetail detail2 =
          CreateHashDetail(config2.v5_type, config2.attributes);

      // Verify severity (pairwise)
      ThreatResult res1_2 = DetermineMostSevereThreat({&detail1, &detail2});
      ThreatResult res2_1 = DetermineMostSevereThreat({&detail2, &detail1});
      ThreatResult res2 = DetermineMostSevereThreat({&detail2});

      if (config1.severity_rank < config2.severity_rank) {
        // config1 is more severe. config1 must win.
        EXPECT_EQ(res1_2, res1);
        EXPECT_EQ(res2_1, res1);
      } else if (config1.severity_rank > config2.severity_rank) {
        // config2 is more severe. config2 must win.
        EXPECT_EQ(res1_2, res2);
        EXPECT_EQ(res2_1, res2);
      } else {
        // Equal severity. First one wins.
        EXPECT_EQ(res1_2, res1);
        EXPECT_EQ(res2_1, res2);
      }
    }
  }
}

TEST_F(V5SearchHashesUtilTest,
       DetermineMostSevereThreat_SubresourceFilterMetadataMerging) {
  V5::FullHash::FullHashDetail bas_enforce =
      CreateHashDetail(V5::ThreatType::BETTER_ADS_VIOLATION);
  V5::FullHash::FullHashDetail bas_warn = CreateHashDetail(
      V5::ThreatType::BETTER_ADS_VIOLATION, {V5::ThreatAttribute::CANARY});
  V5::FullHash::FullHashDetail abs_enforce =
      CreateHashDetail(V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION);
  V5::FullHash::FullHashDetail soceng =
      CreateHashDetail(V5::ThreatType::SOCIAL_ENGINEERING);

  // 1. Both BETTER_ADS and ABUSIVE with ENFORCE -> both present.
  {
    ThreatResult result =
        DetermineMostSevereThreat({&bas_enforce, &abs_enforce});
    EXPECT_EQ(result.threat_type,
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
    EXPECT_THAT(result.metadata.subresource_filter_match,
                testing::UnorderedElementsAre(
                    std::make_pair(SubresourceFilterType::BETTER_ADS,
                                   SubresourceFilterLevel::ENFORCE),
                    std::make_pair(SubresourceFilterType::ABUSIVE,
                                   SubresourceFilterLevel::ENFORCE)));
  }

  // 2. BETTER_ADS WARN and ABUSIVE ENFORCE -> both present.
  {
    ThreatResult result = DetermineMostSevereThreat({&bas_warn, &abs_enforce});
    EXPECT_EQ(result.threat_type,
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
    EXPECT_THAT(result.metadata.subresource_filter_match,
                testing::UnorderedElementsAre(
                    std::make_pair(SubresourceFilterType::BETTER_ADS,
                                   SubresourceFilterLevel::WARN),
                    std::make_pair(SubresourceFilterType::ABUSIVE,
                                   SubresourceFilterLevel::ENFORCE)));
  }

  // 3. Same threat type with different levels -> ENFORCE replaces WARN.
  {
    ThreatResult result = DetermineMostSevereThreat({&bas_warn, &bas_enforce});
    EXPECT_EQ(result.threat_type,
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
    EXPECT_THAT(result.metadata.subresource_filter_match,
                testing::UnorderedElementsAre(
                    std::make_pair(SubresourceFilterType::BETTER_ADS,
                                   SubresourceFilterLevel::ENFORCE)));
  }

  // 4. Same as #3 above but opposite call order.
  {
    ThreatResult result = DetermineMostSevereThreat({&bas_enforce, &bas_warn});
    EXPECT_EQ(result.threat_type,
              SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
    EXPECT_THAT(result.metadata.subresource_filter_match,
                testing::UnorderedElementsAre(
                    std::make_pair(SubresourceFilterType::BETTER_ADS,
                                   SubresourceFilterLevel::ENFORCE)));
  }

  // 5. Subresource filter and higher-severity threat (SOCIAL_ENGINEERING) ->
  // SOCIAL_ENGINEERING wins and metadata is empty.
  {
    ThreatResult result =
        DetermineMostSevereThreat({&bas_enforce, &abs_enforce, &soceng});
    EXPECT_EQ(result.threat_type, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(result.metadata, ThreatMetadata());
  }
}

}  // namespace safe_browsing::v5_search_hashes_util
