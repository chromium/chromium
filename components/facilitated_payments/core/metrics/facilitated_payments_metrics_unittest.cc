// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

std::string GetPurchaseActionResultString(PurchaseActionResult result) {
  switch (result) {
    case PurchaseActionResult::kResultOk:
      return "Succeeded";
    case PurchaseActionResult::kCouldNotInvoke:
      return "Failed";
    case PurchaseActionResult::kResultCanceled:
      return "Abandoned";
  }
}

}  // namespace

TEST(FacilitatedPaymentsMetricsTest, LogPixCodeCopied) {
  base::HistogramTester histogram_tester;

  LogPixCodeCopied(ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.PixCodeCopied",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogEwalletPaymentLinkDetected) {
  base::HistogramTester histogram_tester;

  LogPaymentLinkDetected(ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PaymentLinkDetected",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogPixFopSelectedAndLatency) {
  base::HistogramTester histogram_tester;

  LogPixFopSelectedAndLatency(base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.FopSelected.Latency",
      /*sample=*/10,
      /*expected_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogEwalletFopSelected) {
  for (AvailableEwalletsConfiguration type :
       {AvailableEwalletsConfiguration::kSingleBoundEwallet,
        AvailableEwalletsConfiguration::kSingleUnboundEwallet,
        AvailableEwalletsConfiguration::kMultipleEwallets}) {
    base::HistogramTester histogram_tester;

    LogEwalletFopSelected(type);

    std::string type_string;
    switch (type) {
      case AvailableEwalletsConfiguration::kSingleBoundEwallet:
        type_string = "SingleBoundEwallet";
        break;
      case AvailableEwalletsConfiguration::kSingleUnboundEwallet:
        type_string = "SingleUnboundEwallet";
        break;
      case AvailableEwalletsConfiguration::kMultipleEwallets:
        type_string = "MultipleEwallets";
        break;
    }
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Ewallet.FopSelector.UserAction.",
                      type_string}),
        /*sample=*/FopSelectorAction::kFopSelected,
        /*expected_bucket_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_ValidatorFailed) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(
      /*result=*/base::unexpected("Data Decoder terminated unexpectedly"),
      base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.ValidatorFailed.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_InvalidCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(/*result=*/false,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.InvalidCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_ValidCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(/*result=*/true,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.ValidCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPixInitiatePurchaseActionResultAndLatency) {
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;

    LogPixInitiatePurchaseActionResultAndLatency(result,
                                                 base::Milliseconds(10));

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.",
                      GetPurchaseActionResultString(result), ".Latency"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest, LogPixTransactionResultAndLatency) {
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;

    LogPixTransactionResultAndLatency(result, base::Milliseconds(10));

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.Transaction.",
                      GetPurchaseActionResultString(result), ".Latency"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest,
     LogEwalletInitiatePurchaseActionResultAndLatency_DeviceBound) {
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;

    LogEwalletInitiatePurchaseActionResultAndLatency(
        result, base::Milliseconds(10),
        PaymentLinkValidator::Scheme::kShopeePay,
        /*is_device_bound=*/true);

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                      GetPurchaseActionResultString(result),
                      ".Latency.DeviceBound"}),
        /*sample=*/10,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                      GetPurchaseActionResultString(result),
                      ".Latency.ShopeePay.DeviceBound"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest,
     LogEwalletInitiatePurchaseActionResultAndLatency_DeviceNotBound) {
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;

    LogEwalletInitiatePurchaseActionResultAndLatency(
        result, base::Milliseconds(10),
        PaymentLinkValidator::Scheme::kShopeePay,
        /*is_device_bound=*/false);

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                      GetPurchaseActionResultString(result),
                      ".Latency.DeviceNotBound"}),
        /*sample=*/10,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                      GetPurchaseActionResultString(result),
                      ".Latency.ShopeePay.DeviceNotBound"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

class FacilitatedPaymentsMetricsEwalletExitedReasonTest
    : public testing::TestWithParam<
          std::tuple<EwalletFlowExitedReason, PaymentLinkValidator::Scheme>> {
 public:
  EwalletFlowExitedReason payflow_exit_reason() const {
    return std::get<0>(GetParam());
  }

  PaymentLinkValidator::Scheme scheme() const {
    return std::get<1>(GetParam());
  }

  std::string GetSchemeString() const {
    switch (scheme()) {
      case PaymentLinkValidator::Scheme::kDuitNow:
        return "DuitNow";
      case PaymentLinkValidator::Scheme::kShopeePay:
        return "ShopeePay";
      case PaymentLinkValidator::Scheme::kTngd:
        return "Tngd";
      case PaymentLinkValidator::Scheme::kInvalid:
        NOTREACHED();
    }
  }
};

TEST_P(FacilitatedPaymentsMetricsEwalletExitedReasonTest,
       LogEwalletFlowExitedReason) {
  base::HistogramTester histogram_tester;

  LogEwalletFlowExitedReason(payflow_exit_reason(), scheme());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/payflow_exit_reason(),
      /*expected_bucket_count=*/1);
  if (payflow_exit_reason() != EwalletFlowExitedReason::kNotInAllowlist &&
      payflow_exit_reason() != EwalletFlowExitedReason::kLinkIsInvalid) {
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Ewallet.PayflowExitedReason.",
                      GetSchemeString()}),
        /*sample=*/payflow_exit_reason(),
        /*expected_bucket_count=*/1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsEwalletExitedReasonTest,
    testing::Combine(
        testing::Values(EwalletFlowExitedReason::kLinkIsInvalid,
                        EwalletFlowExitedReason::kUserOptedOut,
                        EwalletFlowExitedReason::kNoSupportedEwallet,
                        EwalletFlowExitedReason::kLandscapeScreenOrientation,
                        EwalletFlowExitedReason::kNotInAllowlist,
                        EwalletFlowExitedReason::kApiClientNotAvailable,
                        EwalletFlowExitedReason::kRiskDataEmpty,
                        EwalletFlowExitedReason::kClientTokenNotAvailable,
                        EwalletFlowExitedReason::kInitiatePaymentFailed,
                        EwalletFlowExitedReason::kActionTokenNotAvailable,
                        EwalletFlowExitedReason::kUserLoggedOut,
                        EwalletFlowExitedReason::kFopSelectorClosedNotByUser,
                        EwalletFlowExitedReason::kFopSelectorClosedByUser,
                        EwalletFlowExitedReason::kFoldableDevice,
                        EwalletFlowExitedReason::kMaxStrikes),
        testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                        PaymentLinkValidator::Scheme::kShopeePay,
                        PaymentLinkValidator::Scheme::kTngd)));

