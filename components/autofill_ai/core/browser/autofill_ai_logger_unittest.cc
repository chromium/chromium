// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_logger.h"

#include <memory>
#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager_test_api.h"
#include "components/autofill_ai/core/browser/mock_autofill_ai_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill_ai {

namespace {

constexpr char submitted_str[] = "Submitted";
constexpr char abandoned_str[] = "Abandoned";
constexpr char eligibility[] = "Autofill.FormsAI.Funnel.%s.Eligibility";
constexpr char readiness_after_eligibility[] =
    "Autofill.FormsAI.Funnel.%s.ReadinessAfterEligibility";
constexpr char fill_after_suggestion[] =
    "Autofill.FormsAI.Funnel.%s.FillAfterSuggestion";
constexpr char correction_after_fill[] =
    "Autofill.FormsAI.Funnel.%s.CorrectionAfterFill";

std::string GetEligibilityHistogram() {
  return base::StringPrintf(eligibility, "Aggregate");
}
std::string GetEligibilityHistogram(bool submitted) {
  return base::StringPrintf(eligibility,
                            submitted ? submitted_str : abandoned_str);
}

std::string GetReadinessAfterEligibilityHistogram() {
  return base::StringPrintf(readiness_after_eligibility, "Aggregate");
}
std::string GetReadinessAfterEligibilityHistogram(bool submitted) {
  return base::StringPrintf(readiness_after_eligibility,
                            submitted ? submitted_str : abandoned_str);
}

std::string GetFillAfterSuggestionHistogram() {
  return base::StringPrintf(fill_after_suggestion, "Aggregate");
}
std::string GetFillAfterSuggestionHistogram(bool submitted) {
  return base::StringPrintf(fill_after_suggestion,
                            submitted ? submitted_str : abandoned_str);
}

std::string GetCorrectionAfterFillHistogram() {
  return base::StringPrintf(correction_after_fill, "Aggregate");
}
std::string GetCorrectionAfterFillHistogram(bool submitted) {
  return base::StringPrintf(correction_after_fill,
                            submitted ? submitted_str : abandoned_str);
}

class BaseAutofillAiTest : public testing::Test {
 public:
  BaseAutofillAiTest() {
    manager_ = std::make_unique<AutofillAiManager>(&client_, &strike_database_);
    ON_CALL(client_, GetAutofillClient)
        .WillByDefault(testing::ReturnRef(autofill_client_));
    ON_CALL(client_, GetEntityDataManager)
        .WillByDefault(testing::Return(&entity_data_manager_));
  }

  AutofillAiManager& manager() { return *manager_; }

  void AddOrUpdateEntityInstance(autofill::EntityInstance entity) {
    entity_data_manager_.AddOrUpdateEntityInstance(std::move(entity));
    webdata_helper_.WaitUntilIdle();
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::TestAutofillClient autofill_client_;
  testing::NiceMock<MockAutofillAiClient> client_;
  std::unique_ptr<AutofillAiManager> manager_;
  autofill::TestStrikeDatabase strike_database_;
  autofill::AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<autofill::EntityTable>()};
  autofill::EntityDataManager entity_data_manager_{
      webdata_helper_.autofill_webdata_service(), /*history_service=*/nullptr,
      /*strike_database=*/nullptr};
};

// Test that the funnel metrics are logged correctly given different scenarios.
// This test is parameterized by a boolean representing whether the form was
// submitted or abandoned, and an integer representing the last stage of the
// funnel that was reached:
//
// 0) A form was loaded
// 1) The form was detected eligible for AutofillAi.
// 2) The user had data stored to fill the loaded form.
// 3) The user saw prediction improvements entry-point suggestions.
// 4) The user started loading filling suggestions.
// 5) The user saw filling suggestions.
// 6) The user accepted a filling suggestion.
// 7) The user corrected the filled suggestion.
class AutofillAiFunnelMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<std::tuple<bool, int>> {
 public:
  AutofillAiFunnelMetricsTest() = default;

  bool submitted() { return std::get<0>(GetParam()); }
  bool is_form_eligible() { return std::get<1>(GetParam()) > 0; }
  bool user_has_data() { return std::get<1>(GetParam()) > 1; }
  bool user_saw_suggestions() { return std::get<1>(GetParam()) > 2; }
  bool user_triggered_manual_fallbacks() { return std::get<1>(GetParam()) > 3; }
  bool user_saw_filling_suggestions() { return std::get<1>(GetParam()) > 4; }
  bool user_filled_suggestion() { return std::get<1>(GetParam()) > 5; }
  bool user_corrected_filling() { return std::get<1>(GetParam()) > 6; }

