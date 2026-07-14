// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"

#import "base/functional/function_ref.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill::autofill_metrics {

namespace {

struct TestParams {
  bool is_upload;
  SaveCreditCardPromptOverlayType overlay_type;
  payments::PaymentsAutofillClient::CardSaveType card_save_type;
  int num_strikes;
  bool request_name;
  bool request_expiry;
};

void RunForAllParams(base::FunctionRef<void(const TestParams&)> test_body) {
  bool is_upload_save_cases[] = {true, false};
  SaveCreditCardPromptOverlayType overlay_type_cases[] = {
      SaveCreditCardPromptOverlayType::kBanner,
      SaveCreditCardPromptOverlayType::kBottomSheet};
  payments::PaymentsAutofillClient::CardSaveType card_save_type_cases[] = {
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc,
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly};
  int num_strikes_cases[] = {0, 1, 2};
  bool request_name_cases[] = {true, false};
  bool request_expiry_cases[] = {true, false};

  for (bool is_upload : is_upload_save_cases) {
    for (SaveCreditCardPromptOverlayType overlay_type : overlay_type_cases) {
      for (payments::PaymentsAutofillClient::CardSaveType card_save_type :
           card_save_type_cases) {
        for (int num_strikes : num_strikes_cases) {
          for (bool request_name : request_name_cases) {
            for (bool request_expiry : request_expiry_cases) {
              SCOPED_TRACE(
                  testing::Message()
                  << "is_upload: " << is_upload
                  << ", overlay_type: " << static_cast<int>(overlay_type)
                  << ", card_save_type: " << static_cast<int>(card_save_type)
                  << ", num_strikes: " << num_strikes << ", request_name: "
                  << request_name << ", request_expiry: " << request_expiry);
              test_body({is_upload, overlay_type, card_save_type, num_strikes,
                         request_name, request_expiry});
            }
          }
        }
      }
    }
  }
}

payments::PaymentsAutofillClient::SaveCreditCardOptions GetOptions(
    const TestParams& params) {
  return payments::PaymentsAutofillClient::SaveCreditCardOptions()
      .with_card_save_type(params.card_save_type)
      .with_num_strikes(params.num_strikes)
      .with_should_request_name_from_user(params.request_name)
      .with_should_request_expiration_date_from_user(params.request_expiry);
}

std::string GetBaseHistogramSuffix(const TestParams& params) {
  std::string destination = params.is_upload ? ".Server" : ".Local";
  std::string overlay =
      (params.overlay_type == SaveCreditCardPromptOverlayType::kBanner)
          ? ".Banner"
          : ".BottomSheet";
  std::string save_type =
      (params.card_save_type ==
       payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc)
          ? ".SavingWithCvc"
          : ".SavingWithoutCvc";
  return base::StrCat({destination, overlay, save_type});
}

std::string GetHistogramSuffix(const TestParams& params) {
  std::string destination = params.is_upload ? ".Server" : ".Local";
  std::string overlay =
      (params.overlay_type == SaveCreditCardPromptOverlayType::kBanner)
          ? ".Banner"
          : ".BottomSheet";
  std::string save_type =
      (params.card_save_type ==
       payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc)
          ? ".SavingWithCvc"
          : ".SavingWithoutCvc";
  std::string fix_flow;
  if (params.request_name && params.request_expiry) {
    fix_flow = ".RequestingCardHolderNameAndExpiryDate";
  } else if (params.request_name) {
    fix_flow = ".RequestingCardHolderName";
  } else if (params.request_expiry) {
    fix_flow = ".RequestingExpiryDate";
  } else {
    fix_flow = ".NoFixFlow";
  }
  return base::StrCat({destination, overlay, ".NumStrikes.",
                       base::NumberToString(params.num_strikes), fix_flow,
                       save_type});
}

}  // namespace

using CreditCardSaveMetricsIosTest = PlatformTest;

// Tests that LogSaveCreditCardPromptOfferMetricIos correctly logs for all
// combinations of parameters.
TEST_F(CreditCardSaveMetricsIosTest,
       LogSaveCreditCardPromptOfferMetricIos_AllCombinations) {
  const SaveCardPromptOffer kMetric = SaveCardPromptOffer::kShown;

  RunForAllParams([](const TestParams& params) {
    base::HistogramTester histogram_tester;

    LogSaveCreditCardPromptOfferMetricIos(
        kMetric, params.is_upload, GetOptions(params), params.overlay_type);

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS",
                      GetBaseHistogramSuffix(params)}),
        kMetric, 1);

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS",
                      GetHistogramSuffix(params)}),
        kMetric, 1);
  });
}

// Tests that LogSaveCreditCardPromptResultIOS correctly logs for all
// combinations of parameters.
TEST_F(CreditCardSaveMetricsIosTest,
       LogSaveCreditCardPromptResultIOS_AllCombinations) {
  const SaveCreditCardPromptResultIOS kMetric =
      SaveCreditCardPromptResultIOS::kAccepted;

  RunForAllParams([](const TestParams& params) {
    base::HistogramTester histogram_tester;

    LogSaveCreditCardPromptResultIOS(kMetric, params.is_upload,
                                     GetOptions(params), params.overlay_type);

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Autofill.SaveCreditCardPromptResult.IOS",
                      GetHistogramSuffix(params)}),
        kMetric, 1);
  });
}

// Tests CVC offer and result logging.
TEST_F(CreditCardSaveMetricsIosTest, LogSaveCvcPromptMetrics) {
  {
    base::HistogramTester histogram_tester;
    LogSaveCvcPromptOfferedIOS(/*is_uploading=*/true);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCvcPromptOffer.IOS.Upload", SaveCardPromptOffer::kShown,
        1);
  }
  {
    base::HistogramTester histogram_tester;
    LogSaveCvcPromptOfferedIOS(/*is_uploading=*/false);
    histogram_tester.ExpectUniqueSample("Autofill.SaveCvcPromptOffer.IOS.Local",
                                        SaveCardPromptOffer::kShown, 1);
  }
  {
    base::HistogramTester histogram_tester;
    payments::PaymentsAutofillClient::SaveCreditCardOptions options;
    LogSaveCvcPromptResultIOS(SaveCvcPromptResultIOS::kAccepted,
                              /*is_uploading=*/true, options);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCvcPromptResult.IOS.Upload",
        SaveCvcPromptResultIOS::kAccepted, 1);
  }
  {
    base::HistogramTester histogram_tester;
    payments::PaymentsAutofillClient::SaveCreditCardOptions options;
    LogSaveCvcPromptResultIOS(SaveCvcPromptResultIOS::kTimedOut,
                              /*is_uploading=*/false, options);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCvcPromptResult.IOS.Local",
        SaveCvcPromptResultIOS::kTimedOut, 1);
  }
}

}  // namespace autofill::autofill_metrics
