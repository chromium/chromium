// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

FillFieldLogEvent GetFillFieldLogEventWithFillingMethod(
    FillingMethod filling_method) {
  return FillFieldLogEvent{
      .fill_event_id = GetNextFillEventId(),
      .had_value_before_filling = ToOptionalBoolean(false),
      .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
      .was_autofilled_before_security_policy = ToOptionalBoolean(true),
      .had_value_after_filling = ToOptionalBoolean(true),
      .filling_method = filling_method,
      .filling_prevented_by_iframe_security_policy = OptionalBoolean::kFalse};
}

std::vector<test::FieldDescription> GetTestFormDataFields() {
  return {
      {.role = NAME_FULL, .value = u"First Middle Last", .is_autofilled = true},
      // Those two fields are going to be changed to a value of the
      // same type.
      {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
      {.role = NAME_LAST, .value = u"Last", .is_autofilled = true},
      // This field is going to be changed to a value of a different
      // type.
      {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
      // This field is going to be changed to another value of unknown
      // type.
      {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
      // This field is going to be changed to the empty value.
      {.role = NAME_MIDDLE, .value = u"Middle", .is_autofilled = true},
      // This field remains.
      {.role = NAME_LAST, .value = u"Last", .is_autofilled = true},
      // This following two fields are manually filled to a value of
      // type NAME_FIRST.
      {.role = NAME_FIRST, .value = u"Elvis", .is_autofilled = false},
      {.role = NAME_FIRST, .value = u"Elvis", .is_autofilled = false},
      // This one is manually filled to a value of type NAME_LAST.
      {.role = NAME_FIRST, .value = u"Presley", .is_autofilled = false},
      // This next three are manually filled to a value of
      // UNKNOWN_TYPE.
      {.role = NAME_FIRST, .value = u"Random Value", .is_autofilled = false},
      {.role = NAME_MIDDLE, .value = u"Random Value", .is_autofilled = false},
      {.role = NAME_LAST, .value = u"Random Value", .is_autofilled = false},
      {.role = ADDRESS_HOME_LINE1,
       .value = u"Erika-mann",
       .is_autofilled = true},
      {.role = ADDRESS_HOME_ZIP, .value = u"89173", .is_autofilled = true},
      {.role = ADDRESS_HOME_APT_NUM, .value = u"33", .is_autofilled = true},
      // The last field is not autofilled and empty.
      {.role = ADDRESS_HOME_CITY, .value = u"", .is_autofilled = false},
      // We add two credit cards field to make sure those are counted
      // in separate statistics.
      {.role = CREDIT_CARD_NAME_FULL,
       .value = u"Test Name",
       .is_autofilled = true},
      {.role = CREDIT_CARD_NUMBER, .value = u"", .is_autofilled = false}};
}

// Expects that `histogram_tester` contains `sample`. The histogram's name is
// the concatenation of  "Autofill.FieldFillingStats." and
// `histogram_name_suffix`. The histogram bucket count defaults to 1.
void ExpectFieldFillingStatsUniqueSample(
    const base::HistogramTester& histogram_tester,
    std::string_view histogram_name_suffix,
    int sample) {
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.FieldFillingStats.", histogram_name_suffix}),
      sample, /*expected_bucket_count=*/1);
}

class AutofillFieldFillingStatsAndScoreMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }

  // Simulates user changes to the fields [1, 5] of `form_data_`. Used
  // to cover user correction metrics.
  void SimulationOfDefaultUserChangesOnAddedFormTextFields() {
    ASSERT_GT(form_data_.fields().size(), 6u);
    // Elvis is of type NAME_FIRST in the test profile.
    SimulateUserChangedTextFieldTo(form_data_, form_data_.fields()[1],
                                   u"Elvis");
    // Presley is of type NAME_LAST in the test profile
    SimulateUserChangedTextFieldTo(form_data_, form_data_.fields()[2],
                                   u"Presley");
    // Presley is of type NAME_LAST in the test profile
    SimulateUserChangedTextFieldTo(form_data_, form_data_.fields()[3],
                                   u"Presley");
    // This is a random string of UNKNOWN_TYPE.
    SimulateUserChangedTextFieldTo(form_data_, form_data_.fields()[4],
                                   u"something random");
    SimulateUserChangedTextFieldTo(form_data_, form_data_.fields()[5], u"");
  }

  // Creates, adds and "sees" a form that contains `fields`.
  const FormData& GetAndAddSeenFormWithFields(
      const std::vector<test::FieldDescription>& fields) {
    form_data_ =
        GetAndAddSeenForm({.description_for_logging = "FieldFillingStats",
                           .fields = fields,
                           .renderer_id = test::MakeFormRendererId(),
                           .main_frame_origin = url::Origin::Create(
                               autofill_client_->form_origin())});
    return form_data_;
  }

 private:
  // `FormData` initialized on `GetAndAddSeenFormWithFields()`. Used to simulate
  // a form submission.
  FormData form_data_;
};

