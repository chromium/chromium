// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// Parameterized test that tests all possible combinations of better auth
// logging based on the criteria of the user having a local card present, a
// masked server card present, and being on a device that is FIDO eligible.
// Params of the BetterAuthMetricsTest:
// -- bool include_local_credit_card;
// -- bool include_masked_server_credit_card;
// -- bool is_user_opted_in_to_fido;
// -- bool is_unmask_details_request_in_progress;
class BetterAuthMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  BetterAuthMetricsTest() = default;
  ~BetterAuthMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  FormData SetUpCreditCardUnmaskingPreflightCallTest() {
    CreditCardAccessManager& access_manager =
        autofill_manager().GetCreditCardAccessManager();
    test_api(access_manager)
        .set_unmask_details_request_in_progress(
            IsUnmaskDetailsRequestInProgress());
    static_cast<TestCreditCardFidoAuthenticator*>(
        access_manager.GetOrCreateFidoAuthenticator())
        ->set_is_user_opted_in(IsUserOptedInToFido());

    RecreateCreditCards(
        /*include_local_credit_card=*/std::get<0>(GetParam()),
        /*include_masked_server_credit_card=*/std::get<1>(GetParam()),
        /*masked_card_is_enrolled_for_virtual_card=*/false);
    FormData form = CreateForm({test::CreateTestFormField(
        "Credit card", "cardnum", "", FormControlType::kInputText)});
    std::vector<FieldType> field_types = {CREDIT_CARD_NUMBER};
    autofill_manager().AddSeenForm(form, field_types);
    return form;
  }

  bool HasServerCard() const { return std::get<1>(GetParam()); }

  bool IsUserOptedInToFido() const { return std::get<2>(GetParam()); }

  bool IsUnmaskDetailsRequestInProgress() const {
    return std::get<3>(GetParam());
  }

  const std::string kPreflightCallMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";
  const std::string kPreflightLatencyMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";
  const std::string kPreflightFlowInitiatedMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightInitiated";
};

// Test that we log preflight calls for credit card unmasking when the user is
// FIDO eligible, and we have a server card present.
TEST_P(BetterAuthMetricsTest, CreditCardUnmaskingPreflightCall_FidoEligible) {
  base::HistogramTester histogram_tester;
  const FormData& form = SetUpCreditCardUnmaskingPreflightCallTest();
  SetFidoEligibility(/*is_verifiable=*/true);

  // Check that the correct metrics are logged even if suggestions are shown
  // multiple times in a row.
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);

  // If a server card is available, and a previous request was not made, then a
  // preflight flow is initiated and a preflight call is made.
  if (HasServerCard() && !IsUnmaskDetailsRequestInProgress()) {
    histogram_tester.ExpectUniqueSample(/*name=*/kPreflightCallMetrics,
                                        /*sample=*/IsUserOptedInToFido(),
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(/*name=*/kPreflightFlowInitiatedMetrics,
                                        /*sample=*/true,
                                        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                      /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                      /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(
      /*name=*/kPreflightLatencyMetrics,
      /*expected_count=*/HasServerCard() && !IsUnmaskDetailsRequestInProgress()
          ? 1
          : 0);
}

// Test that we do not log preflight calls for credit card unmasking when the
// user is not FIDO eligible, even if we have a server card present.
TEST_P(BetterAuthMetricsTest,
       CreditCardUnmaskingPreflightCall_NotFidoEligible) {
  base::HistogramTester histogram_tester;
  const FormData& form = SetUpCreditCardUnmaskingPreflightCallTest();
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);

  // If the preflight flow is initiated, we will always log it.
  if (HasServerCard() && !IsUnmaskDetailsRequestInProgress()) {
    histogram_tester.ExpectUniqueSample(/*name=*/kPreflightFlowInitiatedMetrics,
                                        /*sample*/ true,
                                        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                      /*expected_count=*/0);
  }
  // If user is not verifiable, then no preflight call is made.
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(,
                         BetterAuthMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace autofill::autofill_metrics