  void ExpectCorrectFunnelRecording(
      const base::HistogramTester& histogram_tester) {
    // Expect that we do not record any sample for the submission-specific
    // histograms that are not applicable.
    histogram_tester.ExpectTotalCount(GetEligibilityHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetReadinessAfterEligibilityHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetFillAfterSuggestionHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetCorrectionAfterFillHistogram(!submitted()), 0);

    // Expect that the aggregate and appropriate submission-specific histograms
    // record the correct values.
    histogram_tester.ExpectUniqueSample(GetEligibilityHistogram(),
                                        is_form_eligible(), 1);
    histogram_tester.ExpectUniqueSample(GetEligibilityHistogram(submitted()),
                                        is_form_eligible(), 1);

    if (is_form_eligible()) {
      histogram_tester.ExpectUniqueSample(
          GetReadinessAfterEligibilityHistogram(), user_has_data(), 1);
      histogram_tester.ExpectUniqueSample(
          GetReadinessAfterEligibilityHistogram(submitted()), user_has_data(),
          1);
    } else {
      histogram_tester.ExpectTotalCount(GetReadinessAfterEligibilityHistogram(),
                                        0);
      histogram_tester.ExpectTotalCount(
          GetReadinessAfterEligibilityHistogram(submitted()), 0);
    }

    if (user_has_data()) {
      histogram_tester.ExpectUniqueSample(GetFillAfterSuggestionHistogram(),
                                          user_filled_suggestion(), 1);
      histogram_tester.ExpectUniqueSample(
          GetFillAfterSuggestionHistogram(submitted()),
          user_filled_suggestion(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetFillAfterSuggestionHistogram(), 0);
      histogram_tester.ExpectTotalCount(
          GetFillAfterSuggestionHistogram(submitted()), 0);
    }

    if (user_filled_suggestion()) {
      histogram_tester.ExpectUniqueSample(GetCorrectionAfterFillHistogram(),
                                          user_corrected_filling(), 1);
      histogram_tester.ExpectUniqueSample(
          GetCorrectionAfterFillHistogram(submitted()),
          user_corrected_filling(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetCorrectionAfterFillHistogram(), 0);
      histogram_tester.ExpectTotalCount(
          GetCorrectionAfterFillHistogram(submitted()), 0);
    }
  }

  // A form is made eligible by adding an AutofillAi type prediction.
  std::unique_ptr<autofill::FormStructure> CreateEligibleForm() {
    autofill::FormData form_data;
    auto form = std::make_unique<autofill::FormStructure>(form_data);
    autofill::AutofillField& autofill_ai_field = test_api(*form).PushField();
    autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
        FieldPrediction prediction;
    prediction.set_type(autofill::PASSPORT_NAME_TAG);
    autofill_ai_field.set_server_predictions({prediction});

    return form;
  }

  std::unique_ptr<autofill::FormStructure> CreateIneligibleForm() {
    autofill::FormData form_data;
    auto form = std::make_unique<autofill::FormStructure>(form_data);
    autofill::AutofillField& prediction_improvement_field =
        test_api(*form).PushField();
    prediction_improvement_field.SetTypeTo(
        autofill::AutofillType(autofill::CREDIT_CARD_NUMBER),
        autofill::AutofillPredictionSource::kHeuristics);
    return form;
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillAiTest,
    AutofillAiFunnelMetricsTest,
    testing::Combine(testing::Bool(), testing::Values(0, 1, 2, 3, 4, 5, 6, 7)));

// Tests that appropriate calls in `AutofillAiLogger`
// result in correct metric logging.
TEST_P(AutofillAiFunnelMetricsTest, Logger) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);

  test_api(manager()).logger().OnFormEligibilityAvailable(form.global_id(),
                                                          is_form_eligible());

  if (user_has_data()) {
    test_api(manager()).logger().OnFormHasDataToFill(form.global_id());
  }
  if (user_triggered_manual_fallbacks()) {
    test_api(manager()).logger().OnTriggeredFillingSuggestions(
        form.global_id());
  }
  if (user_saw_filling_suggestions()) {
    test_api(manager()).logger().OnFillingSuggestionsShown(form.global_id());
  }
  if (user_filled_suggestion()) {
    test_api(manager()).logger().OnDidFillSuggestion(form.global_id());
  }
  if (user_corrected_filling()) {
    test_api(manager()).logger().OnDidCorrectFillingSuggestion(
        form.global_id());
  }

  base::HistogramTester histogram_tester;
  test_api(manager()).logger().RecordMetricsForForm(form.global_id(),
                                                    submitted());
  ExpectCorrectFunnelRecording(histogram_tester);
}

// Tests that appropriate calls in `AutofillAiManager`
// result in correct metric logging.
TEST_P(AutofillAiFunnelMetricsTest, Manager) {
  // This will dictate whether the form will be eligible for filling or not.
  std::unique_ptr<autofill::FormStructure> form =
      is_form_eligible() ? CreateEligibleForm() : CreateIneligibleForm();
  // This will dictate whether we consider the form ready to be filled or not.
  if (user_has_data()) {
    AddOrUpdateEntityInstance(autofill::test::GetPassportEntityInstance());
  }
  manager().OnFormSeen(*form);

  if (user_saw_filling_suggestions()) {
    manager().OnSuggestionsShown({autofill::SuggestionType::kFillAutofillAi},
                                 form->global_id());
  }
  if (user_filled_suggestion()) {
    manager().OnDidFillSuggestion(form->global_id());
  }
  if (user_corrected_filling()) {
    manager().OnEditedAutofilledField(form->global_id());
  }

  base::HistogramTester histogram_tester;
  test_api(manager()).logger().RecordMetricsForForm(form->global_id(),
                                                    submitted());
  ExpectCorrectFunnelRecording(histogram_tester);
}

}  // namespace

}  // namespace autofill_ai