// Test the logging of the field-wise filling stats.
// TODO(crbug.com/40274514): Delete this test once cleanup starts.
TEST_F(AutofillFieldFillingStatsAndScoreMetricsTest, FillingStats) {
  const FormData& form = GetAndAddSeenFormWithFields(GetTestFormDataFields());
  SimulationOfDefaultUserChangesOnAddedFormTextFields();
  base::HistogramTester histogram_tester;

  SubmitForm(form);

  // Testing of the FormFillingStats expectations. Note that `form` is an
  // address form but also a `FormTypeNameForLogging::kPostalAddressForm` form,
  // as opposed to for example, a `FormTypeNameForLogging::kEmailOnlyForm`.
  for (std::string_view form_type :
       {FormTypeNameForLoggingToStringView(
            FormTypeNameForLogging::kAddressForm),
        FormTypeNameForLoggingToStringView(
            FormTypeNameForLogging::kPostalAddressForm)}) {
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".Accepted"}), 5);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".CorrectedToSameType"}), 2);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester,
        base::StrCat({form_type, ".CorrectedToDifferentType"}), 1);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".CorrectedToUnknownType"}),
        1);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".CorrectedToEmpty"}), 1);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".LeftEmpty"}), 1);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester,
        base::StrCat({form_type, ".ManuallyFilledToSameType"}), 2);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester,
        base::StrCat({form_type, ".ManuallyFilledToDifferentType"}), 1);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester,
        base::StrCat({form_type, ".ManuallyFilledToUnknownType"}), 3);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".TotalManuallyFilled"}), 6);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".TotalFilled"}), 10);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".TotalCorrected"}), 5);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".TotalUnfilled"}), 7);
    ExpectFieldFillingStatsUniqueSample(
        histogram_tester, base::StrCat({form_type, ".Total"}), 17);
  }
}

// Same as above but for different filling methods. Note that metrics related to
// a filled not being autofilled like `ManuallyFilledToSameType` are always
// counted in the Any bucket of filling methods, since the other ones by
// definition means that the filled was autofilled.
TEST_F(AutofillFieldFillingStatsAndScoreMetricsTest,
       FillingStats_FillingMethod) {
  base::test::ScopedFeatureList features(
      features::kAutofillGranularFillingAvailable);

  const FormData& form = GetAndAddSeenFormWithFields(GetTestFormDataFields());
  FormStructure* form_structure =
      autofill_manager().FindCachedFormById(form.global_id());

  form_structure->field(0)->AppendLogEventIfNotRepeated(
      GetFillFieldLogEventWithFillingMethod(FillingMethod::kGroupFillingName));
  form_structure->field(1)->AppendLogEventIfNotRepeated(
      GetFillFieldLogEventWithFillingMethod(
          FillingMethod::kGroupFillingAddress));
  form_structure->field(2)->AppendLogEventIfNotRepeated(
      GetFillFieldLogEventWithFillingMethod(
          FillingMethod::kFieldByFieldFilling));

  // Make all other filled fields, be `FillingMethod::kFullForm`.
  for (size_t i = 3; i < form_structure->fields().size(); i++) {
    AutofillField* field = form_structure->field(i);
    if (field->is_autofilled()) {
      field->AppendLogEventIfNotRepeated(
          GetFillFieldLogEventWithFillingMethod(FillingMethod::kFullForm));
    }
  }
  base::HistogramTester histogram_tester;
  SimulationOfDefaultUserChangesOnAddedFormTextFields();
  SubmitForm(form);

  // The first field which was simply accepted had
  // FillingMethod::kGroupFillingAddress as filling method.
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "GroupFilling.Address.Accepted", 1);
  // The second field was corrected.
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "GroupFilling.Address.CorrectedToSameType", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "GroupFilling.Address.TotalFilled", 2);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "GroupFilling.Address.Total", 2);

  // The third field which was changed to the same type had
  // FillingMethod::kGroupFillingAddress as filling method.
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FieldByFieldFilling.Address."
                                      "CorrectedToSameType",
                                      1);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "FieldByFieldFilling.Address.TotalFilled", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FieldByFieldFilling.Address.Total", 1);

  // The other filled fields had FillingMethod::kFullForm as filling
  // method
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "FullForm.Address.CorrectedToDifferentType", 1);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "FullForm.Address.CorrectedToUnknownType", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FullForm.Address.CorrectedToEmpty", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FullForm.Address.TotalFilled", 7);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FullForm.Address.TotalCorrected", 3);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "FullForm.Address.Total", 7);

  // Manually filled fields are only counted under Any since they have no
  // filling method.
  ExpectFieldFillingStatsUniqueSample(histogram_tester, "Any.Address.Accepted",
                                      5);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.CorrectedToSameType", 2);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "Any.Address.CorrectedToDifferentType", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.CorrectedToUnknownType", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.CorrectedToEmpty", 1);
  ExpectFieldFillingStatsUniqueSample(histogram_tester, "Any.Address.LeftEmpty",
                                      1);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "Any.Address.ManuallyFilledToSameType", 2);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "Any.Address.ManuallyFilledToDifferentType", 1);
  ExpectFieldFillingStatsUniqueSample(
      histogram_tester, "Any.Address.ManuallyFilledToUnknownType", 3);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.TotalManuallyFilled", 6);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.TotalFilled", 10);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.TotalCorrected", 5);
  ExpectFieldFillingStatsUniqueSample(histogram_tester,
                                      "Any.Address.TotalUnfilled", 7);
  ExpectFieldFillingStatsUniqueSample(histogram_tester, "Any.Address.Total",
                                      17);
}

