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

std::string GetSchemeString(PaymentLinkValidator::Scheme scheme) {
  switch (scheme) {
    case PaymentLinkValidator::Scheme::kDuitNow:
      return "DuitNow";
    case PaymentLinkValidator::Scheme::kShopeePay:
      return "ShopeePay";
    case PaymentLinkValidator::Scheme::kTngd:
      return "Tngd";
    case PaymentLinkValidator::Scheme::kPromptPay:
      return "PromptPay";
    case PaymentLinkValidator::Scheme::kMomo:
      return "Momo";
    case PaymentLinkValidator::Scheme::kDana:
      return "Dana";
    case PaymentLinkValidator::Scheme::kInvalid:
      NOTREACHED();
  }
}

struct A2AInvokePaymentAppMetricsTestParam {
  bool result;
  PaymentLinkValidator::Scheme scheme;
};

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

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.PaymentLinkDetected",
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
      PixCodeValidationResult::kValidatorFailed, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result",
      /*sample=*/PixCodeValidationResult::kValidatorFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.ValidatorFailed.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_InvalidCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(PixCodeValidationResult::kInvalid,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result",
      /*sample=*/PixCodeValidationResult::kInvalid,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.InvalidCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_DynamicCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(PixCodeValidationResult::kDynamic,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result",
      /*sample=*/PixCodeValidationResult::kDynamic,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.DynamicCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_StaticCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(PixCodeValidationResult::kStatic,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result",
      /*sample=*/PixCodeValidationResult::kStatic,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.StaticCode.Latency",
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

TEST(FacilitatedPaymentsMetricsTest, LogPixAccountLinkingPromptShown) {
  base::HistogramTester histogram_tester;

  LogPixAccountLinkingPromptShown();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.PromptShown",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogPixAccountLinkingPromptAccepted) {
  base::HistogramTester histogram_tester;

  LogPixAccountLinkingPromptAccepted();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.PromptAccepted",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

class FacilitatedPaymentsMetricsPixAccountLinkingFlowExitedReasonTest
    : public testing::TestWithParam<PixAccountLinkingFlowExitedReason> {};

TEST_P(FacilitatedPaymentsMetricsPixAccountLinkingFlowExitedReasonTest,
       LogPixAccountLinkingFlowExitedReason) {
  base::HistogramTester histogram_tester;

  LogPixAccountLinkingFlowExitedReason(GetParam());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/GetParam(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsPixAccountLinkingFlowExitedReasonTest,
    testing::Values(
        PixAccountLinkingFlowExitedReason::kScreenNotShown,
        PixAccountLinkingFlowExitedReason::kScreenClosedNotByUser,
        PixAccountLinkingFlowExitedReason::kScreenClosedByUser,
        PixAccountLinkingFlowExitedReason::kUserDeclined,
        PixAccountLinkingFlowExitedReason::kWalletNotInstalled,
        PixAccountLinkingFlowExitedReason::kWalletVersionNotSupported,
        PixAccountLinkingFlowExitedReason::kUserOptedOut,
        PixAccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup,
        PixAccountLinkingFlowExitedReason::kServerSideIneligible,
        PixAccountLinkingFlowExitedReason::kTabIsNotActive,
        PixAccountLinkingFlowExitedReason::kUserSwitchedWebsite));

TEST(FacilitatedPaymentsMetricsTest,
     LogGetDetailsForCreatePaymentInstrumentResultAndLatency) {
  base::HistogramTester histogram_tester;

  LogGetDetailsForCreatePaymentInstrumentResultAndLatency(
      true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
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
                      GetSchemeString(scheme())}),
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
                        EwalletFlowExitedReason::kMaxStrikes,
                        EwalletFlowExitedReason::kOtherFopSelected),
        testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                        PaymentLinkValidator::Scheme::kShopeePay,
                        PaymentLinkValidator::Scheme::kTngd,
                        PaymentLinkValidator::Scheme::kMomo,
                        PaymentLinkValidator::Scheme::kDana)));

class FacilitatedPaymentsMetricsA2AExitedReasonTest
    : public testing::TestWithParam<
          std::tuple<A2AFlowExitedReason, PaymentLinkValidator::Scheme>> {
 public:
  A2AFlowExitedReason payflow_exit_reason() const {
    return std::get<0>(GetParam());
  }

  PaymentLinkValidator::Scheme scheme() const {
    return std::get<1>(GetParam());
  }
};

TEST_P(FacilitatedPaymentsMetricsA2AExitedReasonTest,
       LogA2APayflowExitedReason) {
  base::HistogramTester histogram_tester;

  LogA2APayflowExitedReason(payflow_exit_reason(), scheme());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      /*sample=*/payflow_exit_reason(),
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.A2A.PayflowExitedReason.",
                    GetSchemeString(scheme())}),
      /*sample=*/payflow_exit_reason(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsA2AExitedReasonTest,
    testing::Combine(
        testing::Values(A2AFlowExitedReason::kNotInAllowlist,
                        A2AFlowExitedReason::kUserOptedOut,
                        A2AFlowExitedReason::kNoSupportedPaymentApp,
                        A2AFlowExitedReason::kFopSelectorClosedNotByUser,
                        A2AFlowExitedReason::kFopSelectorClosedByUser,
                        A2AFlowExitedReason::kOtherFopSelected,
                        A2AFlowExitedReason::kFlagNotEnabled),
        testing::Values(PaymentLinkValidator::Scheme::kPromptPay)));

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
                    PixFlowExitedReason::kFopSelectorClosedByUser,
                    PixFlowExitedReason::kAutofillPaymentMethodsDisabled,
                    PixFlowExitedReason::kMerchantNotAllowlisted,
                    PixFlowExitedReason::kPixCodeInIFrame,
                    PixFlowExitedReason::kFrameNotActive));

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
        PaymentLinkValidator::Scheme::kTngd,
        PaymentLinkValidator::Scheme::kMomo,
        PaymentLinkValidator::Scheme::kDana}) {
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
          PaymentLinkValidator::Scheme::kTngd,
          PaymentLinkValidator::Scheme::kMomo,
          PaymentLinkValidator::Scheme::kDana}) {
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

class FacilitatedPaymentsFopSelectorTypesMetricsParameterizedTest
    : public testing::TestWithParam<std::tuple<PaymentLinkFopSelectorTypes,
                                               PaymentLinkFopSelectorAction,
                                               PaymentLinkValidator::Scheme>> {
 public:
  PaymentLinkFopSelectorTypes payment_link_fop_selector_type() const {
    return std::get<0>(GetParam());
  }

  PaymentLinkFopSelectorAction payment_link_fop_selector_action() const {
    return std::get<1>(GetParam());
  }

  PaymentLinkValidator::Scheme scheme() const {
    return std::get<2>(GetParam());
  }

  std::string GetPaymentLinkFopSelectorTypeString() const {
    switch (payment_link_fop_selector_type()) {
      case PaymentLinkFopSelectorTypes::kEwalletOnly:
        return "EwalletOnly";
      case PaymentLinkFopSelectorTypes::kA2AOnly:
        return "A2AOnly";
      case PaymentLinkFopSelectorTypes::kEwalletAndA2A:
        return "EwalletAndA2A";
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsFopSelectorTypesMetricsParameterizedTest,
    testing::Combine(
        testing::Values(PaymentLinkFopSelectorTypes::kEwalletOnly,
                        PaymentLinkFopSelectorTypes::kA2AOnly,
                        PaymentLinkFopSelectorTypes::kEwalletAndA2A),
        testing::Values(PaymentLinkFopSelectorAction::kEwalletSelected,
                        PaymentLinkFopSelectorAction::kPaymentAppSelected),
        testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                        PaymentLinkValidator::Scheme::kShopeePay,
                        PaymentLinkValidator::Scheme::kTngd,
                        PaymentLinkValidator::Scheme::kMomo,
                        PaymentLinkValidator::Scheme::kPromptPay)));

TEST_P(FacilitatedPaymentsFopSelectorTypesMetricsParameterizedTest,
       LogNonCardPaymentMethodsFopSelected) {
  base::HistogramTester histogram_tester;

  LogNonCardPaymentMethodsFopSelected(payment_link_fop_selector_type(),
                                      payment_link_fop_selector_action(),
                                      scheme());
  std::string type_string = GetPaymentLinkFopSelectorTypeString();
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"FacilitatedPayments.", type_string, ".FopSelector.UserAction"}),
      payment_link_fop_selector_action(),
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.", type_string,
                    ".FopSelector.UserAction.", GetSchemeString(scheme())}),
      payment_link_fop_selector_action(),
      /*expected_bucket_count=*/1);
}

