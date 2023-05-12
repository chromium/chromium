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

constexpr int kAllThreatsOfInterest[] = {
    JAVA_THREAT_TYPE_UNWANTED_SOFTWARE,
    JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION,
    JAVA_THREAT_TYPE_SOCIAL_ENGINEERING, JAVA_THREAT_TYPE_BILLING};

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
  void AddBlocklistResponse(const GURL& url,
                            const std::string& metadata,
                            const int* expected_threats_of_interest,
                            const size_t expected_threat_size) {
    ScopedJavaLocalRef<jstring> j_url =
        ConvertUTF8ToJavaString(env_, url.spec());
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setExpectedThreatsOfInterest(
        env_, j_url,
        ToJavaIntArray(env_, expected_threats_of_interest,
                       expected_threat_size));
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setMetadata(
        env_, j_url, ConvertUTF8ToJavaString(env_, metadata));
  }

  void CheckHistogramValues(int expected_result) {
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

TEST_F(SafeBrowsingApiHandlerBridgeTest, UrlCheck_Safe) {
  GURL url("https://example.com");
  AddBlocklistResponse(url, /*metadata=*/"{}", kAllThreatsOfInterest,
                       /*expected_threat_size=*/4);

  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_SAFE);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(callback), url, GetAllThreatTypes());
  task_environment_.RunUntilIdle();

  CheckHistogramValues(/*expected_result=*/UMA_STATUS_SAFE);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, UrlCheck_SingleThreatMatch) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com");
  // threat_type: 3 is unwanted.
  std::string metadata = "{\"matches\":[{\"threat_type\":\"3\"}]}";
  AddBlocklistResponse(url, metadata, kAllThreatsOfInterest,
                       /*expected_threat_size=*/4);

  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_URL_UNWANTED);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(callback), url, GetAllThreatTypes());
  task_environment_.RunUntilIdle();

  CheckHistogramValues(/*expected_result=*/UMA_STATUS_MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, UrlCheck_MultipleThreatMatch) {
  GURL url("https://example.com");
  std::string metadata =
      "{\"matches\":[{\"threat_type\":\"4\"}, {\"threat_type\":\"5\"}]}";
  AddBlocklistResponse(url, metadata, kAllThreatsOfInterest,
                       /*expected_threat_size=*/4);

  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            // Although the URL matches both malware and phishing, the returned
            // threat type should be malware because the severity of malware
            // threat is higher.
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_URL_MALWARE);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(callback), url, GetAllThreatTypes());
  task_environment_.RunUntilIdle();

  CheckHistogramValues(/*expected_result=*/UMA_STATUS_MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       UrlCheck_ThreatMatchWithSubresourceFilter) {
  GURL url("https://example.com");
  const int expected_java_threat_types[] = {
      JAVA_THREAT_TYPE_SUBRESOURCE_FILTER};
  std::string metadata =
      "{\"matches\":[{\"threat_type\":\"13\", "
      "\"sf_absv\":\"enforce\"}]}";
  AddBlocklistResponse(url, metadata, expected_java_threat_types,
                       /*expected_threat_size=*/1);

  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_SUBRESOURCE_FILTER);
            SubresourceFilterMatch expected_subresource_filter_match = {
                {SubresourceFilterType::ABUSIVE,
                 SubresourceFilterLevel::ENFORCE}};
            EXPECT_EQ(metadata.subresource_filter_match,
                      expected_subresource_filter_match);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(callback), url,
      CreateSBThreatTypeSet({SB_THREAT_TYPE_SUBRESOURCE_FILTER}));
  task_environment_.RunUntilIdle();

  CheckHistogramValues(/*expected_result=*/UMA_STATUS_MATCH);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, UrlCheck_MultipleRequests) {
  GURL unsafe_url("https://unsafe.com");
  GURL safe_url("https://safe.com");
  const int expected_java_threat_types[] = {
      JAVA_THREAT_TYPE_SOCIAL_ENGINEERING};
  std::string metadata_unsafe = "{\"matches\":[{\"threat_type\":\"5\"}]}";
  std::string metadata_safe = "{}";
  AddBlocklistResponse(unsafe_url, metadata_unsafe, expected_java_threat_types,
                       /*expected_threat_size=*/1);
  AddBlocklistResponse(safe_url, metadata_safe, expected_java_threat_types,
                       /*expected_threat_size=*/1);

  auto unsafe_callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_URL_PHISHING);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(unsafe_callback), unsafe_url,
      CreateSBThreatTypeSet({SB_THREAT_TYPE_URL_PHISHING}));
  auto safe_callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_SAFE);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(safe_callback), safe_url,
      CreateSBThreatTypeSet({SB_THREAT_TYPE_URL_PHISHING}));
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CheckDelta",
                                       /*sample=*/kExpectedCheckDeltaMs,
                                       /*expected_bucket_count=*/2);
  histogram_tester_.ExpectBucketCount("SB2.RemoteCall.Result",
                                      /*sample=*/UMA_STATUS_MATCH,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount("SB2.RemoteCall.Result",
                                      /*sample=*/UMA_STATUS_SAFE,
                                      /*expected_count=*/1);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, UrlCheck_Timeout) {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setResult(
      env_, RESULT_STATUS_TIMEOUT);
  GURL url("https://example.com");

  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce([](SBThreatType matched_threat_type,
                            const ThreatMetadata& metadata) {
            EXPECT_EQ(matched_threat_type, SB_THREAT_TYPE_SAFE);
          }));
  SafeBrowsingApiHandlerBridge::GetInstance().StartURLCheck(
      std::move(callback), url, GetAllThreatTypes());
  task_environment_.RunUntilIdle();

  CheckHistogramValues(/*expected_result=*/UMA_STATUS_TIMEOUT);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, AllowlistCheck) {
  // Csd allowlist
  GURL url("https://example.com");
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env_, url.spec());
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdAllowlistMatch(
      env_, j_url, true);
  EXPECT_TRUE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url));
  EXPECT_FALSE(SafeBrowsingApiHandlerBridge::GetInstance()
                   .StartHighConfidenceAllowlistCheck(url)
                   .value());
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdAllowlistMatch(
      env_, j_url, false);
  EXPECT_FALSE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url));

  // High confidence allowlist
  GURL url2("https://example2.com");
  ScopedJavaLocalRef<jstring> j_url2 =
      ConvertUTF8ToJavaString(env_, url2.spec());
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setHighConfidenceAllowlistMatch(
      env_, j_url2, true);
  EXPECT_TRUE(SafeBrowsingApiHandlerBridge::GetInstance()
                  .StartHighConfidenceAllowlistCheck(url2));
  EXPECT_FALSE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url2));
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setHighConfidenceAllowlistMatch(
      env_, j_url2, false);
  absl::optional<bool> result = SafeBrowsingApiHandlerBridge::GetInstance()
                                    .StartHighConfidenceAllowlistCheck(url2);
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
  // Uninitialized
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_tearDown(env_);
  result = SafeBrowsingApiHandlerBridge::GetInstance()
               .StartHighConfidenceAllowlistCheck(url2);
  EXPECT_FALSE(result.has_value());
}

}  // namespace safe_browsing
