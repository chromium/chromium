// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/safe_browsing/android/native_j_unittests_jni_headers/SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_jni.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

using ::testing::NotNull;

namespace safe_browsing {

namespace {

// This value should be aligned with DEFAULT_CHECK_DELTA_MS in
// SafeBrowsingApiHandlerBridgeNativeUnitTestHelper.
constexpr int kExpectedCheckDeltaMs = 10;

std::vector<SafetyNetJavaThreatType> GetAllSafetyNetThreatsOfInterest() {
  return {SafetyNetJavaThreatType::UNWANTED_SOFTWARE,
          SafetyNetJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION,
          SafetyNetJavaThreatType::SOCIAL_ENGINEERING,
          SafetyNetJavaThreatType::BILLING};
}

std::vector<SafeBrowsingJavaThreatType> GetAllSafeBrowsingThreatTypes() {
  return {SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE,
          SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION,
          SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING,
          SafeBrowsingJavaThreatType::BILLING};
}

SBThreatTypeSet GetAllThreatTypes() {
  return CreateSBThreatTypeSet(
      {SB_THREAT_TYPE_URL_UNWANTED, SB_THREAT_TYPE_URL_MALWARE,
       SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_BILLING});
}

}  // namespace

class SafeBrowsingApiHandlerBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    ASSERT_THAT(env_, NotNull());
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setUp(env_);
  }

  void TearDown() override {
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_tearDown(env_);
  }

 protected:
  void AddSafetyNetBlocklistResponse(const GURL& url,
                                     const std::string& metadata,
                                     const std::vector<SafetyNetJavaThreatType>&
                                         expected_threats_of_interest) {
    ScopedJavaLocalRef<jstring> j_url =
        ConvertUTF8ToJavaString(env_, url.spec());
    int int_threats_of_interest[expected_threats_of_interest.size()];
    int* itr = &int_threats_of_interest[0];
    for (auto threat_type : expected_threats_of_interest) {
      *itr++ = static_cast<int>(threat_type);
    }
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setExpectedSafetyNetApiHandlerThreatsOfInterest(
        env_, j_url,
        ToJavaIntArray(env_, int_threats_of_interest,
                       expected_threats_of_interest.size()));
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafetyNetApiHandlerMetadata(
        env_, j_url, ConvertUTF8ToJavaString(env_, metadata));
  }

  void AddSafeBrowsingResponse(
      const GURL& url,
      const SafeBrowsingJavaThreatType& returned_threat_type,
      const std::vector<SafeBrowsingJavaThreatType>& expected_threat_types,
      const SafeBrowsingJavaProtocol& expected_protocol) {
    ScopedJavaLocalRef<jstring> j_url =
        ConvertUTF8ToJavaString(env_, url.spec());
    int int_threat_types[expected_threat_types.size()];
    int* itr = &int_threat_types[0];
    for (auto expected_threat_type : expected_threat_types) {
      *itr++ = static_cast<int>(expected_threat_type);
    }
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setExpectedSafeBrowsingApiHandlerThreatTypes(
        env_, j_url,
        ToJavaIntArray(env_, int_threat_types, expected_threat_types.size()));
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setExpectedSafeBrowsingApiHandlerProtocol(
        env_, j_url, static_cast<int>(expected_protocol));
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafeBrowsingApiHandlerThreatType(
        env_, j_url, static_cast<int>(returned_threat_type));
  }

  void RunHashDatabaseUrlCheck(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      SBThreatType expected_threat_type,
      SubresourceFilterMatch expected_subresource_filter_match) {
    bool callback_executed = false;
    auto callback =
        std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
            base::BindOnce(
                [](bool* callback_executed, SBThreatType expected_threat_type,
                   SubresourceFilterMatch expected_subresource_filter_match,
                   SBThreatType returned_threat_type,
                   const ThreatMetadata& returned_metadata) {
                  *callback_executed = true;
                  EXPECT_EQ(returned_threat_type, expected_threat_type);
                  EXPECT_EQ(returned_metadata.subresource_filter_match,
                            expected_subresource_filter_match);
                },
                &callback_executed, expected_threat_type,
                expected_subresource_filter_match));
    SafeBrowsingApiHandlerBridge::GetInstance().StartHashDatabaseUrlCheck(
        std::move(callback), url, threat_types);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(callback_executed);
  }

  void RunHashRealTimeUrlCheck(const GURL& url,
                               const SBThreatTypeSet& threat_types,
                               SBThreatType expected_threat_type) {
    bool callback_executed = false;
    auto callback =
        std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
            base::BindOnce(
                [](bool* callback_executed, SBThreatType expected_threat_type,
                   SBThreatType returned_threat_type,
                   const ThreatMetadata& returned_metadata) {
                  *callback_executed = true;
                  EXPECT_EQ(returned_threat_type, expected_threat_type);
                },
                &callback_executed, expected_threat_type));
    SafeBrowsingApiHandlerBridge::GetInstance().StartHashRealTimeUrlCheck(
        std::move(callback), url, threat_types);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(callback_executed);
  }

  void CheckHistogramValues(UmaRemoteCallResult expected_result) {
    histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CheckDelta",
                                         /*sample=*/kExpectedCheckDeltaMs,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.Result",
                                         /*sample=*/expected_result,
                                         /*expected_bucket_count=*/1);
  }

  raw_ptr<JNIEnv> env_;
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashDatabaseUrlCheck_Safe) {
  GURL url("https://example.com");
  AddSafetyNetBlocklistResponse(url, /*metadata=*/"{}",
                                GetAllSafetyNetThreatsOfInterest());

  RunHashDatabaseUrlCheck(url, /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});
  task_environment_.RunUntilIdle();

  CheckHistogramValues(
      /*expected_result=*/UmaRemoteCallResult::SAFE);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_SingleThreatMatch) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com");
  // threat_type: 3 is unwanted.
  std::string metadata = "{\"matches\":[{\"threat_type\":\"3\"}]}";
  AddSafetyNetBlocklistResponse(url, metadata,
                                GetAllSafetyNetThreatsOfInterest());

  RunHashDatabaseUrlCheck(url, /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED,
                          /*expected_subresource_filter_match=*/{});

  CheckHistogramValues(
      /*expected_result=*/UmaRemoteCallResult::MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_MultipleThreatMatch) {
  GURL url("https://example.com");
  std::string metadata =
      "{\"matches\":[{\"threat_type\":\"4\"}, {\"threat_type\":\"5\"}]}";
  AddSafetyNetBlocklistResponse(url, metadata,
                                GetAllSafetyNetThreatsOfInterest());

  // Although the URL matches both malware and phishing, the returned
  // threat type should be malware because the severity of malware
  // threat is higher.
  RunHashDatabaseUrlCheck(url, /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_MALWARE,
                          /*expected_subresource_filter_match=*/{});

  CheckHistogramValues(
      /*expected_result=*/UmaRemoteCallResult::MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_ThreatMatchWithSubresourceFilter) {
  GURL url("https://example.com");
  std::string metadata =
      "{\"matches\":[{\"threat_type\":\"13\", "
      "\"sf_absv\":\"enforce\"}]}";
  AddSafetyNetBlocklistResponse(url, metadata,
                                {SafetyNetJavaThreatType::SUBRESOURCE_FILTER});

  RunHashDatabaseUrlCheck(
      url, /*threat_types=*/{SB_THREAT_TYPE_SUBRESOURCE_FILTER},
      /*expected_threat_type=*/SB_THREAT_TYPE_SUBRESOURCE_FILTER,
      /*expected_subresource_filter_match=*/
      {{SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}});

  CheckHistogramValues(
      /*expected_result=*/UmaRemoteCallResult::MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_MultipleRequests) {
  GURL unsafe_url("https://unsafe.com");
  GURL safe_url("https://safe.com");
  std::string metadata_unsafe = "{\"matches\":[{\"threat_type\":\"5\"}]}";
  std::string metadata_safe = "{}";
  AddSafetyNetBlocklistResponse(unsafe_url, metadata_unsafe,
                                {SafetyNetJavaThreatType::SOCIAL_ENGINEERING});
  AddSafetyNetBlocklistResponse(safe_url, metadata_safe,
                                {SafetyNetJavaThreatType::SOCIAL_ENGINEERING});

  RunHashDatabaseUrlCheck(unsafe_url,
                          /*threat_types=*/{SB_THREAT_TYPE_URL_PHISHING},
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
  RunHashDatabaseUrlCheck(safe_url,
                          /*threat_types=*/{SB_THREAT_TYPE_URL_PHISHING},
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});

  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CheckDelta",
                                       /*sample=*/kExpectedCheckDeltaMs,
                                       /*expected_bucket_count=*/2);
  histogram_tester_.ExpectBucketCount("SB2.RemoteCall.Result",
                                      /*sample=*/UmaRemoteCallResult::MATCH,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount("SB2.RemoteCall.Result",
                                      /*sample=*/UmaRemoteCallResult::SAFE,
                                      /*expected_count=*/1);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashDatabaseUrlCheck_Timeout) {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafetyNetApiHandlerResult(
      env_, static_cast<int>(SafetyNetRemoteCallResultStatus::TIMEOUT));
  GURL url("https://example.com");

  RunHashDatabaseUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});

  CheckHistogramValues(
      /*expected_result=*/UmaRemoteCallResult::TIMEOUT);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, CsdAllowlistCheck) {
  GURL url("https://example.com");
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env_, url.spec());
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdAllowlistMatch(
      env_, j_url, true);
  EXPECT_TRUE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url));
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdAllowlistMatch(
      env_, j_url, false);
  EXPECT_FALSE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashRealTimeUrlCheck_Safe) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(url, SafeBrowsingJavaThreatType::NO_THREAT,
                          GetAllSafeBrowsingThreatTypes(),
                          SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashRealTimeUrlCheck_ThreatMatch) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(url, SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE,
                          GetAllSafeBrowsingThreatTypes(),
                          SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED);
}

