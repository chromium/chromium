// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/web_payments_telemetry.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace payments {
namespace {

constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

std::string Base64UrlEncodeString(std::string_view input) {
  std::string encoded;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

std::string CreateCResJson(std::string_view trans_status) {
  return base::StringPrintf(R"({"transStatus":"%s"})", trans_status.data());
}

class WebPaymentsTelemetryTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  const ukm::SourceId source_id_ = ukm::UkmRecorder::GetNewSourceID();
};

TEST_F(WebPaymentsTelemetryTest, UnrelatedFormData) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  RecordThreeDSecureTelemetryFromFormData("username=foo&password=bar",
                                          source_id_);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsTelemetryTest, ChallengeRequestTelemetry) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  RecordThreeDSecureTelemetryFromFormData("creq=sample_challenge_request",
                                          source_id_);
  histogram_tester.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, source_id_);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
          kChallengeRequestName,
      true);
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsTelemetryTest, ChallengeResponseTelemetry_ValidStatuses) {
  const struct {
    const char* status_char;
    ThreeDSecureTransactionStatus expected_status;
  } kTestCases[] = {
      {"Y", ThreeDSecureTransactionStatus::kSuccess},
      {"N", ThreeDSecureTransactionStatus::kDenied},
      {"U", ThreeDSecureTransactionStatus::kCouldNotBePerformed},
      {"A", ThreeDSecureTransactionStatus::kAttemptsProcessingPerformed},
      {"C", ThreeDSecureTransactionStatus::kChallengeRequired},
      {"D", ThreeDSecureTransactionStatus::kChallengeRequiredDecoupled},
      {"R", ThreeDSecureTransactionStatus::kRejected},
      {"I", ThreeDSecureTransactionStatus::kInformationalOnly},
      {"S", ThreeDSecureTransactionStatus::kChallengeUsingSPC},
      {"y", ThreeDSecureTransactionStatus::kSuccess},
      {"n", ThreeDSecureTransactionStatus::kDenied},
      {"u", ThreeDSecureTransactionStatus::kCouldNotBePerformed},
      {"a", ThreeDSecureTransactionStatus::kAttemptsProcessingPerformed},
      {"c", ThreeDSecureTransactionStatus::kChallengeRequired},
      {"d", ThreeDSecureTransactionStatus::kChallengeRequiredDecoupled},
      {"r", ThreeDSecureTransactionStatus::kRejected},
      {"i", ThreeDSecureTransactionStatus::kInformationalOnly},
      {"s", ThreeDSecureTransactionStatus::kChallengeUsingSPC},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::string form_data =
        "cres=" + Base64UrlEncodeString(CreateCResJson(test_case.status_char));
    RecordThreeDSecureTelemetryFromFormData(form_data, source_id_);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        test_case.expected_status, 1);
    histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0]->source_id, source_id_);
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        entries[0],
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
            kChallengeResponseName,
        static_cast<int64_t>(test_case.expected_status));
    EXPECT_TRUE(ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
                            kEntryName)
                    .empty());
  }
}

TEST_F(WebPaymentsTelemetryTest,
       ChallengeResponseTelemetry_CaseInsensitiveTransactionStatusKey) {
  for (const char* key : {"transstatus", "TRANSSTATUS", "TransStatus"}) {
    base::HistogramTester histogram_tester;
    std::string form_data = "cres=" + Base64UrlEncodeString(base::StringPrintf(
                                          R"({"%s":"Y"})", key));
    RecordThreeDSecureTelemetryFromFormData(form_data, source_id_);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        ThreeDSecureTransactionStatus::kSuccess,
                                        1);
  }
}

TEST_F(WebPaymentsTelemetryTest, CaseInsensitiveFormDataKeys) {
  for (const char* key : {"CREQ", "CReq", "creq"}) {
    base::HistogramTester histogram_tester;
    RecordThreeDSecureTelemetryFromFormData(
        base::StringPrintf("%s=sample_challenge_request", key), source_id_);
    histogram_tester.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  }
  for (const char* key : {"CRES", "CRes", "cres"}) {
    base::HistogramTester histogram_tester;
    RecordThreeDSecureTelemetryFromFormData(
        base::StringPrintf("%s=", key) +
            Base64UrlEncodeString(CreateCResJson("Y")),
        source_id_);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        ThreeDSecureTransactionStatus::kSuccess,
                                        1);
  }
}

