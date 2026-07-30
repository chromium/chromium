// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"

#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/base64url.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class V5GetHashProtocolManagerTest : public ::testing::Test {
 protected:
  using OperationOutcome = V5GetHashProtocolManager::OperationOutcome;

  V5GetHashProtocolManagerTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        cache_(std::make_unique<V5SearchHashesCache>(
            /*history_service=*/nullptr)) {
    feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
  }

  std::unique_ptr<V5GetHashProtocolManager> CreateProtocolManager() {
    // TODO(crbug.com/362791941): Handle v4 references.
    return std::make_unique<V5GetHashProtocolManager>(
        test_shared_loader_factory_, GetTestV4ProtocolConfig(), cache_.get());
  }

  std::string GetExpectedRequestUrl(std::vector<std::string> prefixes) {
    std::ranges::sort(prefixes);
    prefixes.erase(std::ranges::unique(prefixes).begin(), prefixes.end());
    V5::SearchHashesRequest request;
    for (const auto& prefix : prefixes) {
      request.add_hash_prefixes(prefix);
    }
    std::string request_data, request_base64;
    request.SerializeToString(&request_data);
    base::Base64UrlEncode(request_data,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &request_base64);
    std::string url = base::StrCat(
        {"https://safebrowsing.googleapis.com/v5/hashes:search?$req=",
         request_base64, "&$ct=application/x-protobuf"});
    auto api_key = google_apis::GetAPIKey();
    if (!api_key.empty()) {
      base::StringAppendF(
          &url, "&key=%s",
          base::EscapeQueryParamValue(api_key, /*use_plus=*/true).c_str());
    }
    return url;
  }

  std::string GetExpectedRequestUrl(const std::string& prefix) {
    return GetExpectedRequestUrl(std::vector<std::string>{prefix});
  }

  V5::FullHash CreateFullHashProto(
      const std::string& full_hash,
      std::vector<V5::ThreatType> threat_types,
      std::optional<std::vector<std::vector<V5::ThreatAttribute>>>
          threat_attributes) {
    if (threat_attributes.has_value()) {
      EXPECT_EQ(threat_attributes->size(), threat_types.size());
    }
    auto full_hash_proto = V5::FullHash();
    full_hash_proto.set_full_hash(full_hash);
    for (size_t i = 0u; i < threat_types.size(); ++i) {
      auto* details = full_hash_proto.add_full_hash_details();
      details->set_threat_type(threat_types[i]);
      if (threat_attributes.has_value()) {
        for (const auto& attribute : threat_attributes.value()[i]) {
          details->add_attributes(attribute);
        }
      }
    }
    return full_hash_proto;
  }

  std::string CreateSerializedResponse(
      const std::vector<V5::FullHash>& full_hashes) {
    V5::SearchHashesResponse response;
    response.mutable_full_hashes()->Assign(full_hashes.begin(),
                                           full_hashes.end());
    V5::Duration* cache_duration = response.mutable_cache_duration();
    cache_duration->set_seconds(300);
    std::string response_str;
    response.SerializeToString(&response_str);
    return response_str;
  }

  void SetUpDefaultLookupResponse(
      const std::string& request_url,
      const std::vector<V5::FullHash>& full_hashes) {
    test_url_loader_factory_.AddResponse(request_url,
                                         CreateSerializedResponse(full_hashes));
  }

  void SimulatePendingLookupResponse(
      const std::string& request_url,
      const std::vector<V5::FullHash>& full_hashes) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        request_url, CreateSerializedResponse(full_hashes));
  }

  void ResetMetrics() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void CheckCacheHitMetrics(bool expect_cache_hit) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.CacheHitAllPrefixes",
        /*sample=*/expect_cache_hit,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.SBGetHash.CacheHitAllPrefixes",
        /*sample=*/expect_cache_hit,
        /*expected_bucket_count=*/1);
  }

  void CheckRequestMetrics(int expected_prefix_count,
                           int expected_network_result) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.Request.CountOfPrefixes",
        /*sample=*/expected_prefix_count,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.SBGetHash.Request.CountOfPrefixes",
        /*sample=*/expected_prefix_count,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.Network.Result",
        /*sample=*/expected_network_result,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.SBGetHash.Network.Result",
        /*sample=*/expected_network_result,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.V5GetHash.Network.Time",
                                        1);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.SBGetHash.Network.Time",
                                        1);
  }

  void CheckNoNetworkMetrics() {
    histogram_tester_->ExpectTotalCount(
        "SafeBrowsing.V5GetHash.Request.CountOfPrefixes", 0);
    histogram_tester_->ExpectTotalCount(
        "SafeBrowsing.SBGetHash.Request.CountOfPrefixes", 0);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.V5GetHash.Network.Result",
                                        0);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.SBGetHash.Network.Result",
                                        0);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.V5GetHash.Network.Time",
                                        0);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.SBGetHash.Network.Time",
                                        0);
  }

  void CheckOperationOutcome(OperationOutcome expected_outcome) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.OperationOutcome",
        /*sample=*/static_cast<int>(expected_outcome),
        /*expected_bucket_count=*/1);
  }

  void CheckThreatInfoSize(int expected_size) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.ThreatInfoSize",
        /*sample=*/expected_size,
        /*expected_bucket_count=*/1);
  }

  void CheckParseFailureReason(
      v5_search_hashes_util::ParseFailure expected_reason) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.ParseFailureReason",
        /*sample=*/static_cast<int>(expected_reason),
        /*expected_bucket_count=*/1);
  }

  void CheckFoundUnmatchedFullHashes(bool expected) {
    histogram_tester_->ExpectUniqueSample(
        "SafeBrowsing.V5GetHash.FoundUnmatchedFullHashes",
        /*sample=*/expected,
        /*expected_bucket_count=*/1);
  }

  void CheckSuccessTestLogs(int expected_prefix_count,
                            int expected_threat_info_size,
                            bool expected_found_unmatched_full_hashes) {
    CheckCacheHitMetrics(/*expect_cache_hit=*/false);
    CheckRequestMetrics(expected_prefix_count, /*expected_network_result=*/200);
    CheckOperationOutcome(OperationOutcome::kSuccess);
    CheckThreatInfoSize(expected_threat_info_size);
    CheckFoundUnmatchedFullHashes(expected_found_unmatched_full_hashes);
    ResetMetrics();
  }

  void CheckFullyCachedTestLogs(int expected_threat_info_size) {
    CheckCacheHitMetrics(/*expect_cache_hit=*/true);
    CheckNoNetworkMetrics();
    CheckOperationOutcome(OperationOutcome::kLocalCacheHit);
    CheckThreatInfoSize(expected_threat_info_size);
    ResetMetrics();
  }

  void CheckBackoffTestLogs() {
    CheckNoNetworkMetrics();
    CheckOperationOutcome(OperationOutcome::kBackoffError);
    ResetMetrics();
  }

  void CheckFailureTestLogs(
      int expected_prefix_count,
      int net_error,
      int response_code,
      OperationOutcome expected_outcome,
      v5_search_hashes_util::ParseFailure expected_parse_failure) {
    CheckCacheHitMetrics(/*expect_cache_hit=*/false);
    CheckRequestMetrics(expected_prefix_count,
                        net_error == net::OK ? response_code : net_error);
    CheckOperationOutcome(expected_outcome);
    CheckParseFailureReason(expected_parse_failure);
    ResetMetrics();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<V5SearchHashesCache> cache_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
};

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_OneHash_Safe) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));
  SetUpDefaultLookupResponse(expected_url, /*full_hashes=*/{});

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/0,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_OneHash_Threat) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(full_hash, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/1,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_MultipleHashes_MostSevere) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash1("11111111111111111111111111111111");
  FullHashStr hash2("22222222222222222222222222222222");

  // We request multiple hashes in a single call.
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[hash1] = {
      SBThreatType::SB_THREAT_TYPE_URL_UNWANTED};
  full_hash_to_threat_types[hash2] = {SBThreatType::SB_THREAT_TYPE_URL_MALWARE};

  // Expected URL contains both sorted prefixes: 1111 and 2222.
  std::string expected_url =
      GetExpectedRequestUrl({SBProtocolManagerUtil::GetHashPrefix(hash1),
                             SBProtocolManagerUtil::GetHashPrefix(hash2)});

  // Server returns matches for both.
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(hash1, {V5::ThreatType::UNWANTED_SOFTWARE},
                          /*threat_attributes=*/std::nullopt),
      CreateFullHashProto(hash2, {V5::ThreatType::MALWARE},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  // MALWARE is more severe than UNWANTED_SOFTWARE, so it should be returned.
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/2,
                       /*expected_threat_info_size=*/2,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_OneHash_MultipleThreats_MostSevere) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  // We check for both unwanted software and malware.
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
      SBThreatType::SB_THREAT_TYPE_URL_MALWARE};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  // Server returns both MALWARE and UNWANTED_SOFTWARE for this single hash.
  std::vector<V5::FullHash> full_hashes = {CreateFullHashProto(
      full_hash, {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::MALWARE},
      /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  // MALWARE (severity 0) is more severe than UNWANTED_SOFTWARE (severity 1),
  // so it should be returned.
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/2,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_Cached) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(full_hash, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  // 1. First request should hit network and cache the result.
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  test_url_loader_factory_.ClearResponses();

  // 2. Second request should hit cache and NOT network.
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckFullyCachedTestLogs(/*expected_threat_info_size=*/1);
  }
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_PartialCached_CachedIsMoreSevere) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash_cached("11111111111111111111111111111111");
  FullHashStr hash_network("22222222222222222222222222222222");

  // 1. Cache hash_cached as MALWARE (severity 0) via a real request.
  {
    std::map<FullHashStr, std::vector<SBThreatType>> cache_request;
    cache_request[hash_cached] = {SBThreatType::SB_THREAT_TYPE_URL_MALWARE};
    std::string expected_url = GetExpectedRequestUrl(
        SBProtocolManagerUtil::GetHashPrefix(hash_cached));
    std::vector<V5::FullHash> response = {
        CreateFullHashProto(hash_cached, {V5::ThreatType::MALWARE},
                            /*threat_attributes=*/std::nullopt)};
    SetUpDefaultLookupResponse(expected_url, response);

    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(cache_request, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  test_url_loader_factory_.ClearResponses();

  // 2. Request both. Network should only be hit for hash_network.
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[hash_cached] = {
      SBThreatType::SB_THREAT_TYPE_URL_MALWARE};
  full_hash_to_threat_types[hash_network] = {
      SBThreatType::SB_THREAT_TYPE_URL_UNWANTED};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash_network));
  std::vector<V5::FullHash> network_response = {
      CreateFullHashProto(hash_network, {V5::ThreatType::UNWANTED_SOFTWARE},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, network_response);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  // MALWARE (cached, sev 0) is more severe than UNWANTED_SOFTWARE (network, sev
  // 1).
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/2,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_PartialCached_NetworkIsMoreSevere) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash_cached("11111111111111111111111111111111");
  FullHashStr hash_network("22222222222222222222222222222222");

  // 1. Cache hash_cached as UNWANTED_SOFTWARE (severity 1) via a real request.
  {
    std::map<FullHashStr, std::vector<SBThreatType>> cache_request;
    cache_request[hash_cached] = {SBThreatType::SB_THREAT_TYPE_URL_UNWANTED};
    std::string expected_url = GetExpectedRequestUrl(
        SBProtocolManagerUtil::GetHashPrefix(hash_cached));
    std::vector<V5::FullHash> response = {
        CreateFullHashProto(hash_cached, {V5::ThreatType::UNWANTED_SOFTWARE},
                            /*threat_attributes=*/std::nullopt)};
    SetUpDefaultLookupResponse(expected_url, response);

    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(cache_request, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  test_url_loader_factory_.ClearResponses();

  // 2. Request both. Network should only be hit for hash_network.
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[hash_cached] = {
      SBThreatType::SB_THREAT_TYPE_URL_UNWANTED};
  full_hash_to_threat_types[hash_network] = {
      SBThreatType::SB_THREAT_TYPE_URL_MALWARE};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash_network));
  std::vector<V5::FullHash> network_response = {
      CreateFullHashProto(hash_network, {V5::ThreatType::MALWARE},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, network_response);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  // MALWARE (network, sev 0) is more severe than UNWANTED_SOFTWARE (cached, sev
  // 1).
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/2,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_Backoff) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  // 1. Trigger first failure to enter backoff (delay [15, 30] mins).
  test_url_loader_factory_.AddResponse(
      GURL(expected_url), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_CONNECTION_RESET,
                         /*response_code=*/0, OperationOutcome::kNetworkError,
                         v5_search_hashes_util::ParseFailure::kNetworkError);
  }

  // 2. Verify subsequent request is rejected immediately at T=0.
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }

  // 3. Fast forward by 14 minutes (less than min delay 15 mins). Request should
  // still be rejected.
  task_environment_.FastForwardBy(base::Minutes(14));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }

  // 4. Fast forward by another 17 minutes (total 31 minutes, which is > max
  // delay 30 mins). Request should be allowed. We trigger another failure.
  // Delay should increase to [30, 60] mins.
  task_environment_.FastForwardBy(base::Minutes(17));
  test_url_loader_factory_.AddResponse(
      GURL(expected_url), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_CONNECTION_RESET,
                         /*response_code=*/0, OperationOutcome::kNetworkError,
                         v5_search_hashes_util::ParseFailure::kNetworkError);
  }

  // 5. Fast forward by 29 minutes (less than min delay 30 mins). Request should
  // be rejected.
  task_environment_.FastForwardBy(base::Minutes(29));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }

  // 6. Fast forward by another 32 minutes (total 61 minutes since second
  // failure, > max delay 60 mins). Request should be allowed. We let it succeed
  // this time.
  task_environment_.FastForwardBy(base::Minutes(32));
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(full_hash, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  // 7. Success should reset backoff. Verify next request is allowed
  // immediately (using a different hash to avoid cache hit).
  FullHashStr full_hash2("56789012345678901234567890123456");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types2;
  full_hash_to_threat_types2[full_hash2] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url2 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash2));
  std::vector<V5::FullHash> full_hashes2 = {
      CreateFullHashProto(full_hash2, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};

  test_url_loader_factory_.ClearResponses();
  SetUpDefaultLookupResponse(expected_url2, full_hashes2);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types2, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest,
       TestNumRequestsSkippedDuringBackoffHistogram) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  // 1. Trigger first failure to enter backoff.
  test_url_loader_factory_.AddResponse(
      GURL(expected_url), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    std::ignore = future.Get();
  }

  // 2. Two requests during backoff should be rejected and count as skipped.
  test_url_loader_factory_.ClearResponses();
  for (int i = 0; i < 2; i++) {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    std::ignore = future.Get();
  }

  // Verify histograms are not logged yet before the success call.
  histogram_tester_->ExpectTotalCount(
      "SafeBrowsing.V5GetHash.NumRequestsSkippedDuringBackoff", 0);
  histogram_tester_->ExpectTotalCount(
      "SafeBrowsing.SBGetHash.Result.BackoffErrorCount", 0);

  // 3. Fast forward past backoff and let next request succeed.
  task_environment_.FastForwardBy(base::Minutes(30));
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(full_hash, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    std::ignore = future.Get();
  }

  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.V5GetHash.NumRequestsSkippedDuringBackoff",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.SBGetHash.Result.BackoffErrorCount",
      /*sample=*/2,
      /*expected_bucket_count=*/1);

  // Reset histogram tester to check the second backoff window independently.
  ResetMetrics();

  // 4. Trigger second failure to enter backoff again (using a new hash to avoid
  // cache hit).
  FullHashStr full_hash2("56789012345678901234567890123456");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types2;
  full_hash_to_threat_types2[full_hash2] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url2 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash2));

  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(
      GURL(expected_url2), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types2, future.GetCallback());
    std::ignore = future.Get();
  }

  // 5. One request during second backoff should be rejected and count as
  // skipped.
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types2, future.GetCallback());
    std::ignore = future.Get();
  }

  // 6. Fast forward past backoff and let next request succeed.
  task_environment_.FastForwardBy(base::Minutes(30));
  std::vector<V5::FullHash> full_hashes2 = {
      CreateFullHashProto(full_hash2, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url2, full_hashes2);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types2, future.GetCallback());
    std::ignore = future.Get();
  }

  // Histograms
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.V5GetHash.NumRequestsSkippedDuringBackoff",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.SBGetHash.Result.BackoffErrorCount",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_Backoff_RetriableErrors) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash1("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types1;
  full_hash_to_threat_types1[full_hash1] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url1 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash1));

  // 1. Trigger a retriable failure (ERR_NETWORK_CHANGED).
  test_url_loader_factory_.AddResponse(
      GURL(expected_url1), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types1, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_NETWORK_CHANGED,
                         /*response_code=*/0, OperationOutcome::kRetriableError,
                         v5_search_hashes_util::ParseFailure::kRetriableError);
  }

  // 2. Verify subsequent request is not blocked + succeeds with URL_PHISHING.
  FullHashStr full_hash2("23456789012345678901234567890123");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types2;
  full_hash_to_threat_types2[full_hash2] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url2 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash2));
  std::vector<V5::FullHash> full_hashes2 = {
      CreateFullHashProto(full_hash2, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};

  test_url_loader_factory_.ClearResponses();
  SetUpDefaultLookupResponse(expected_url2, full_hashes2);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types2, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  // 3. Trigger a real (non-retriable) error to enter backoff.
  FullHashStr full_hash3("34567890123456789012345678901234");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types3;
  full_hash_to_threat_types3[full_hash3] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url3 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash3));

  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(
      GURL(expected_url3), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types3, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_CONNECTION_RESET,
                         /*response_code=*/0, OperationOutcome::kNetworkError,
                         v5_search_hashes_util::ParseFailure::kNetworkError);
  }

  // 4. Verify we are now in backoff (returns safe, not phishing).
  FullHashStr full_hash4("45678901234567890123456789012345");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types4;
  full_hash_to_threat_types4[full_hash4] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types4, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }

  // 5. Fast forward 31 minutes to exit backoff (exceeds max delay 30 mins).
  task_environment_.FastForwardBy(base::Minutes(31));

  // 6. Trigger another retriable error after exiting backoff.
  FullHashStr full_hash5("56789012345678901234567890123456");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types5;
  full_hash_to_threat_types5[full_hash5] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url5 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash5));

  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(
      GURL(expected_url5), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types5, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_NETWORK_CHANGED,
                         /*response_code=*/0, OperationOutcome::kRetriableError,
                         v5_search_hashes_util::ParseFailure::kRetriableError);
  }

  // 7. Verify subsequent request is not blocked + succeeds with URL_PHISHING.
  FullHashStr full_hash6("67890123456789012345678901234567");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types6;
  full_hash_to_threat_types6[full_hash6] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
  std::string expected_url6 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash6));
  std::vector<V5::FullHash> full_hashes6 = {
      CreateFullHashProto(full_hash6, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};

  test_url_loader_factory_.ClearResponses();
  SetUpDefaultLookupResponse(expected_url6, full_hashes6);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types6, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_Metadata_SubresourceFilter_AbusiveEnforce) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER};
  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  std::vector<V5::FullHash> full_hashes = {CreateFullHashProto(
      full_hash, {V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION},
      /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  const ThreatMetadata& metadata = future.Get<1>();
  auto it =
      metadata.subresource_filter_match.find(SubresourceFilterType::ABUSIVE);
  ASSERT_NE(it, metadata.subresource_filter_match.end());
  EXPECT_EQ(it->second, SubresourceFilterLevel::ENFORCE);

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/1,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_Metadata_SubresourceFilter_BetterAdsWarn) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER};
  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  std::vector<std::vector<V5::ThreatAttribute>> attributes = {
      {V5::ThreatAttribute::CANARY}};
  std::vector<V5::FullHash> full_hashes = {CreateFullHashProto(
      full_hash, {V5::ThreatType::BETTER_ADS_VIOLATION}, attributes)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/0,
                       /*expected_found_unmatched_full_hashes=*/false);
