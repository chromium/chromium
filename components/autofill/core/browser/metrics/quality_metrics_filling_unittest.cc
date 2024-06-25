// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics_filling.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

constexpr char kUmaAutomationRate[] = "Autofill.AutomationRate.Address";

std::unique_ptr<FormStructure> GetFormStructure(
    const test::FormDescription& form_description) {
  auto form_structure =
      std::make_unique<FormStructure>(test::GetFormData(form_description));
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  return form_structure;
}

class QualityMetricsFillingTest : public testing::Test {
 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::HistogramTester histogram_tester_;
};

// Tests that no metrics are reported for
// Autofill.AutomationRate.Address if a form is submitted with empty
// fields because it's not possible to calculate a filling rate if the
// denominator is 0.
TEST_F(QualityMetricsFillingTest, AutomationRateNotEmittedForEmptyForm) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST},
                 {.role = NAME_LAST},
                 {.role = ADDRESS_HOME_LINE1}}};

  LogFillingQualityMetrics(*GetFormStructure(form_description));

  EXPECT_TRUE(histogram_tester_.GetAllSamples(kUmaAutomationRate).empty());
}

// Tests that Autofill.AutomationRate.Address is reported as 0 if all
// input was generated via manual typing.
TEST_F(QualityMetricsFillingTest, AutomationRate0EmittedForManuallyFilledForm) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST, .value = u"Jane"},
                 {.role = NAME_LAST, .value = u"Doe"},
                 {.role = ADDRESS_HOME_LINE1}}};

  LogFillingQualityMetrics(*GetFormStructure(form_description));

  histogram_tester_.ExpectUniqueSample(kUmaAutomationRate, 0, 1);
}

// Tests that Autofill.AutomationRate.Address is reported as 100% if all
// input was generated via autofilling typing.
TEST_F(QualityMetricsFillingTest, AutomationRate100EmittedForAutofilledForm) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FIRST,
                  .value = u"Jane",
                  .is_autofilled = true},
                 {.role = NAME_LAST,
                  .heuristic_type = NAME_LAST,
                  .value = u"Doe",
                  .is_autofilled = true},
                 {.role = ADDRESS_HOME_LINE1}}};

  LogFillingQualityMetrics(*GetFormStructure(form_description));

  histogram_tester_.ExpectUniqueSample(kUmaAutomationRate, 100, 1);
}

// Tests that Autofill.AutomationRate.Address is reported as 57% if
// 4 out of 7 submitted characters are are autofilled.
TEST_F(QualityMetricsFillingTest,
       AutomationRateEmittedWithCorrectCalculationForPartiallyAutofilledForm) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FIRST,
                  .value = u"Jane",
                  .is_autofilled = true},
                 {.role = NAME_LAST, .value = u"Doe"},
                 {.role = ADDRESS_HOME_LINE1}}};

  LogFillingQualityMetrics(*GetFormStructure(form_description));

  histogram_tester_.ExpectUniqueSample(kUmaAutomationRate, 57, 1);
}

// Tests that fields with a lot of input are ignored in the calculation of
// Autofill.AutomationRate.Address. This is to prevent outliers in
// metrics where a user types a long essay in a single field.
TEST_F(QualityMetricsFillingTest, AutomationRateEmittedIgnoringLongValues) {
  test::FormDescription form_description = {
      .fields = {
          {.role = NAME_FIRST,
           .heuristic_type = NAME_FIRST,
           .value = u"Jane",
           .is_autofilled = true},
          {.role = NAME_LAST,
           .heuristic_type = NAME_LAST,
           .value = u"Very very very very very very very very very very very "
                    u"very very very very very very very very very very very "
                    u"very very very very very very very long text"},
          {.role = ADDRESS_HOME_LINE1}}};

  LogFillingQualityMetrics(*GetFormStructure(form_description));

  histogram_tester_.ExpectUniqueSample(kUmaAutomationRate, 100, 1);
}

// Tests that fields that were already filled at page load time and not modified
// by the user are ignored in the calculation of
// Autofill.AutomationRate.Address.
TEST_F(QualityMetricsFillingTest,
       AutomationRateEmittedIgnoringUnchangedPreFilledValues) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST, .value = u"Jane"},
                 {.role = NAME_LAST, .value = u"Doe", .is_autofilled = true},
                 {.role = ADDRESS_HOME_LINE1}}};
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure(form_description);
  ASSERT_EQ(form_structure->fields().size(), form_description.fields.size());
  form_structure->field(0)->set_initial_value_changed(false);
  LogFillingQualityMetrics(*form_structure);

  histogram_tester_.ExpectUniqueSample(kUmaAutomationRate, 100, 1);
}

// Tests that no Autofill.DataUtilization* metric is emitted for a field with
// possible types {UNKNOWN_TYPE}.
TEST_F(QualityMetricsFillingTest, DataUtilizationNotEmittedForUnknownType) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{}}});
  form_structure->field(0)->set_possible_types({UNKNOWN_TYPE});

  LogFillingQualityMetrics(*form_structure);

  // Autofill.DataUtilization.AllFieldTypes.Aggregate is always recorded if any
  // data utilization metric is recorded so it suffices to check that it's not
  // emitted here.
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.Aggregate")
          .empty());
}

// Tests that no Autofill.DataUtilization* metric is emitted for a field with
// possible types {EMPTY_TYPE}.
TEST_F(QualityMetricsFillingTest, DataUtilizationNotEmittedForEmptyType) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{}}});
  form_structure->field(0)->set_possible_types({EMPTY_TYPE});

  LogFillingQualityMetrics(*form_structure);

  // Autofill.DataUtilization.AllFieldTypes.Aggregate is always recorded if any
  // data utilization metric is recorded so it suffices to check that it's not
  // emitted here.
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.Aggregate")
          .empty());
}

