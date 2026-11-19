// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

// All codes below end with a valid CRC-16/CCITT-FALSE, unless the test is about
// the CRC itself.
constexpr char kDynamicPixCode[] =
    "00020126370014br.gov.bcb.pix2515www.example.com63042003";
constexpr char kDuitNowCode[] =
    "00020126280014A0000006150001"
    "0106890000"
    "5802MY6304DCBC";
constexpr char kDuitNowCodeWithWrongCrc[] =
    "00020126280014A0000006150001"
    "0106890000"
    "5802MY6304DCBD";
constexpr char kUrlQrCode[] = "https://example.com/";
// A Pix code without the CRC data object.
constexpr char kPixCodeWithoutCrc[] =
    "00020126370014br.gov.bcb.pix2515www.example.com5802BR";
constexpr char kPixCodeWithEmptyAdditionalDataFieldTemplate[] =
    "00020126370014br.gov.bcb.pix"
    "2515www.example.com62006304BBBE";

constexpr char kPixValidationResultHistogram[] =
    "FacilitatedPayments.QrCode.Pix.ValidationResult";
constexpr char kDuitNowValidationResultHistogram[] =
    "FacilitatedPayments.QrCode.DuitNow.ValidationResult";
constexpr char kUnknownValidationResultHistogram[] =
    "FacilitatedPayments.QrCode.Unknown.ValidationResult";

class TestFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  using FacilitatedPaymentsDriver::FacilitatedPaymentsDriver;
  ~TestFacilitatedPaymentsDriver() override = default;

 protected:
  bool IsSecureForPaymentHandling() const override { return true; }
};

class FacilitatedPaymentsDriverQrCodeTest : public testing::Test {
 public:
  FacilitatedPaymentsDriverQrCodeTest()
      : driver_(&client_,
                base::BindRepeating(
                    &MockFacilitatedPaymentsApiClient::CreateApiClient)) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  const ukm::SourceId ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  MockFacilitatedPaymentsClient client_;
  TestFacilitatedPaymentsDriver driver_;
};

// Tests that a valid code is logged as valid under its scheme.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsValidCodeUnderScheme) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kDuitNowCode}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kDuitNowValidationResultHistogram,
                                      QrCodeValidationResult::kValid, 1);
}

// Tests that a Pix code is logged with its Pix code type.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsPixCodeType) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kDynamicPixCode}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kPixValidationResultHistogram,
                                      QrCodeValidationResult::kValidPixDynamic,
                                      1);
}

// Tests that a code that fails validation is still logged under its scheme, so
// that failures can be measured per scheme.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsFailureUnderScheme) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kDuitNowCodeWithWrongCrc}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kDuitNowValidationResultHistogram,
                                      QrCodeValidationResult::kFailedCrcCheck,
                                      1);
}

// Tests that a code whose data objects fail to parse is logged under its
// scheme.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsFailedTlvParsingUnderScheme) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kPixCodeWithoutCrc}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kPixValidationResultHistogram,
                                      QrCodeValidationResult::kFailedTlvParsing,
                                      1);
}

// Tests that a code with an empty additional data field template is logged
// under its scheme.
TEST_F(FacilitatedPaymentsDriverQrCodeTest,
       LogsEmptyAdditionalDataFieldTemplateUnderScheme) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected(
      {kPixCodeWithEmptyAdditionalDataFieldTemplate}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(
      kPixValidationResultHistogram,
      QrCodeValidationResult::kEmptyAdditionalDataFieldTemplate, 1);
}

// Tests that a QR code that is not a payment code is logged under the unknown
// scheme.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsNonPaymentCodeAsUnknown) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kUrlQrCode}, ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kUnknownValidationResultHistogram,
                                      QrCodeValidationResult::kNotPaymentCode,
                                      1);
}

// Tests that every QR code decoded from a page is logged.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, LogsEachCode) {
  base::HistogramTester histogram_tester;

  driver_.OnPaymentQrCodesDetected({kDuitNowCode, kDuitNowCode, kUrlQrCode},
                                   ukm_source_id_);

  histogram_tester.ExpectUniqueSample(kDuitNowValidationResultHistogram,
                                      QrCodeValidationResult::kValid, 2);
  histogram_tester.ExpectUniqueSample(kUnknownValidationResultHistogram,
                                      QrCodeValidationResult::kNotPaymentCode,
                                      1);
}

// Tests that a UKM with the scheme and the validation result is recorded for
// each QR code, on the page's source.
TEST_F(FacilitatedPaymentsDriverQrCodeTest, RecordsUkmForEachCode) {
  using UkmEntry = ukm::builders::FacilitatedPayments_QrCodeDetected;

  driver_.OnPaymentQrCodesDetected({kDuitNowCodeWithWrongCrc, kUrlQrCode},
                                   ukm_source_id_);

  const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder_.GetEntries(UkmEntry::kEntryName,
                               {UkmEntry::kPaymentQrCodeSchemeName,
                                UkmEntry::kValidationResultName});
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].source_id, ukm_source_id_);
  EXPECT_EQ(entries[0].metrics.at(UkmEntry::kPaymentQrCodeSchemeName),
            static_cast<int64_t>(QrCodeScheme::kDuitNow));
  EXPECT_EQ(entries[0].metrics.at(UkmEntry::kValidationResultName),
            static_cast<int64_t>(QrCodeValidationResult::kFailedCrcCheck));
  EXPECT_EQ(entries[1].source_id, ukm_source_id_);
  EXPECT_EQ(entries[1].metrics.at(UkmEntry::kPaymentQrCodeSchemeName),
            static_cast<int64_t>(QrCodeScheme::kUnknown));
  EXPECT_EQ(entries[1].metrics.at(UkmEntry::kValidationResultName),
            static_cast<int64_t>(QrCodeValidationResult::kNotPaymentCode));
}

}  // namespace
}  // namespace payments::facilitated