class FacilitatedPaymentsMetricsPixExitedReasonTest
    : public testing::TestWithParam<PixFlowExitedReason> {};

TEST_P(FacilitatedPaymentsMetricsPixExitedReasonTest, LogPixFlowExitedReason) {
  base::HistogramTester histogram_tester;

  LogPixFlowExitedReason(GetParam());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/GetParam(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsPixExitedReasonTest,
    testing::Values(PixFlowExitedReason::kCodeValidatorFailed,
                    PixFlowExitedReason::kInvalidCode,
                    PixFlowExitedReason::kUserOptedOut,
                    PixFlowExitedReason::kNoLinkedAccount,
                    PixFlowExitedReason::kLandscapeScreenOrientation,
                    PixFlowExitedReason::kApiClientNotAvailable,
                    PixFlowExitedReason::kRiskDataNotAvailable,
                    PixFlowExitedReason::kClientTokenNotAvailable,
                    PixFlowExitedReason::kInitiatePaymentFailed,
                    PixFlowExitedReason::kActionTokenNotAvailable,
                    PixFlowExitedReason::kUserLoggedOut,
                    PixFlowExitedReason::kFopSelectorClosedNotByUser,
                    PixFlowExitedReason::kFopSelectorClosedByUser));

class FacilitatedPaymentsMetricsUkmTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogPixCodeCopied) {
  LogPixCodeCopied(ukm::UkmRecorder::GetNewSourceID());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeCopied::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeCopied::kPixCodeCopiedName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PixCodeCopied"), true);
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogEwalletPaymentLinkDetectedUkm) {
  LogPaymentLinkDetected(ukm::UkmRecorder::GetNewSourceID());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PaymentLinkDetected::kEntryName,
      {ukm::builders::FacilitatedPayments_PaymentLinkDetected::
           kPaymentLinkDetectedName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PaymentLinkDetected"), true);
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogEwalletFopSelectorShownUkm) {
  size_t index = 0;
  for (PaymentLinkValidator::Scheme scheme :
       {PaymentLinkValidator::Scheme::kDuitNow,
        PaymentLinkValidator::Scheme::kShopeePay,
        PaymentLinkValidator::Scheme::kTngd}) {
    LogEwalletFopSelectorShownUkm(ukm::UkmRecorder::GetNewSourceID(), scheme);

    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::kEntryName,
        {ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
             kShownName,
         ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
             kSchemeName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index].metrics.at("Shown"), true);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Scheme"),
              static_cast<uint8_t>(scheme));
  }
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogPixFopSelectorShownUkm) {
  LogPixFopSelectorShownUkm(ukm::UkmRecorder::GetNewSourceID());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogPixFopSelectorResult) {
  size_t index = 0;
  for (bool accepted : {true, false}) {
    LogPixFopSelectorResultUkm(accepted, ukm::UkmRecorder::GetNewSourceID());

    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"), accepted);
  }
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogEwalletFopSelectorResult) {
  size_t index = 0;
  for (bool accepted : {true, false}) {
    for (PaymentLinkValidator::Scheme scheme :
         {PaymentLinkValidator::Scheme::kDuitNow,
          PaymentLinkValidator::Scheme::kShopeePay,
          PaymentLinkValidator::Scheme::kTngd}) {
      LogEwalletFopSelectorResultUkm(
          accepted, ukm::UkmRecorder::GetNewSourceID(), scheme);

      auto ukm_entries = ukm_recorder_.GetEntries(
          ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::
              kEntryName,
          {ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::
               kResultName,
           ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
               kSchemeName});
      ASSERT_EQ(ukm_entries.size(), index + 1);
      EXPECT_EQ(ukm_entries[index].metrics.at("Result"), accepted);
      EXPECT_EQ(ukm_entries[index++].metrics.at("Scheme"),
                static_cast<uint8_t>(scheme));
    }
  }
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogInitiatePurchaseActionResultUkm) {
  size_t index = 0;
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    LogInitiatePurchaseActionResultUkm(result,
                                       ukm::UkmRecorder::GetNewSourceID());

    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
            kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"),
              static_cast<uint8_t>(result));
  }
}