// Verifies that the callback_id counters are accumulated correctly.
// Call order: SafetyNet unsafe URL, SafeBrowsing unsafe URL, SafetyNet safe
// URL, SafeBrowsing safe URL.
TEST_F(SafeBrowsingApiHandlerBridgeTest, MultipleRequestsWithDifferentApis) {
  GURL safetynet_unsafe_url("https://safetynet.unsafe.com");
  GURL safetynet_safe_url("https://safetynet.safe.com");
  GURL safebrowsing_unsafe_url("https://safebrowsing.unsafe.com");
  GURL safebrowsing_safe_url("https://safebrowsing.safe.com");
  std::string metadata_unsafe = "{\"matches\":[{\"threat_type\":\"5\"}]}";
  std::string metadata_safe = "{}";
  AddSafetyNetBlocklistResponse(safetynet_unsafe_url, metadata_unsafe,
                                GetAllSafetyNetThreatsOfInterest());
  AddSafetyNetBlocklistResponse(safetynet_safe_url, metadata_safe,
                                GetAllSafetyNetThreatsOfInterest());
  AddSafeBrowsingResponse(
      safebrowsing_unsafe_url, SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);
  AddSafeBrowsingResponse(
      safebrowsing_safe_url, SafeBrowsingJavaThreatType::NO_THREAT,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashDatabaseUrlCheck(safetynet_unsafe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
  RunHashRealTimeUrlCheck(safebrowsing_unsafe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED);
  RunHashDatabaseUrlCheck(safetynet_safe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});
  RunHashRealTimeUrlCheck(safebrowsing_safe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
}

}  // namespace safe_browsing
