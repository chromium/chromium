// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/refill_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

class RefillMetricsTest : public AutofillMetricsBaseTest, public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  void FillForm(FormData form, FillingPayload filling_payload) {
    test_api(autofill_manager())
        .form_filler()
        .FillOrPreviewForm(
            mojom::ActionPersistence::kFill, form, filling_payload,
            *autofill_manager().FindCachedFormById(form.global_id()),
            *autofill_manager().GetAutofillField(
                form.global_id(), form.fields().front().global_id()),
            AutofillTriggerSource::kPopup);
  }
};

// Test that for a form that changed its structure after being seen, second
// FormsSeen call triggers a refill, RefillTriggerReason metric gets reported.
TEST_F(RefillMetricsTest, RefillTriggerReason_FormChanged) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     .autocomplete_attribute = "cc-exp"}}});
  SeeForm({form});

  CreditCard credit_card = test::GetCreditCard();
  FillForm(form, &credit_card);

  base::HistogramTester histogram_tester;
  form.set_url(GURL("https://foo.com/bar"));
  SeeForm({form});

  histogram_tester.ExpectUniqueSample("Autofill.RefillTriggerReason",
                                      RefillTriggerReason::kFormChanged, 1);
}

// Test that for a form that was seen and filled, OnSelectFieldOptionsDidChange
// triggers a refill, RefillTriggerReason metric gets reported.
TEST_F(RefillMetricsTest, RefillTriggerReason_OnSelectFieldOptionsDidChange) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     .autocomplete_attribute = "cc-exp"}}});
  SeeForm({form});

  CreditCard credit_card = test::GetCreditCard();
  FillForm(form, &credit_card);

  base::HistogramTester histogram_tester;
  autofill_manager().OnSelectFieldOptionsDidChange(form);

  histogram_tester.ExpectUniqueSample(
      "Autofill.RefillTriggerReason",
      RefillTriggerReason::kSelectOptionsChanged, 1);
}

// Test that in case an expiration date refill is triggered,
// RefillTriggerReason metric gets reported.
TEST_F(RefillMetricsTest,
       RefillTriggerReason_OnJavaScriptChangedAutofilledValue) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     .autocomplete_attribute = "cc-exp"}}});
  SeeForm({form});

  CreditCard credit_card = test::GetCreditCard();
  FillForm(form, &credit_card);

  // Simulate that JavaScript modifies the expiration date field incorrectly.
  FormData form_after_js_modification = form;
  test_api(form_after_js_modification).field(2).set_value(u"04 / 20");
  test_api(form_after_js_modification).field(2).set_is_autofilled(true);

  base::HistogramTester histogram_tester;
  autofill_manager().OnJavaScriptChangedAutofilledValue(
      form_after_js_modification,
      form_after_js_modification.fields()[2].global_id(), u"04/2099");

  histogram_tester.ExpectUniqueSample(
      "Autofill.RefillTriggerReason",
      RefillTriggerReason::kExpirationDateFormatted, 1);
}

}  // namespace
}  // namespace autofill::autofill_metrics
