// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class V5GetHashProtocolManagerTest : public ::testing::Test {
 protected:
  V5GetHashProtocolManagerTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
  }

  std::unique_ptr<V5GetHashProtocolManager> CreateProtocolManager() {
    // TODO(crbug.com/362791941): Handle v4 references.
    return std::make_unique<V5GetHashProtocolManager>(
        test_shared_loader_factory_, GetTestV4ProtocolConfig());
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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
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
      {V5::ThreatType::TRICK_TO_BILL, SBThreatType::SB_THREAT_TYPE_BILLING},
  };

  for (const auto& tc : test_cases) {
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
    EXPECT_EQ(metadata, ThreatMetadata());
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
  }
}

}  // namespace safe_browsing
