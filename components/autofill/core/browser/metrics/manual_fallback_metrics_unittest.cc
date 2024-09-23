// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

// Test parameter data for asserting that the expected metrics are emitted
// depending on the fallback option selected.
struct ManualFallbackTestParams {
  const AutofillSuggestionTriggerSource manual_fallback_option;
  const std::string test_name;
};

class ManualFallbackEventLoggerTest
    : public AutofillMetricsBaseTest,
      public testing::TestWithParam<ManualFallbackTestParams> {
 protected:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }

  // Show suggestions on the first field of the `form`.
  void ShowSuggestions(
      const FormData& form,
      AutofillSuggestionTriggerSource fallback_trigger_source) {
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields()[0].global_id(), fallback_trigger_source);
    DidShowAutofillSuggestions(
        form, /*field_index=*/0,
        fallback_trigger_source ==
                AutofillSuggestionTriggerSource::kManualFallbackAddress
            ? SuggestionType::kAddressEntry
            : SuggestionType::kCreditCardEntry);
  }

  // Fills the first field in the form by calling `FillOrPreviewField()`. Uses a
  // hardcoded value to be filled but makes the `type` passed to
  // the filling function depend on whether the `manual_fallback_option` param
  // attribute is `AutofillSuggestionTriggerSource::kManualFallbackAddress` or
  // `AutofillSuggestionTriggerSource::kManualFallbackPayments`. Using
  // `SuggestionType::kAddressFieldByFieldFilling` for the former and
  // `SuggestionType::kCreditCardFieldByFieldFilling` for the latter.
  void FillFirstFormField(const FormData& form) {
    const ManualFallbackTestParams& params = GetParam();
    autofill_manager().FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        form, form.fields()[0], u"value to fill",
        params.manual_fallback_option ==
                AutofillSuggestionTriggerSource::kManualFallbackAddress
            ? SuggestionType::kAddressFieldByFieldFilling
            : SuggestionType::kCreditCardFieldByFieldFilling,
        params.manual_fallback_option ==
                AutofillSuggestionTriggerSource::kManualFallbackAddress
            ? NAME_FULL
            : CREDIT_CARD_NAME_FULL);
  }

  std::string ExpectedBucketNameForManualFallbackOption() const {
    return GetParam().manual_fallback_option ==
                   AutofillSuggestionTriggerSource::kManualFallbackAddress
               ? "Address"
               : "CreditCard";
  }
};

// Tests that when suggestions on an unclassified field are shown and filled,
// the FillAfterSuggestion metric is emitted correctly. Note that only
// field-by-field filling is available in this flow.
TEST_P(ManualFallbackEventLoggerTest, FillAfterSuggestion_Filled) {
  FormData form = test::GetFormData(
      {.fields = {{.label = u"unclassified", .name = u"unclassified"}}});
  SeeForm(form);
  const ManualFallbackTestParams& params = GetParam();

  ShowSuggestions(form,
                  /*fallback_trigger_source=*/params.manual_fallback_option);
  // Fill the suggestion.
  FillFirstFormField(form);

  base::HistogramTester histogram_tester;
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.NotClassifiedAsTargetFilling."
      "FillAfterSuggestion." +
          ExpectedBucketNameForManualFallbackOption(),
      true, 1);
}

// Tests that when suggestions on an unclassified field are shown,
// but not selected, FillAfterSuggestion metric is
// emitted correctly. Note that only field-by-field filling is available in this
// flow.
TEST_P(ManualFallbackEventLoggerTest, FillAfterSuggestion_NotFilled) {
  FormData form = test::GetFormData(
      {.fields = {{.label = u"unclassified", .name = u"unclassified"}}});
  SeeForm(form);
  const ManualFallbackTestParams& params = GetParam();

  ShowSuggestions(form,
                  /*fallback_trigger_source=*/params.manual_fallback_option);

  base::HistogramTester histogram_tester;
  // No suggestion was selected.
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.NotClassifiedAsTargetFilling."
      "FillAfterSuggestion." +
          ExpectedBucketNameForManualFallbackOption(),
      false, 1);
}

// Tests that regular field-by-field filling suggestion acceptance (i.e, on a
// classified field), does not emit the metric.
TEST_P(ManualFallbackEventLoggerTest,
       FillAfterSuggestion_RegularFieldByFieldFillingSuggestion) {
  const ManualFallbackTestParams& params = GetParam();
  bool const is_address_filling =
      params.manual_fallback_option ==
      AutofillSuggestionTriggerSource::kManualFallbackAddress;
  FormData form = is_address_filling
                      ? test::CreateTestAddressFormData()
                      : test::CreateTestCreditCardFormData(
                            /*is_https=*/true, /*use_month_type=*/false);
  SeeForm(form);

  ShowSuggestions(form,
                  /*fallback_trigger_source=*/params.manual_fallback_option);
  // Fill the suggestion.
  FillFirstFormField(form);

  base::HistogramTester histogram_tester;
  // No suggestion was selected.
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectTotalCount(
      "Autofill.Funnel.NotClassifiedAsTargetFilling."
      "FillAfterSuggestion." +
          ExpectedBucketNameForManualFallbackOption(),
      0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManualFallbackEventLoggerTest,
    ::testing::ValuesIn(std::vector<ManualFallbackTestParams>(
        {{.manual_fallback_option =
              AutofillSuggestionTriggerSource::kManualFallbackAddress,
          .test_name = "_AddressManualFallback"},
         {.manual_fallback_option =
              AutofillSuggestionTriggerSource::kManualFallbackPayments,
          .test_name = "_PaymentsManualFallback"}})),
    [](const ::testing::TestParamInfo<ManualFallbackEventLoggerTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace

}  // namespace autofill::autofill_metrics
