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

}  // namespace

}  // namespace autofill::autofill_metrics
