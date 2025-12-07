// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/per_fill_metrics.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

using ::testing::DoAll;
using ::testing::Return;

// Action `SaveArgElementsTo<k>(pointer)` saves the value pointed to by the
// `k`th (0-based) argument of the mock function by moving it to `*pointer`.
ACTION_TEMPLATE(SaveArgElementsTo,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  auto span = testing::get<k>(args);
  pointer->assign(span.begin(), span.end());
}

class PerFillMetricsTest : public AutofillMetricsBaseTest,
                           public testing::Test {
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

  // Lets `BrowserAutofillManager` fill `form` with `filling_payload` and
  // returns `form` as it would be extracted from the renderer afterwards, i.e.,
  // with the autofilled `FormFieldData::value`s.
  FormData FillFormAndGetFilledVersion(FormData form,
                                       FillingPayload filling_payload) {
    std::vector<FormFieldData> filled_fields;
    // After the call, `filled_fields` will only contain the fields that were
    // autofilled in this call of FillOrPreviewForm (% fields not filled due
    // to the iframe security policy).
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(
            SaveArgElementsTo<2>(&filled_fields),
            Return(base::ToVector(form.fields(), &FormFieldData::global_id))));
    FillForm(form, filling_payload);
    // Copy the filled data into the form.
    for (FormFieldData& field : test_api(form).fields()) {
      if (auto it = std::ranges::find(filled_fields, field.global_id(),
                                      &FormFieldData::global_id);
          it != filled_fields.end()) {
        field = *it;
      }
    }
    return form;
  }
};

// Tests that for a form fill, Autofill.NumberOfFieldsPerAutofill is emitted for
// the actually filled fields.
TEST_F(PerFillMetricsTest, FillForm) {
  base::HistogramTester histogram_tester;
  AutofillProfile autofill_profile = test::GetFullProfile();

  FormData form = test::GetFormData({.fields = {{.role = NAME_FIRST},
                                                {.role = NAME_LAST},
                                                {.role = ADDRESS_HOME_LINE1},
                                                {.role = ADDRESS_HOME_ZIP},
                                                {.role = CREDIT_CARD_NUMBER}}});
  SeeForm({form});

  // Only the first three fields are actually filled.
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce(Return(base::ToVector(base::span(form.fields()).first(3u),
                                      &FormFieldData::global_id)));
  FillForm(form, &autofill_profile);

  histogram_tester.ExpectUniqueSample("Autofill.NumberOfFieldsPerAutofill", 3,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfFieldsPerAutofill.AutofillProfile", 3, 1);
}

// Test that for a form that changed its structure after being seen, second
// FormsSeen call triggers a refill, RefillTriggerReason metric gets reported.
TEST_F(PerFillMetricsTest, RefillTriggerReason_FormChanged) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     .autocomplete_attribute = "cc-exp"}}});
  SeeForm({form});

  CreditCard credit_card = test::GetCreditCard();
  FillForm(form, &credit_card);

  base::HistogramTester histogram_tester;
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields.push_back(fields.back());
  form.set_fields(std::move(fields));

  SeeForm({form});

  histogram_tester.ExpectUniqueSample("Autofill.RefillTriggerReason",
                                      RefillTriggerReason::kFormChanged, 1);
}

// Test that for a form that was seen and filled, OnSelectFieldOptionsDidChange
// triggers a refill, RefillTriggerReason metric gets reported.
TEST_F(PerFillMetricsTest, RefillTriggerReason_OnSelectFieldOptionsDidChange) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     .autocomplete_attribute = "cc-exp"}}});
  SeeForm({form});

  CreditCard credit_card = test::GetCreditCard();
  FillForm(form, &credit_card);

  base::HistogramTester histogram_tester;
  autofill_manager().OnSelectFieldOptionsDidChange(
      form, form.fields().back().global_id());

  histogram_tester.ExpectUniqueSample(
      "Autofill.RefillTriggerReason",
      RefillTriggerReason::kSelectOptionsChanged, 1);
}

// Test that in case an expiration date refill is triggered,
// RefillTriggerReason metric gets reported.
TEST_F(PerFillMetricsTest,
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

// Test that we correctly log the number of modified fields during a refill.
TEST_F(PerFillMetricsTest, ModifiedFieldsCount) {
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FIRST},
                                    {.role = CREDIT_CARD_NUMBER}}});
  autofill_manager().AddSeenForm(form,
                                 {CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER});

  CreditCard credit_card = test::GetCreditCard();
  form = FillFormAndGetFilledVersion(form, &credit_card);

  base::HistogramTester histogram_tester;
  test_api(form).fields().emplace_back();

  // Mock the router not blocking any field for filling.
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce([](mojom::FormActionType action_type,
                   mojom::ActionPersistence action_persistence,
                   base::span<const FormFieldData> data,
                   const url::Origin& triggered_origin,
                   const base::flat_map<FieldGlobalId, FieldType>&,
                   const Section&) {
        return base::ToVector(data, &FormFieldData::global_id);
      });

  autofill_manager().AddSeenForm(
      form,
      {CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER, CREDIT_CARD_NAME_LAST});

  // Since the previously filled two fields should remain unchanged, we expect
  // only the newly added field to be modified by the refill operation.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Refill.ModifiedFieldsCount.FormChanged", 1, 1);
}

}  // namespace
}  // namespace autofill::autofill_metrics