// Tests that no Autofill.DataUtilization* metric is emitted for a field with
// an unchanged initial value.
TEST_F(QualityMetricsFillingTest,
       DataUtilizationNotEmittedForUnchangedPreFilledFields) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{}}});
  form_structure->field(0)->set_possible_types({NAME_FIRST});
  form_structure->field(0)->set_initial_value_changed(false);

  LogFillingQualityMetrics(*form_structure);

  // Autofill.DataUtilization.AllFieldTypes.Aggregate is always recorded if any
  // data utilization metric is recorded so it suffices to check that it's not
  // emitted here.
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.Aggregate")
          .empty());
}

// Tests that all "Autofill.DataUtilization*" variants, except for
// "*HadPrediction", are recorded for a fillable, non-numeric field type with
// autocomplete="garbage".
TEST_F(QualityMetricsFillingTest,
       DataUtilizationEmittedWithVariantsGarbageAndNoPrediction) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{.autocomplete_attribute = "garbage"}}});
  form_structure->field(0)->set_possible_types({NAME_FIRST});
  form_structure->field(0)->set_initial_value_changed(true);

  LogFillingQualityMetrics(*form_structure);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Aggregate",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.Aggregate",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Garbage",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.Garbage",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.NoPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.NoPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.ByPossibleType", (NAME_FIRST << 6) | 0, 1);

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.HadPrediction")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.SelectedFieldTypes.HadPrediction")
          .empty());

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.AllFieldTypes.GarbageHadPrediction")
          .empty());
  EXPECT_TRUE(histogram_tester_
                  .GetAllSamples("Autofill.DataUtilization.SelectedFieldTypes."
                                 "GarbageHadPrediction")
                  .empty());
}

// Tests that metrics "Autofill.DataUtilization.*.{Aggregate, HadPrediction}"
// are recorded for a field with a `NAME_FIRST` field type prediction.
TEST_F(QualityMetricsFillingTest,
       DataUtilizationEmittedWithVariantHasPrediction) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{.is_autofilled = true}}});
  form_structure->field(0)->set_possible_types({NAME_FIRST});
  form_structure->field(0)->SetTypeTo(AutofillType(NAME_FIRST));

  LogFillingQualityMetrics(*form_structure);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Aggregate",
      AutofillDataUtilization::kAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.Aggregate",
      AutofillDataUtilization::kAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.HadPrediction",
      AutofillDataUtilization::kAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.HadPrediction",
      AutofillDataUtilization::kAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.ByPossibleType", (NAME_FIRST << 6) | 1, 1);

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.Garbage")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.SelectedFieldTypes.Garbage")
          .empty());

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.NoPrediction")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.SelectedFieldTypes.NoPrediction")
          .empty());

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.AllFieldTypes.GarbageHadPrediction")
          .empty());
  EXPECT_TRUE(histogram_tester_
                  .GetAllSamples("Autofill.DataUtilization.SelectedFieldTypes."
                                 "GarbageHadPrediction")
                  .empty());
}

// Tests that no "SelectedFieldTypes" variants are recorded for a field with
// possible types {CREDIT_CARD_EXP_MONTH}.
TEST_F(QualityMetricsFillingTest,
       DataUtilizationEmittedForAllFieldTypesVariantOnlyWhenTypeIsNumeric) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{}}});
  form_structure->field(0)->set_possible_types({CREDIT_CARD_EXP_MONTH});

  LogFillingQualityMetrics(*form_structure);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Aggregate",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.NoPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.ByPossibleType",
      (CREDIT_CARD_EXP_MONTH << 6) | 0, 1);

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.Garbage")
          .empty());

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.HadPrediction")
          .empty());

  // No "SelectedFieldTypes" variant emitted.
  EXPECT_TRUE(histogram_tester_
                  .GetAllSamples(
                      "Autofill.DataUtilization.SelectedFieldTypes.Aggregate")
                  .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.SelectedFieldTypes.NoPrediction")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.SelectedFieldTypes.HadPrediction")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.SelectedFieldTypes.Garbage")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.AllFieldTypes.GarbageHadPrediction")
          .empty());
  EXPECT_TRUE(histogram_tester_
                  .GetAllSamples("Autofill.DataUtilization.SelectedFieldTypes."
                                 "GarbageHadPrediction")
                  .empty());
}

// Tests that "GarbageHadPrediction" variants are recorded for a NAME_FIRST
// field with autocomplete=garbage.
TEST_F(QualityMetricsFillingTest,
       DataUtilizationEmittedWithVariantGarbageHadPrediction) {
  std::unique_ptr<FormStructure> form_structure =
      GetFormStructure({.fields = {{.autocomplete_attribute = "garbage"}}});
  form_structure->field(0)->set_possible_types({NAME_FIRST});
  form_structure->field(0)->SetTypeTo(AutofillType(NAME_FIRST));

  LogFillingQualityMetrics(*form_structure);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Aggregate",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.Aggregate",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.HadPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.HadPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.Garbage",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.Garbage",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.AllFieldTypes.GarbageHadPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.SelectedFieldTypes.GarbageHadPrediction",
      AutofillDataUtilization::kNotAutofilled, 1);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.DataUtilization.ByPossibleType", (NAME_FIRST << 6) | 0, 1);

  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples("Autofill.DataUtilization.AllFieldTypes.NoPrediction")
          .empty());
  EXPECT_TRUE(
      histogram_tester_
          .GetAllSamples(
              "Autofill.DataUtilization.SelectedFieldTypes.NoPrediction")
          .empty());
}

}  // namespace

}  // namespace autofill::autofill_metrics
