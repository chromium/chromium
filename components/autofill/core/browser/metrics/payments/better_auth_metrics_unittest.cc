// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class BetterAuthMetricsTest : public metrics::AutofillMetricsBaseTest,
                              public testing::Test {
 public:
  BetterAuthMetricsTest() = default;
  ~BetterAuthMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Test that we log preflight calls for credit card unmasking.
TEST_F(BetterAuthMetricsTest, CreditCardUnmaskingPreflightCall) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillCreditCardAuthentication);
  std::string preflight_call_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightCalled";
  std::string preflight_latency_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";

  FormData form =
      CreateForm({CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Create local cards and set user as eligible for FIDO authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(/*include_local_credit_card=*/true,
                        /*include_masked_server_credit_card=*/false,
                        /*include_full_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                          form, form.fields[0]);
    // If no masked server cards are available, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create masked server cards and set user as ineligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(/*include_local_credit_card=*/false,
                        /*include_masked_server_credit_card=*/true,
                        /*include_full_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);
    SetFidoEligibility(false);
    autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                          form, form.fields[0]);
    // If user is not verifiable, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create full server cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(/*include_local_credit_card=*/false,
                        /*include_masked_server_credit_card=*/false,
                        /*include_full_server_credit_card=*/true,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);
    SetFidoEligibility(false);
    autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                          form, form.fields[0]);
    // If no masked server cards are available, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create masked server cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(/*include_local_credit_card=*/false,
                        /*include_masked_server_credit_card=*/true,
                        /*include_full_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                          form, form.fields[0]);
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = metrics::kTestMaskedCardId}));
    // Preflight call is made only if a masked server card is available and the
    // user is eligible for FIDO authentication (except iOS).
    histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 1);
  }

  {
    // Create all types of cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(/*include_local_credit_card=*/true,
                        /*include_masked_server_credit_card=*/true,
                        /*include_full_server_credit_card=*/true,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                          form, form.fields[0]);
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = metrics::kTestMaskedCardId}));
    // Preflight call is made only if a masked server card is available and the
    // user is eligible for FIDO authentication (except iOS).
    histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 1);
  }
}

}  // namespace autofill::autofill_metrics