TEST_F(WebPaymentsTelemetryTest, ChallengeResponseTelemetry_UnknownStatuses) {
  const std::string kInvalidJsons[] = {
      CreateCResJson("X"),          // Unrecognized status character
      CreateCResJson(""),           // Empty status
      CreateCResJson("YY"),         // Multi-character status
      R"({"otherField":"value"})",  // Missing transStatus
      R"({"transStatus":123})",     // Non-string transStatus
      R"({"transStatus":true})",    // Boolean transStatus
      "not_a_valid_json_string",    // Invalid JSON
  };

  for (const auto& json : kInvalidJsons) {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::string form_data = "cres=" + Base64UrlEncodeString(json);
    RecordThreeDSecureTelemetryFromFormData(form_data, source_id_);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        ThreeDSecureTransactionStatus::kUnknown,
                                        1);
    histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        entries[0],
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
            kChallengeResponseName,
        static_cast<int64_t>(ThreeDSecureTransactionStatus::kUnknown));
    EXPECT_TRUE(ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
                            kEntryName)
                    .empty());
  }
}

TEST_F(WebPaymentsTelemetryTest, ChallengeResponseTelemetry_EncryptedJWE) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // RFC 7516 JSON Web Encryption (JWE) compact serialization string:
  // BASE64URL(Protected Header).BASE64URL(Encrypted Key).BASE64URL(IV).
  // BASE64URL(Ciphertext).BASE64URL(Authentication Tag)
  const std::string jwe =
      "eyJhbGciOiJSU0ExXzUiLCJlbmMiOiJBMTI4Q0JDLUhTMjU2In0."
      "UGhIOguFailaqAn0_JHAkWGqqDaioIGPBAEBPqsuTO4TXdzUAvnCxfndoAqudaZWYemmE00D"
      "a_z4GQ0_gevmUhBpKt_xwwAiExoDZWoiK0gyTFsPXWGxCpElSRS8uvPTOG42XuBpLOCAcNc"
      "pq7uLqtuSTFaQO59bVDiGDQu_ly2OOVC38nJLgy1hyGQDxacWm8smLXyhnxxx0IKndihQiA"
      "mRmVMwGMXhz945mlPdkZ87X6li4BOFcT5qP036znVV03cvHiKTiN75qqbYHyKnYkhFlvMQ"
      "J59tATzx87Pn9lPlVOECQeaTdHodTfZuoOH88MgU10PWCf252WVGYFs6UUCBKjg."
      "AxY8DCtDaGlsbGljb3RoZQ."
      "KDlTtXchhZTGufMYmOYGS4HffxPSnvxxo63n2YsTyUQ."
      "7AEfFlyCLHHZmrientbOTw";
  std::string form_data = "cres=" + jwe;
  RecordThreeDSecureTelemetryFromFormData(form_data, source_id_);
  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram,
      ThreeDSecureTransactionStatus::kJSONEncrypted, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kJSONEncrypted));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

TEST_F(WebPaymentsTelemetryTest,
       ChallengeResponseTelemetry_InvalidBase64WithoutDots) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  RecordThreeDSecureTelemetryFromFormData("cres=invalid!char#without%dots",
                                          source_id_);
  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram, ThreeDSecureTransactionStatus::kUnknown, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kUnknown));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

TEST_F(WebPaymentsTelemetryTest,
       ChallengeResponseTelemetry_PercentEncodedPadding) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // {"transStatus":"Y"} Base64Url-encoded with padding is
  // eyJ0cmFuc1N0YXR1cyI6IlkifQ== Standard form submissions encode '=' as '%3D'.
  RecordThreeDSecureTelemetryFromFormData(
      "cres=eyJ0cmFuc1N0YXR1cyI6IlkifQ%3D%3D", source_id_);
  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram, ThreeDSecureTransactionStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kSuccess));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

void ThreeDSecureTelemetryChallengePostData(const std::string& form_data) {
  RecordThreeDSecureTelemetryFromFormData(form_data, ukm::kInvalidSourceId);
}

void ThreeDSecureTelemetryChallengeResponseValue(
    const std::string& cres_value) {
  RecordThreeDSecureTelemetryFromFormData("cres=" + cres_value,
                                          ukm::kInvalidSourceId);
}

FUZZ_TEST(WebPaymentsTelemetryFuzzTest, ThreeDSecureTelemetryChallengePostData);
FUZZ_TEST(WebPaymentsTelemetryFuzzTest,
          ThreeDSecureTelemetryChallengeResponseValue);

}  // namespace
}  // namespace payments