// Test form-wise filling score for the different form types.
TEST_F(AutofillFieldFillingStatsAndScoreMetricsTest, FillingScores) {
  const FormData& form = GetAndAddSeenFormWithFields(GetTestFormDataFields());
  base::HistogramTester histogram_tester;
  SimulationOfDefaultUserChangesOnAddedFormTextFields();

  SubmitForm(form);

  // Testing of the FormFillingScore expectations.

  // The form contains a total of 7 autofilled address fields. Two fields are
  // accepted while 5 are corrected.
  const int accepted_address_fields = 5;
  const int corrected_address_fields = 5;

  const int expected_address_score =
      2 * accepted_address_fields - 3 * corrected_address_fields + 100;
  const int expected_address_complex_score =
      accepted_address_fields * 10 + corrected_address_fields;

  histogram_tester.ExpectUniqueSample("Autofill.FormFillingScore.Address",
                                      expected_address_score, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormFillingComplexScore.Address",
      expected_address_complex_score, 1);

  // Also test for credit cards where there is exactly one accepted field and
  // no corrected fields.
  histogram_tester.ExpectUniqueSample("Autofill.FormFillingScore.CreditCard",
                                      102, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormFillingComplexScore.CreditCard", 10, 1);
}

class AutocompleteUnrecognizedFieldFillingStatsTest
    : public AutofillFieldFillingStatsAndScoreMetricsTest {};

TEST_F(AutocompleteUnrecognizedFieldFillingStatsTest, FieldFillingStats) {
  FormData form = GetAndAddSeenForm(
      {.fields = {
           {.role = NAME_FIRST,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = NAME_MIDDLE,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = NAME_LAST,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = ADDRESS_HOME_COUNTRY,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = ADDRESS_HOME_STREET_NAME,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = ADDRESS_HOME_HOUSE_NUMBER,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = ADDRESS_HOME_CITY,
            .autocomplete_attribute = "unrecognized",
            .is_autofilled = true},
           {.role = ADDRESS_HOME_ZIP, .autocomplete_attribute = "unrecognized"},
           {.role = PHONE_HOME_WHOLE_NUMBER,
            .autocomplete_attribute = "unrecognized"},
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "unrecognized"}}});

  SimulateUserChangedTextFieldTo(form, form.fields()[0],
                                 u"Corrected First Name");
  SimulateUserChangedTextFieldTo(form, form.fields()[1],
                                 u"Corrected Middle Name");
  SimulateUserChangedTextFieldTo(form, form.fields()[2],
                                 u"Corrected Last Name");
  SimulateUserChangedTextFieldTo(form, form.fields()[8],
                                 u"Manually Filled Phone");
  SimulateUserChangedTextFieldTo(form, form.fields()[9],
                                 u"Manually Filled Email");

  base::HistogramTester histogram_tester;
  SubmitForm(form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.AutocompleteUnrecognized.FieldFillingStats2"),
      base::BucketsAre(
          base::Bucket(FieldFillingStatus::kAccepted, 4),
          base::Bucket(FieldFillingStatus::kCorrectedToUnknownType, 3),
          base::Bucket(FieldFillingStatus::kManuallyFilledToUnknownType, 2),
          base::Bucket(FieldFillingStatus::kLeftEmpty, 1)));
}

}  // namespace
}  // namespace autofill::autofill_metrics