class FacilitatedPaymentsMetricsParameterizedTest
    : public testing::TestWithParam<
          std::tuple<FacilitatedPaymentsType, PaymentLinkValidator::Scheme>> {
 public:
  FacilitatedPaymentsType payment_type() const {
    return std::get<0>(GetParam());
  }

  PaymentLinkValidator::Scheme scheme() const {
    return std::get<1>(GetParam());
  }

  std::string GetFacilitatedPaymentsTypeString() const {
    switch (payment_type()) {
      case FacilitatedPaymentsType::kEwallet:
        return "Ewallet";
      case FacilitatedPaymentsType::kPix:
        return "Pix";
    }
  }

  std::string GetSchemeString() const {
    switch (scheme()) {
      case PaymentLinkValidator::Scheme::kDuitNow:
        return "DuitNow";
      case PaymentLinkValidator::Scheme::kShopeePay:
        return "ShopeePay";
      case PaymentLinkValidator::Scheme::kTngd:
        return "Tngd";
      case PaymentLinkValidator::Scheme::kInvalid:
        NOTREACHED();
    }
  }

  std::string GetFopSelectorShownLatencyString() const {
    switch (payment_type()) {
      case FacilitatedPaymentsType::kEwallet:
        return "LatencyAfterDetectingPaymentLink";
      case FacilitatedPaymentsType::kPix:
        return "LatencyAfterCopy";
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsParameterizedTest,
    testing::Combine(testing::Values(FacilitatedPaymentsType::kEwallet,
                                     FacilitatedPaymentsType::kPix),
                     testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                                     PaymentLinkValidator::Scheme::kShopeePay,
                                     PaymentLinkValidator::Scheme::kTngd)));

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogApiAvailabilityCheckResultAndLatency_Success) {
  base::HistogramTester histogram_tester;

  LogApiAvailabilityCheckResultAndLatency(payment_type(), /*result=*/true,
                                          base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".IsApiAvailable.Success.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.Ewallet.IsApiAvailable.Success.Latency.",
           GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogApiAvailabilityCheckResultAndLatency_Failure) {
  base::HistogramTester histogram_tester;

  LogApiAvailabilityCheckResultAndLatency(payment_type(), /*result=*/false,
                                          base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".IsApiAvailable.Failure.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.Ewallet.IsApiAvailable.Failure.Latency.",
           GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogGetClientTokenResultAndLatency_Success) {
  base::HistogramTester histogram_tester;

  LogGetClientTokenResultAndLatency(payment_type(), /*result=*/true,
                                    base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".GetClientToken.Success.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.Ewallet.GetClientToken.Success.Latency.",
           GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogGetClientTokenResultAndLatency_Failure) {
  base::HistogramTester histogram_tester;

  LogGetClientTokenResultAndLatency(payment_type(), /*result=*/false,
                                    base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".GetClientToken.Failure.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.Ewallet.GetClientToken.Failure.Latency.",
           GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogLoadRiskDataResult_Success) {
  base::HistogramTester histogram_tester;

  LogLoadRiskDataResultAndLatency(payment_type(), /*was_successful=*/true,
                                  base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".LoadRiskData.Success.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.Ewallet.LoadRiskData.Success.Latency.",
                    GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogLoadRiskDataResult_Failure) {
  base::HistogramTester histogram_tester;

  LogLoadRiskDataResultAndLatency(payment_type(), /*was_successful=*/false,
                                  base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".LoadRiskData.Failure.Latency"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.Ewallet.LoadRiskData.Failure.Latency.",
                    GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogInitiatePurchaseActionAttempt) {
  base::HistogramTester histogram_tester;

  LogInitiatePurchaseActionAttempt(payment_type(), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePurchaseAction.Attempt"}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.Ewallet.InitiatePurchaseAction.Attempt.",
           GetSchemeString()}),
      /*sample=*/true,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogInitiatePaymentResultAndLatency_Success) {
  base::HistogramTester histogram_tester;

  LogInitiatePaymentResultAndLatency(payment_type(), /*result=*/true,
                                     base::Milliseconds(10), scheme());

  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePayment.Success.Latency"}),
      /*sample=*/10,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePayment.Success.Latency.", GetSchemeString()}),
      /*sample=*/10,
      /*expected_count=*/payment_type() == FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogInitiatePaymentResultAndLatency_Failure) {
  base::HistogramTester histogram_tester;

  LogInitiatePaymentResultAndLatency(payment_type(), /*result=*/false,
                                     base::Milliseconds(10), scheme());

  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePayment.Failure.Latency"}),
      /*sample=*/10,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePayment.Failure.Latency.", GetSchemeString()}),
      /*sample=*/10,
      /*expected_count=*/payment_type() == FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest, LogInitiatePaymentAttempt) {
  base::HistogramTester histogram_tester;

  LogInitiatePaymentAttempt(payment_type(), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".InitiatePayment.Attempt"}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.Ewallet.InitiatePayment.Attempt.",
                    GetSchemeString()}),
      /*sample=*/true,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

TEST_P(FacilitatedPaymentsMetricsParameterizedTest,
       LogFopSelectorShownLatency) {
  base::HistogramTester histogram_tester;

  LogFopSelectorShownLatency(payment_type(), base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".FopSelectorShown.", GetFopSelectorShownLatencyString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.Ewallet.FopSelectorShown."
                    "LatencyAfterDetectingPaymentLink.",
                    GetSchemeString()}),
      /*sample=*/10,
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

class FacilitatedPaymentsMetricsTestForUiScreens
    : public testing::TestWithParam<std::tuple<FacilitatedPaymentsType,
                                               PaymentLinkValidator::Scheme,
                                               UiState>> {
 public:
  FacilitatedPaymentsType payment_type() const {
    return std::get<0>(GetParam());
  }

  PaymentLinkValidator::Scheme scheme() const {
    return std::get<1>(GetParam());
  }

  UiState ui_screen() { return std::get<2>(GetParam()); }

  std::string GetFacilitatedPaymentsTypeString() const {
    switch (payment_type()) {
      case FacilitatedPaymentsType::kEwallet:
        return "Ewallet";
      case FacilitatedPaymentsType::kPix:
        return "Pix";
    }
  }

  std::string GetSchemeString() const {
    switch (scheme()) {
      case PaymentLinkValidator::Scheme::kDuitNow:
        return "DuitNow";
      case PaymentLinkValidator::Scheme::kShopeePay:
        return "ShopeePay";
      case PaymentLinkValidator::Scheme::kTngd:
        return "Tngd";
      case PaymentLinkValidator::Scheme::kInvalid:
        NOTREACHED();
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsTestForUiScreens,
    testing::Combine(testing::Values(FacilitatedPaymentsType::kEwallet,
                                     FacilitatedPaymentsType::kPix),
                     testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                                     PaymentLinkValidator::Scheme::kShopeePay,
                                     PaymentLinkValidator::Scheme::kTngd),
                     testing::Values(UiState::kFopSelector,
                                     UiState::kProgressScreen,
                                     UiState::kErrorScreen)));

TEST_P(FacilitatedPaymentsMetricsTestForUiScreens, LogUiScreenShown) {
  base::HistogramTester histogram_tester;

  LogUiScreenShown(payment_type(), ui_screen(), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".UiScreenShown"}),
      /*sample=*/ui_screen(),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", GetFacilitatedPaymentsTypeString(),
                    ".UiScreenShown.", GetSchemeString()}),
      /*sample=*/ui_screen(),
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

}  // namespace payments::facilitated