#else
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  const ThreatMetadata& metadata = future.Get<1>();
  auto it =
      metadata.subresource_filter_match.find(SubresourceFilterType::BETTER_ADS);
  ASSERT_NE(it, metadata.subresource_filter_match.end());
  EXPECT_EQ(it->second, SubresourceFilterLevel::WARN);

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/1,
                       /*expected_found_unmatched_full_hashes=*/false);
#endif
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_AllThreatTypes) {
  struct TestCase {
    V5::ThreatType v5_type;
    SBThreatType sb_type;
  } test_cases[] = {
      {V5::ThreatType::SOCIAL_ENGINEERING,
       SBThreatType::SB_THREAT_TYPE_URL_PHISHING},
      {V5::ThreatType::MALWARE, SBThreatType::SB_THREAT_TYPE_URL_MALWARE},
      {V5::ThreatType::UNWANTED_SOFTWARE,
       SBThreatType::SB_THREAT_TYPE_URL_UNWANTED},
      {V5::ThreatType::MALICIOUS_BINARY,
       SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE},
      {V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER},
      {V5::ThreatType::BETTER_ADS_VIOLATION,
       SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER},
      {V5::ThreatType::TRICK_TO_BILL, SBThreatType::SB_THREAT_TYPE_BILLING},
      {V5::ThreatType::NOTIFICATION_ABUSE,
       SBThreatType::SB_THREAT_TYPE_API_ABUSE},
  };

  for (const auto& tc : test_cases) {
    cache_ = std::make_unique<V5SearchHashesCache>(/*history_service=*/nullptr);
    std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();
    test_url_loader_factory_.ClearResponses();

    FullHashStr full_hash("12345678901234567890123456789012");
    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
    full_hash_to_threat_types[full_hash] = {tc.sb_type};

    std::string expected_url =
        GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));
    std::vector<V5::FullHash> full_hashes = {
        CreateFullHashProto(full_hash, {tc.v5_type},
                            /*threat_attributes=*/std::nullopt)};
    SetUpDefaultLookupResponse(expected_url, full_hashes);

    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

    EXPECT_EQ(future.Get<0>(), tc.sb_type);
    const ThreatMetadata& metadata = future.Get<1>();
    if (tc.sb_type != SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
      EXPECT_EQ(metadata, ThreatMetadata());
    } else if (tc.v5_type == V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION) {
      auto it = metadata.subresource_filter_match.find(
          SubresourceFilterType::ABUSIVE);
      ASSERT_NE(it, metadata.subresource_filter_match.end());
      EXPECT_EQ(it->second, SubresourceFilterLevel::ENFORCE);
    } else if (tc.v5_type == V5::ThreatType::BETTER_ADS_VIOLATION) {
      auto it = metadata.subresource_filter_match.find(
          SubresourceFilterType::BETTER_ADS);
      ASSERT_NE(it, metadata.subresource_filter_match.end());
      EXPECT_EQ(it->second, SubresourceFilterLevel::ENFORCE);
    } else {
      NOTREACHED();
    }

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_HTTPError500_TriggersBackoff) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  // 1. Trigger HTTP 500 error.
  test_url_loader_factory_.AddResponse(expected_url, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::OK,
                         /*response_code=*/500, OperationOutcome::kHttpError,
                         v5_search_hashes_util::ParseFailure::kHttpError);
  }

  // 2. Verify subsequent request is blocked by backoff.
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_ParallelRequests) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash1("11111111111111111111111111111111");
  FullHashStr hash2("22222222222222222222222222222222");
  FullHashStr hash3("33333333333333333333333333333333");

  std::map<FullHashStr, std::vector<SBThreatType>> req1;
  req1[hash1] = {SBThreatType::SB_THREAT_TYPE_URL_MALWARE};

  std::map<FullHashStr, std::vector<SBThreatType>> req2;
  req2[hash2] = {SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::map<FullHashStr, std::vector<SBThreatType>> req3;
  req3[hash3] = {SBThreatType::SB_THREAT_TYPE_URL_UNWANTED};

  std::string expected_url1 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash1));
  std::string expected_url2 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash2));
  std::string expected_url3 =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash3));

  // Start requests A, B, C. They should all hit the network.
  base::test::TestFuture<SBThreatType, const ThreatMetadata&> futureA;
  pm->GetFullHashes(req1, futureA.GetCallback());

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> futureB;
  pm->GetFullHashes(req2, futureB.GetCallback());

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> futureC;
  pm->GetFullHashes(req3, futureC.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);

  // 1. Resolve C (Success, returns UNWANTED_SOFTWARE).
  SimulatePendingLookupResponse(
      expected_url3,
      {CreateFullHashProto(hash3, {V5::ThreatType::UNWANTED_SOFTWARE},
                           /*threat_attributes=*/std::nullopt)});
  EXPECT_EQ(futureC.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
  EXPECT_EQ(futureC.Get<1>(), ThreatMetadata());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  // 2. Resolve A (Success, returns MALWARE).
  SimulatePendingLookupResponse(
      expected_url1, {CreateFullHashProto(hash1, {V5::ThreatType::MALWARE},
                                          /*threat_attributes=*/std::nullopt)});
  EXPECT_EQ(futureA.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(futureA.Get<1>(), ThreatMetadata());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // 3. Resolve B (Success, returns SOCIAL_ENGINEERING).
  SimulatePendingLookupResponse(
      expected_url2,
      {CreateFullHashProto(hash2, {V5::ThreatType::SOCIAL_ENGINEERING},
                           /*threat_attributes=*/std::nullopt)});
  EXPECT_EQ(futureB.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_EQ(futureB.Get<1>(), ThreatMetadata());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Verify all metrics at the end.
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.V5GetHash.Request.CountOfPrefixes",
      /*sample=*/1,
      /*expected_bucket_count=*/3);
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.SBGetHash.Request.CountOfPrefixes",
      /*sample=*/1,
      /*expected_bucket_count=*/3);

  histogram_tester_->ExpectUniqueSample("SafeBrowsing.V5GetHash.Network.Result",
                                        /*sample=*/200,
                                        /*expected_bucket_count=*/3);
  histogram_tester_->ExpectUniqueSample("SafeBrowsing.SBGetHash.Network.Result",
                                        /*sample=*/200,
                                        /*expected_bucket_count=*/3);

  histogram_tester_->ExpectTotalCount("SafeBrowsing.V5GetHash.Network.Time", 3);
  histogram_tester_->ExpectTotalCount("SafeBrowsing.SBGetHash.Network.Time", 3);

  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.V5GetHash.CacheHitAllPrefixes",
      /*sample=*/false,
      /*expected_bucket_count=*/3);
  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.SBGetHash.CacheHitAllPrefixes",
      /*sample=*/false,
      /*expected_bucket_count=*/3);

  histogram_tester_->ExpectUniqueSample(
      "SafeBrowsing.V5GetHash.OperationOutcome",
      /*sample=*/static_cast<int>(OperationOutcome::kSuccess),
      /*expected_bucket_count=*/3);

  histogram_tester_->ExpectUniqueSample("SafeBrowsing.V5GetHash.ThreatInfoSize",
                                        /*sample=*/1,
                                        /*expected_bucket_count=*/3);
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_Backoff_CapsAt24Hours) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr full_hash("12345678901234567890123456789012");
  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[full_hash] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  // Trigger 10 failures. We need to fast forward past the max backoff delay
  // each time to be allowed to send the next request. Max delays for
  // failures 1..10 are: 30m, 60m, 120m, 240m, 480m, 960m, and capped at 1440m
  // (24 hours).
  std::array<int, 10> wait_times_mins = {31,  61,   121,  241,  481,
                                         961, 1441, 1441, 1441, 1441};

  for (int i = 0; i < 10; ++i) {
    test_url_loader_factory_.ClearResponses();
    test_url_loader_factory_.AddResponse(
        GURL(expected_url), network::mojom::URLResponseHead::New(), "",
        network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
    {
      base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
      pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
      EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
      EXPECT_EQ(future.Get<1>(), ThreatMetadata());
      EXPECT_EQ(test_url_loader_factory_.total_requests(), i + 1u);

      CheckFailureTestLogs(/*expected_prefix_count=*/1,
                           net::ERR_CONNECTION_RESET, /*response_code=*/0,
                           OperationOutcome::kNetworkError,
                           v5_search_hashes_util::ParseFailure::kNetworkError);
    }
    // Fast forward to exit backoff for the next request.
    task_environment_.FastForwardBy(base::Minutes(wait_times_mins[i]));
  }

  // Now we have had 10 failures. The next backoff delay should be capped at 24
  // hours. Trigger 11th failure to enter backoff again.
  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(
      GURL(expected_url), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_CONNECTION_RESET,
                         /*response_code=*/0, OperationOutcome::kNetworkError,
                         v5_search_hashes_util::ParseFailure::kNetworkError);
  }

  // Verify we are blocked at 23 hours 59 minutes.
  task_environment_.FastForwardBy(base::Hours(23) + base::Minutes(59));
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckBackoffTestLogs();
  }

  // Fast forward another 2 minutes (total 24 hours 1 minute since 11th
  // failure). Request should be allowed. We let it succeed.
  task_environment_.FastForwardBy(base::Minutes(2));
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(full_hash, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_RelevanceFiltering) {
  FullHashStr full_hash("12345678901234567890123456789012");
  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  struct TestCase {
    std::vector<V5::ThreatType> response_types;
    std::vector<std::vector<V5::ThreatAttribute>> response_attrs;
    std::vector<SBThreatType> requested_types;
    SBThreatType expected_result;
  } test_cases[] = {
      // 1. CANARY + FRAME_ONLY on SOCIAL_ENGINEERING -> Ignored.
      {{V5::ThreatType::SOCIAL_ENGINEERING},
       {{V5::ThreatAttribute::CANARY, V5::ThreatAttribute::FRAME_ONLY}},
       {SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
        SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE},
       SBThreatType::SB_THREAT_TYPE_SAFE},

      // 2. MALWARE with CANARY -> Ignored (Malware only supports enforcement).
      {{V5::ThreatType::MALWARE},
       {{V5::ThreatAttribute::CANARY}},
       {SBThreatType::SB_THREAT_TYPE_URL_MALWARE},
       SBThreatType::SB_THREAT_TYPE_SAFE},

      // 3. Potentially Harmful Application (PHA) -> Ignored (Not supported for
      // local DB).
      {{V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION},
       {{}},
       {SBThreatType::SB_THREAT_TYPE_URL_MALWARE},
       SBThreatType::SB_THREAT_TYPE_SAFE},

      // 4. SOCIAL_ENGINEERING with CANARY -> Allowed if SUSPICIOUS_SITE
      // requested. Never allowed on iOS.
      {{V5::ThreatType::SOCIAL_ENGINEERING},
       {{V5::ThreatAttribute::CANARY}},
       {SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE},
#if BUILDFLAG(IS_IOS)
       SBThreatType::SB_THREAT_TYPE_SAFE
#else
       SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE
#endif
      },

      // 5. SOCIAL_ENGINEERING with CANARY -> Ignored if only PHISHING
      // requested.
      {{V5::ThreatType::SOCIAL_ENGINEERING},
       {{V5::ThreatAttribute::CANARY}},
       {SBThreatType::SB_THREAT_TYPE_URL_PHISHING},
       SBThreatType::SB_THREAT_TYPE_SAFE},
  };

  int idx = 0;
  for (const auto& tc : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Test case index: %d", idx++));
    cache_ = std::make_unique<V5SearchHashesCache>(/*history_service=*/nullptr);
    std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();
    test_url_loader_factory_.ClearResponses();

    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
    full_hash_to_threat_types[full_hash] = tc.requested_types;

    std::vector<V5::FullHash> full_hashes = {
        CreateFullHashProto(full_hash, tc.response_types, tc.response_attrs)};
    SetUpDefaultLookupResponse(expected_url, full_hashes);

    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

    EXPECT_EQ(future.Get<0>(), tc.expected_result);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    int expected_threat_info_size =
        tc.expected_result == SBThreatType::SB_THREAT_TYPE_SAFE ? 0 : 1;
    CheckSuccessTestLogs(/*expected_prefix_count=*/1, expected_threat_info_size,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_CachedResultsAllowedInBackoff) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash_cached("11111111111111111111111111111111");
  FullHashStr hash_network("22222222222222222222222222222222");

  std::map<FullHashStr, std::vector<SBThreatType>> req_cached;
  req_cached[hash_cached] = {SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::map<FullHashStr, std::vector<SBThreatType>> req_network;
  req_network[hash_network] = {SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url_cached =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash_cached));
  std::string expected_url_network =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(hash_network));

  // 1. Cache hash_cached.
  std::vector<V5::FullHash> full_hashes_cached = {
      CreateFullHashProto(hash_cached, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url_cached, full_hashes_cached);
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(req_cached, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/1,
                         /*expected_found_unmatched_full_hashes=*/false);
  }

  // 2. Trigger backoff using hash_network.
  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(
      GURL(expected_url_network), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(req_network, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckFailureTestLogs(/*expected_prefix_count=*/1, net::ERR_CONNECTION_RESET,
                         /*response_code=*/0, OperationOutcome::kNetworkError,
                         v5_search_hashes_util::ParseFailure::kNetworkError);
  }

  // 3. Request hash_cached again. It should return PHISHING from cache
  // even though we are in backoff, and no network request should be sent.
  test_url_loader_factory_.ClearResponses();
  {
    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(req_cached, future.GetCallback());
    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    CheckFullyCachedTestLogs(/*expected_threat_info_size=*/1);
  }
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_PrefixCollision_Ignored) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  // Both hashes have the same prefix "1234" (first 4 bytes).
  FullHashStr hash_requested("12345678901234567890123456789012");
  FullHashStr hash_returned("12349999999999999999999999999999");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[hash_requested] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url = GetExpectedRequestUrl(
      SBProtocolManagerUtil::GetHashPrefix(hash_requested));

  // Server returns match for hash_returned, not hash_requested.
  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(hash_returned, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  // Should return SAFE because the returned full hash didn't match the
  // requested one.
  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/0,
                       /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(V5GetHashProtocolManagerTest,
       GetFullHashes_DifferentThreatType_Ignored) {
  struct TestCase {
    SBThreatType requested_type;
    V5::ThreatType returned_type;
  } test_cases[] = {
      // 1. Requested MALWARE (high), returned UNWANTED (low) -> expect SAFE.
      {SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
       V5::ThreatType::UNWANTED_SOFTWARE},
      // 2. Requested UNWANTED (low), returned MALWARE (high) -> expect SAFE.
      {SBThreatType::SB_THREAT_TYPE_URL_UNWANTED, V5::ThreatType::MALWARE},
  };

  FullHashStr full_hash("12345678901234567890123456789012");
  std::string expected_url =
      GetExpectedRequestUrl(SBProtocolManagerUtil::GetHashPrefix(full_hash));

  for (const auto& tc : test_cases) {
    cache_ = std::make_unique<V5SearchHashesCache>(/*history_service=*/nullptr);
    std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();
    test_url_loader_factory_.ClearResponses();

    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
    full_hash_to_threat_types[full_hash] = {tc.requested_type};

    std::vector<V5::FullHash> full_hashes = {
        CreateFullHashProto(full_hash, {tc.returned_type},
                            /*threat_attributes=*/std::nullopt)};
    SetUpDefaultLookupResponse(expected_url, full_hashes);

    base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
    pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

    EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
    EXPECT_EQ(future.Get<1>(), ThreatMetadata());

    CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                         /*expected_threat_info_size=*/0,
                         /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(V5GetHashProtocolManagerTest, GetFullHashes_UnmatchedPrefix_Ignored) {
  std::unique_ptr<V5GetHashProtocolManager> pm = CreateProtocolManager();

  FullHashStr hash_requested("11111111111111111111111111111111");
  FullHashStr hash_returned("22222222222222222222222222222222");

  std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
  full_hash_to_threat_types[hash_requested] = {
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING};

  std::string expected_url = GetExpectedRequestUrl(
      SBProtocolManagerUtil::GetHashPrefix(hash_requested));

  std::vector<V5::FullHash> full_hashes = {
      CreateFullHashProto(hash_returned, {V5::ThreatType::SOCIAL_ENGINEERING},
                          /*threat_attributes=*/std::nullopt)};
  SetUpDefaultLookupResponse(expected_url, full_hashes);

  base::test::TestFuture<SBThreatType, const ThreatMetadata&> future;
  pm->GetFullHashes(full_hash_to_threat_types, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), SBThreatType::SB_THREAT_TYPE_SAFE);
  EXPECT_EQ(future.Get<1>(), ThreatMetadata());

  CheckSuccessTestLogs(/*expected_prefix_count=*/1,
                       /*expected_threat_info_size=*/0,
                       /*expected_found_unmatched_full_hashes=*/true);
}

}  // namespace safe_browsing
