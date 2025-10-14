// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/fixed_array.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_test_util.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using SafetyNetApiInitializationState =
    safe_browsing::test::SafetyNetApiInitializationState;

std::vector<SafeBrowsingJavaThreatType> GetAllSafeBrowsingThreatTypes() {
  return {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING,
          SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE,
          SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION,
          SafeBrowsingJavaThreatType::BILLING};
}

SBThreatTypeSet GetAllThreatTypes() {
  return CreateSBThreatTypeSet({SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
                                SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
                                SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
                                SBThreatType::SB_THREAT_TYPE_BILLING});
}

}  // namespace

class SafeBrowsingApiHandlerBridgeTest
    : public testing::Test,
      public safe_browsing::test::WithMockSafeBrowsingApiHandler {
 public:
  SafeBrowsingApiHandlerBridgeTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMarkAsPhishing,
        "https://1.example.com,https://examples.com/page1");
  }

  void SetUp() override {
    safe_browsing::test::WithMockSafeBrowsingApiHandler::SetUp();
  }

  void TearDown() override {
    safe_browsing::test::WithMockSafeBrowsingApiHandler::TearDown();
  }

 protected:
  using enum SBThreatType;

  void CheckSafeBrowsingApiHistogramValues(
      const std::string& suffix,
      bool expected_is_available,
      SafeBrowsingJavaValidationResult expected_validation_result,
      int expected_lookup_result,
      std::optional<int> expected_threat_type,
      std::optional<int> expected_threat_attribute,
      std::optional<int> expected_threat_attribute_count,
      std::optional<int> expected_response_status) {
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta",
        /*sample=*/kExpectedSafeBrowsingCheckDeltaMicroseconds,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta" + suffix,
        /*sample=*/kExpectedSafeBrowsingCheckDeltaMicroseconds,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.IsAvailable",
        /*sample=*/expected_is_available,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.IsAvailable" + suffix,
        /*sample=*/expected_is_available,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult",
        /*sample=*/expected_validation_result,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult" + suffix,
        /*sample=*/expected_validation_result,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.LookupResult",
        /*sample=*/expected_lookup_result,
        /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.GmsSafeBrowsingApi.LookupResult" + suffix,
        /*sample=*/expected_lookup_result,
        /*expected_bucket_count=*/1);
    if (expected_threat_type.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatType2",
          /*sample=*/expected_threat_type.value(),
          /*expected_bucket_count=*/1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatType2" + suffix,
          /*sample=*/expected_threat_type.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ThreatType2",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ThreatType2" + suffix,
          /*expected_count=*/0);
    }
    if (expected_threat_attribute.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttribute",
          /*sample=*/expected_threat_attribute.value(),
          /*expected_bucket_count=*/1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttribute" + suffix,
          /*sample=*/expected_threat_attribute.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ThreatAttribute",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ThreatAttribute" + suffix,
          /*expected_count=*/0);
    }
    if (expected_threat_attribute_count.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount",
          /*sample=*/expected_threat_attribute_count.value(),
          /*expected_bucket_count=*/1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount" + suffix,
          /*sample=*/expected_threat_attribute_count.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount" + suffix,
          /*expected_count=*/0);
    }
    if (expected_response_status.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ResponseStatus",
          /*sample=*/expected_response_status.value(),
          /*expected_bucket_count=*/1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.GmsSafeBrowsingApi.ResponseStatus" + suffix,
          /*sample=*/expected_response_status.value(),
          /*expected_bucket_count=*/1);
      if (expected_response_status.value() ==
          static_cast<int>(
              SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME)) {
        histogram_tester_.ExpectUniqueSample(
            "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta.SuccessWithRealTime",
            /*sample=*/kExpectedSafeBrowsingCheckDeltaMicroseconds,
            /*expected_bucket_count=*/1);
      } else {
        histogram_tester_.ExpectTotalCount(
            "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta.SuccessWithRealTime",
            /*expected_count=*/0);
      }
    } else {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ResponseStatus",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.GmsSafeBrowsingApi.ResponseStatus" + suffix,
          /*expected_count=*/0);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashDatabaseUrlCheck_Safe) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      GetAllSafeBrowsingThreatTypes(),
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);

  RunHashDatabaseUrlCheck(url, /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});

  CheckSafeBrowsingApiHistogramValues(
      ".LocalBlocklist",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(SafeBrowsingJavaThreatType::NO_THREAT),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(
          SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_SingleThreatMatch) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      GetAllSafeBrowsingThreatTypes(),
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);

  RunHashDatabaseUrlCheck(url, /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED,
                          /*expected_subresource_filter_match=*/{});

  CheckSafeBrowsingApiHistogramValues(
      ".LocalBlocklist",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(
          SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_ThreatMatchWithSubresourceFilter) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
      {SafeBrowsingJavaThreatAttribute::CANARY},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING,
       SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION},
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);

  RunHashDatabaseUrlCheck(
      url, /*threat_types=*/
      {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_SUBRESOURCE_FILTER},
      /*expected_threat_type=*/SB_THREAT_TYPE_SUBRESOURCE_FILTER,
      /*expected_subresource_filter_match=*/
      {{SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::WARN}});

  CheckSafeBrowsingApiHistogramValues(
      ".LocalBlocklist",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(
          SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION),
      /*expected_threat_attribute=*/1,
      /*expected_threat_attribute_count=*/
      static_cast<int>(SafeBrowsingJavaThreatAttribute::CANARY),
      /*expected_response_status=*/
      static_cast<int>(
          SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashDatabaseUrlCheck_MultipleRequests) {
  GURL unsafe_url("https://unsafe.com");
  GURL safe_url("https://safe.com");
  AddSafeBrowsingResponse(
      unsafe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING},
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
  AddSafeBrowsingResponse(
      safe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING},
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);

  RunHashDatabaseUrlCheck(unsafe_url,
                          /*threat_types=*/{SB_THREAT_TYPE_URL_PHISHING},
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
  RunHashDatabaseUrlCheck(safe_url,
                          /*threat_types=*/{SB_THREAT_TYPE_URL_PHISHING},
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta",
      /*sample=*/kExpectedSafeBrowsingCheckDeltaMicroseconds,
      /*expected_bucket_count=*/2);
  histogram_tester_.ExpectBucketCount(
      "SafeBrowsing.GmsSafeBrowsingApi.ThreatType2",
      /*sample=*/
      static_cast<int>(SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING),
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "SafeBrowsing.GmsSafeBrowsingApi.ThreatType2",
      /*sample=*/
      static_cast<int>(SafeBrowsingJavaThreatType::NO_THREAT),
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashDatabaseUrlCheck_Timeout) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::FAILURE_API_CALL_TIMEOUT,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      GetAllSafeBrowsingThreatTypes(),
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);

  RunHashDatabaseUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});

  CheckSafeBrowsingApiHistogramValues(
      ".LocalBlocklist",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::FAILURE_API_CALL_TIMEOUT),
      /*expected_threat_type=*/std::nullopt,
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/std::nullopt,
      /*expected_response_status=*/std::nullopt);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashDatabaseUrlCheck_FromCommandline) {
  SafeBrowsingApiHandlerBridge::GetInstance().PopulateArtificialDatabase();
  GURL url1("https://1.example.com/");
  GURL url2("https://examples.com/page1");

  RunHashDatabaseUrlCheck(url1,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
  RunHashDatabaseUrlCheck(url2,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, CheckLocalAllowlists) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kInitialized);

  GURL url1("https://example.com");
  GURL url2("https://download.test");
  GURL url3("https://bothlists.test");
  AddLocalAllowlistEntry(url1, /*is_download_allowlist=*/false,
                         /*is_match=*/true);
  AddLocalAllowlistEntry(url2, /*is_download_allowlist=*/false,
                         /*is_match=*/false);
  AddLocalAllowlistEntry(url3, /*is_download_allowlist=*/false,
                         /*is_match=*/true);
  AddLocalAllowlistEntry(url1, /*is_download_allowlist=*/true,
                         /*is_match=*/false);
  AddLocalAllowlistEntry(url2, /*is_download_allowlist=*/true,
                         /*is_match=*/true);
  AddLocalAllowlistEntry(url3, /*is_download_allowlist=*/true,
                         /*is_match=*/true);

  EXPECT_TRUE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url1));
  EXPECT_FALSE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url2));
  EXPECT_TRUE(
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url3));

  EXPECT_FALSE(SafeBrowsingApiHandlerBridge::GetInstance()
                   .StartCSDDownloadAllowlistCheck(url1));
  EXPECT_TRUE(SafeBrowsingApiHandlerBridge::GetInstance()
                  .StartCSDDownloadAllowlistCheck(url2));
  EXPECT_TRUE(SafeBrowsingApiHandlerBridge::GetInstance()
                  .StartCSDDownloadAllowlistCheck(url3));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashRealTimeUrlCheck_Safe) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(SafeBrowsingJavaThreatType::NO_THREAT),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HashRealTimeUrlCheck_ThreatMatch) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/
                          GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_InvalidLookupResult) {
  GURL url("https://example.com");
  int invalid_lookup_result = 100;
  AddSafeBrowsingResponse(
      url, static_cast<SafeBrowsingApiLookupResult>(invalid_lookup_result),
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Although the returned threat type is POTENTIALLY_HARMFUL_APPLICATION, the
  // expected threat type is SAFE because the lookup result is invalid.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/
      SafeBrowsingJavaValidationResult::INVALID_LOOKUP_RESULT,
      /*expected_lookup_result=*/invalid_lookup_result,
      /*expected_threat_type=*/std::nullopt,
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/std::nullopt,
      /*expected_response_status=*/std::nullopt);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_InvalidThreatType) {
  GURL url("https://example.com");
  int invalid_threat_type = 100;
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      static_cast<SafeBrowsingJavaThreatType>(invalid_threat_type), {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Default to safe if the threat type is unrecognized.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/
      SafeBrowsingJavaValidationResult::INVALID_THREAT_TYPE,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/invalid_threat_type,
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_InvalidThreatAttributes) {
  GURL url("https://example.com");
  int invalid_attribute = 0;
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION,
      {static_cast<SafeBrowsingJavaThreatAttribute>(invalid_attribute)},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Although the returned threat type is POTENTIALLY_HARMFUL_APPLICATION, the
  // expected threat type is SAFE because the threat attributes contain invalid
  // value.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/
      SafeBrowsingJavaValidationResult::INVALID_THREAT_ATTRIBUTE,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(
          SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION),
      /*expected_threat_attribute=*/invalid_attribute,
      /*expected_threat_attribute_count=*/1,
      /*expected_response_status=*/
      static_cast<int>(SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME));
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_InvalidResponseStatus) {
  GURL url("https://example.com");
  int invalid_response_status = 100;
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION, {},
      static_cast<SafeBrowsingJavaResponseStatus>(invalid_response_status),
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Although the response status is invalid, the returned threat type should
  // still be sent. This is to avoid the API adding a new success
  // response_status while we haven't integrated the new value yet.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_MALWARE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/
      SafeBrowsingJavaValidationResult::VALID_WITH_UNRECOGNIZED_RESPONSE_STATUS,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(
          SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/invalid_response_status);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_UnsuccessfulLookupResult) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::FAILURE,
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Although the returned threat type is POTENTIALLY_HARMFUL_APPLICATION, the
  // expected threat type is SAFE because the lookup result is failure.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::FAILURE),
      /*expected_threat_type=*/std::nullopt,
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/std::nullopt,
      /*expected_response_status=*/std::nullopt);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_NonRecoverableLookupResult) {
  GURL url1("https://example1.com");
  // FAILURE_API_UNSUPPORTED is a non-recoverable error.
  AddSafeBrowsingResponse(
      url1, SafeBrowsingApiLookupResult::FAILURE_API_UNSUPPORTED,
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashRealTimeUrlCheck(url1,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.GmsSafeBrowsingApi.IsAvailable",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult",
      /*expected_count=*/1);

  GURL url2("https://example2.com");

  RunHashRealTimeUrlCheck(url2,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  histogram_tester_.ExpectBucketCount(
      "SafeBrowsing.GmsSafeBrowsingApi.IsAvailable", /*sample=*/false,
      /*expected_count=*/1);
  // No additional histogram because SafeBrowsing API is not available.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult",
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       HashRealTimeUrlCheck_UnsuccessfulResponseStatus) {
  GURL url("https://example.com");
  AddSafeBrowsingResponse(
      url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION, {},
      SafeBrowsingJavaResponseStatus::FAILURE_NETWORK_UNAVAILABLE,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  // Although the returned threat type is POTENTIALLY_HARMFUL_APPLICATION, the
  // expected threat type is SAFE because the response status is failure.
  RunHashRealTimeUrlCheck(url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  CheckSafeBrowsingApiHistogramValues(
      ".RealTime",
      /*expected_is_available=*/true,
      /*expected_validation_result=*/SafeBrowsingJavaValidationResult::VALID,
      /*expected_lookup_result=*/
      static_cast<int>(SafeBrowsingApiLookupResult::SUCCESS),
      /*expected_threat_type=*/
      static_cast<int>(
          SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION),
      /*expected_threat_attribute=*/std::nullopt,
      /*expected_threat_attribute_count=*/0,
      /*expected_response_status=*/
      static_cast<int>(
          SafeBrowsingJavaResponseStatus::FAILURE_NETWORK_UNAVAILABLE));
}

// Verifies that the callback_id counters are accumulated correctly.
// Call order: hash database unsafe URL, hash realtime unsafe URL, hash database
// safe URL, hash realtime safe URL.
TEST_F(SafeBrowsingApiHandlerBridgeTest,
       MultipleRequestsWithDifferentProtocols) {
  GURL hash_database_unsafe_url("https://hashdatabase.unsafe.com");
  GURL hash_database_safe_url("https://hashdatabase.safe.com");
  GURL hash_realtime_unsafe_url("https://hashrealtime.unsafe.com");
  GURL hash_realtime_safe_url("https://hashrealtime.safe.com");
  AddSafeBrowsingResponse(
      hash_database_unsafe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      GetAllSafeBrowsingThreatTypes(),
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
  AddSafeBrowsingResponse(
      hash_database_safe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      GetAllSafeBrowsingThreatTypes(),
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
  AddSafeBrowsingResponse(
      hash_realtime_unsafe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);
  AddSafeBrowsingResponse(
      hash_realtime_safe_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      GetAllSafeBrowsingThreatTypes(), SafeBrowsingJavaProtocol::REAL_TIME);

  RunHashDatabaseUrlCheck(hash_database_unsafe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING,
                          /*expected_subresource_filter_match=*/{});
  RunHashRealTimeUrlCheck(hash_realtime_unsafe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED);
  RunHashDatabaseUrlCheck(hash_database_safe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*expected_subresource_filter_match=*/{});
  RunHashRealTimeUrlCheck(hash_realtime_safe_url,
                          /*threat_types=*/GetAllThreatTypes(),
                          /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, IsVerifyAppsEnabled) {
  SetVerifyAppsResult(VerifyAppsEnabledResult::SUCCESS_ENABLED);
  base::test::TestFuture<VerifyAppsEnabledResult> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartIsVerifyAppsEnabled(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), VerifyAppsEnabledResult::SUCCESS_ENABLED);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, EnableVerifyApps) {
  SetVerifyAppsResult(VerifyAppsEnabledResult::TIMEOUT);
  base::test::TestFuture<VerifyAppsEnabledResult> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartEnableVerifyApps(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), VerifyAppsEnabledResult::TIMEOUT);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HasPotentiallyHarmfulApps_Success) {
  SetHarmfulAppsResult(HasHarmfulAppsResultStatus::SUCCESS, 2, 0);
  base::test::TestFuture<HasHarmfulAppsResultStatus, int, int> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartHasPotentiallyHarmfulApps(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get<0>(), HasHarmfulAppsResultStatus::SUCCESS);
  EXPECT_EQ(result_future.Get<1>(), 2);
  EXPECT_EQ(result_future.Get<2>(), 0);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, HasPotentiallyHarmfulApps_Failure) {
  static constexpr int kSampleErrorStatusCode = 400;
  SetHarmfulAppsResult(HasHarmfulAppsResultStatus::API_FAILURE, 0,
                       kSampleErrorStatusCode);
  base::test::TestFuture<HasHarmfulAppsResultStatus, int, int> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartHasPotentiallyHarmfulApps(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get<0>(), HasHarmfulAppsResultStatus::API_FAILURE);
  EXPECT_EQ(result_future.Get<1>(), 0);
  EXPECT_EQ(result_future.Get<2>(), kSampleErrorStatusCode);
}

TEST_F(SafeBrowsingApiHandlerBridgeTest, GetSafetyNetIdFailsIfNotInitialized) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kNotAvailable);
  base::test::TestFuture<const std::string&> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), "");
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       GetSafetyNetIdFailsIfFirstPartyApiNotAvailable) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kInitialized);
  base::test::TestFuture<const std::string&> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), "");
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       GetSafetyNetIdSucceedsIfFirstPartyApiAvailable) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kInitializedFirstParty);
  base::test::TestFuture<const std::string&> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), "safety-net-id-0");
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       GetSafetyNetIdCachesAndReturnsSameNonEmptyResult) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kInitializedFirstParty);
  base::test::TestFuture<const std::string&> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), "safety-net-id-0");

  EXPECT_TRUE(SafeBrowsingApiHandlerBridge::GetInstance()
                  .GetCachedSafetyNetIdForTesting()
                  .has_value());
  EXPECT_EQ(*SafeBrowsingApiHandlerBridge::GetInstance()
                 .GetCachedSafetyNetIdForTesting(),
            "safety-net-id-0");

  base::test::TestFuture<const std::string&> result_future2;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future2.GetCallback());
  EXPECT_EQ(result_future2.Get(), "safety-net-id-0");
}

TEST_F(SafeBrowsingApiHandlerBridgeTest,
       GetSafetyNetIdCachesAndReturnsEmptyResult) {
  SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState::kInitializedFirstParty);
  // Simulate an error that returns an empty result despite being initialized.
  SetSafetyNetIdResultEmpty();

  base::test::TestFuture<const std::string&> result_future;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), "");

  EXPECT_TRUE(SafeBrowsingApiHandlerBridge::GetInstance()
                  .GetCachedSafetyNetIdForTesting()
                  .has_value());
  EXPECT_EQ(*SafeBrowsingApiHandlerBridge::GetInstance()
                 .GetCachedSafetyNetIdForTesting(),
            "");

  base::test::TestFuture<const std::string&> result_future2;
  SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
      result_future2.GetCallback());
  EXPECT_EQ(result_future2.Get(), "");
}

}  // namespace safe_browsing