TEST_P(FacilitatedPaymentsFopSelectorTypesMetricsParameterizedTest,
       LogPaymentLinkFopSelectorShownLatency) {
  base::HistogramTester histogram_tester;

  LogPaymentLinkFopSelectorShownLatency(payment_link_fop_selector_type(),
                                        base::Milliseconds(10), scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.",
                    GetPaymentLinkFopSelectorTypeString(),
                    ".FopSelectorShown.LatencyAfterDetectingPaymentLink"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.",
                    GetPaymentLinkFopSelectorTypeString(),
                    ".FopSelectorShown.LatencyAfterDetectingPaymentLink.",
                    GetSchemeString(scheme())}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

class FacilitatedPaymentsA2AInvokePaymentAppMetricsParameterizedTest
    : public testing::TestWithParam<A2AInvokePaymentAppMetricsTestParam> {
 public:
  bool result() const { return GetParam().result; }

  PaymentLinkValidator::Scheme scheme() const { return GetParam().scheme; }

  std::string GetResultString() const {
    return result() ? "Success" : "Failure";
  }
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsA2AInvokePaymentAppMetricsParameterizedTest,
    testing::Values(A2AInvokePaymentAppMetricsTestParam{
                        /*result=*/true,
                        /*scheme=*/PaymentLinkValidator::Scheme::kPromptPay},
                    A2AInvokePaymentAppMetricsTestParam{
                        /*result=*/false,
                        /*scheme=*/PaymentLinkValidator::Scheme::kPromptPay}));

TEST_P(FacilitatedPaymentsA2AInvokePaymentAppMetricsParameterizedTest,
       LogInvokePaymentAppResultAndLatency) {
  base::HistogramTester histogram_tester;

  LogInvokePaymentAppResultAndLatency(result(), base::Milliseconds(10),
                                      scheme());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.A2A.InvokePaymentApp.",
                    GetResultString(), ".LatencyAfterDetectingPaymentLink"}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"FacilitatedPayments.A2A.InvokePaymentApp.",
                    GetResultString(), ".LatencyAfterDetectingPaymentLink.",
                    GetSchemeString(scheme())}),
      /*sample=*/10,
      /*expected_bucket_count=*/1);
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
                                     PaymentLinkValidator::Scheme::kTngd,
                                     PaymentLinkValidator::Scheme::kMomo,
                                     PaymentLinkValidator::Scheme::kDana)));

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
           GetSchemeString(scheme())}),
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
           GetSchemeString(scheme())}),
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
           GetSchemeString(scheme())}),
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
           GetSchemeString(scheme())}),
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
                    GetSchemeString(scheme())}),
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
                    GetSchemeString(scheme())}),
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
           GetSchemeString(scheme())}),
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
                    ".InitiatePayment.Success.Latency.",
                    GetSchemeString(scheme())}),
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
                    ".InitiatePayment.Failure.Latency.",
                    GetSchemeString(scheme())}),
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
                    GetSchemeString(scheme())}),
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
                    GetSchemeString(scheme())}),
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
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsTestForUiScreens,
    testing::Combine(testing::Values(FacilitatedPaymentsType::kEwallet,
                                     FacilitatedPaymentsType::kPix),
                     testing::Values(PaymentLinkValidator::Scheme::kDuitNow,
                                     PaymentLinkValidator::Scheme::kShopeePay,
                                     PaymentLinkValidator::Scheme::kTngd,
                                     PaymentLinkValidator::Scheme::kMomo,
                                     PaymentLinkValidator::Scheme::kDana),
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
                    ".UiScreenShown.", GetSchemeString(scheme())}),
      /*sample=*/ui_screen(),
      /*expected_bucket_count=*/payment_type() ==
              FacilitatedPaymentsType::kEwallet
          ? 1
          : 0);
}

}  // namespace payments::facilitated
