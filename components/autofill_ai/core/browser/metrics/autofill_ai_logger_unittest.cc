// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/metrics/autofill_ai_logger.h"

#include <memory>
#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
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
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager_test_api.h"
#include "components/autofill_ai/core/browser/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill_ai/core/browser/mock_autofill_ai_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill_ai {

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr char submitted_str[] = "Submitted";
constexpr char abandoned_str[] = "Abandoned";
constexpr char eligibility[] = "Autofill.Ai.Funnel.%s.Eligibility";
constexpr char readiness_after_eligibility[] =
    "Autofill.Ai.Funnel.%s.ReadinessAfterEligibility";
constexpr char fill_after_suggestion[] =
    "Autofill.Ai.Funnel.%s.FillAfterSuggestion";
constexpr char correction_after_fill[] =
    "Autofill.Ai.Funnel.%s.CorrectionAfterFill";

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
    autofill_client().set_entity_data_manager(
        std::make_unique<autofill::EntityDataManager>(
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));

    manager_ = std::make_unique<AutofillAiManager>(&client_, &strike_database_);
    ON_CALL(client_, GetAutofillClient)
        .WillByDefault(ReturnRef(autofill_client()));
    ON_CALL(client_, GetEntityDataManager)
        .WillByDefault(Return(autofill_client().GetEntityDataManager()));
  }

  AutofillAiManager& manager() { return *manager_; }

  void AddOrUpdateEntityInstance(autofill::EntityInstance entity) {
    client().GetEntityDataManager()->AddOrUpdateEntityInstance(
        std::move(entity));
    webdata_helper_.WaitUntilIdle();
  }

  // A form is made eligible by adding an AutofillAi type prediction.
  std::unique_ptr<autofill::FormStructure> CreateEligibleForm() {
    autofill::FormData form_data;
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://myform_root.com/form.html")));
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

  autofill::TestAutofillClient& autofill_client() { return autofill_client_; }
  MockAutofillAiClient& client() { return client_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::TestAutofillClient autofill_client_;
  NiceMock<MockAutofillAiClient> client_;
  std::unique_ptr<AutofillAiManager> manager_;
  autofill::TestStrikeDatabase strike_database_;
  autofill::AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<autofill::EntityTable>()};
};

// Test that the funnel metrics are logged correctly given different scenarios.
// This test is parameterized by a boolean representing whether the form was
// submitted or abandoned, and an integer representing the last stage of the
// funnel that was reached:
//
// 0) A form was loaded
// 1) The form was detected eligible for AutofillAi.
// 2) The user had data stored to fill the loaded form.
// 3) The user saw filling suggestions.
// 4) The user accepted a filling suggestion.
// 5) The user corrected the filled suggestion.
class AutofillAiFunnelMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<std::tuple<bool, int>> {
 public:
  AutofillAiFunnelMetricsTest() = default;

  bool submitted() { return std::get<0>(GetParam()); }
  bool is_form_eligible() { return std::get<1>(GetParam()) > 0; }
  bool user_has_data() { return std::get<1>(GetParam()) > 1; }
  bool user_saw_suggestions() { return std::get<1>(GetParam()) > 2; }
  bool user_filled_suggestion() { return std::get<1>(GetParam()) > 3; }
  bool user_corrected_filling() { return std::get<1>(GetParam()) > 4; }

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
};

INSTANTIATE_TEST_SUITE_P(AutofillAiTest,
                         AutofillAiFunnelMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(0, 1, 2, 3, 4, 5)));

// Tests that appropriate calls in `AutofillAiLogger`
// result in correct metric logging.
TEST_P(AutofillAiFunnelMetricsTest, Logger) {
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  test_api(manager()).logger().OnFormEligibilityAvailable(form->global_id(),
                                                          is_form_eligible());

  if (user_has_data()) {
    test_api(manager()).logger().OnFormHasDataToFill(form->global_id());
  }
  if (user_saw_suggestions()) {
    test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                    /*ukm_source_id=*/{});
  }
  if (user_filled_suggestion()) {
    test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(0),
                                                     /*ukm_source_id=*/{});
  }
  if (user_corrected_filling()) {
    test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(0),
                                                         /*ukm_source_id=*/{});
  }

  base::HistogramTester histogram_tester;
  test_api(manager()).logger().RecordFormMetrics(
      *form, /*ukm_source_id=*/{}, submitted(), /*opt_in_status=*/true);
  ExpectCorrectFunnelRecording(histogram_tester);
}

// Tests that appropriate calls in `AutofillAiManager`
// result in correct metric logging.
TEST_P(AutofillAiFunnelMetricsTest, Manager) {
  // This will dictate whether the form will be eligible for filling or not.
  std::unique_ptr<autofill::FormStructure> form =
      is_form_eligible() ? CreateEligibleForm() : CreateIneligibleForm();
  // This will dictate whether we consider the form ready to be filled or not.
  autofill::EntityInstance passport =
      autofill::test::GetPassportEntityInstance();
  if (user_has_data()) {
    AddOrUpdateEntityInstance(passport);
  }
  manager().OnFormSeen(*form);

  if (user_saw_suggestions()) {
    manager().OnSuggestionsShown(*form, *form->field(0), /*ukm_source_id=*/{});
  }
  if (user_filled_suggestion()) {
    manager().OnDidFillSuggestion(passport.guid(), *form, *form->field(0),
                                  {form->field(0)},
                                  /*ukm_source_id=*/{});
  }
  if (user_corrected_filling()) {
    manager().OnEditedAutofilledField(*form, *form->field(0),
                                      /*ukm_source_id=*/{});
  }

  base::HistogramTester histogram_tester;
  test_api(manager()).logger().RecordFormMetrics(
      *form, /*ukm_source_id=*/{}, submitted(), /*opt_in_status=*/true);
  ExpectCorrectFunnelRecording(histogram_tester);
}

class AutofillAiMqlsMetricsTest : public BaseAutofillAiTest {
 public:
  AutofillAiMqlsMetricsTest() {
    autofill_client().SetUpPrefsAndIdentityForAutofillAi();
    logs_uploader_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(&local_state_);

    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        local_state_.registry());
    ON_CALL(client(), GetMqlsUploadService)
        .WillByDefault(testing::Return(logs_uploader_.get()));
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  mqls_logs() {
    return logs_uploader_->uploaded_logs();
  }

  const optimization_guide::proto::AutofillAiFieldEvent&
  GetLastFieldEventLogs() {
    return *(logs_uploader_->uploaded_logs()
                 .back()
                 ->mutable_forms_classifications()
                 ->mutable_quality()
                 ->mutable_field_event());
  }

  const optimization_guide::proto::AutofillAiKeyMetrics& GetKeyMetricsLogs() {
    return *(logs_uploader_->uploaded_logs()
                 .back()
                 ->mutable_forms_classifications()
                 ->mutable_quality()
                 ->mutable_key_metrics());
  }

  void ExpectCorrectMqlsFieldEventLogging(
      const optimization_guide::proto::AutofillAiFieldEvent& mqls_field_event,
      const autofill::FormStructure& form,
      const autofill::AutofillField& field,
      AutofillAiUkmLogger::EventType event_type,
      int event_order) {
    std::string event = [&] {
      switch (event_type) {
        case AutofillAiUkmLogger::EventType::kSuggestionShown:
          return "EventType: SuggestionShown";
        case AutofillAiUkmLogger::EventType::kSuggestionFilled:
          return "EventType: SuggestionFilled";
        case AutofillAiUkmLogger::EventType::kEditedAutofilledValue:
          return "EventType: EditedAutofilledValue";
        case AutofillAiUkmLogger::EventType::kFieldFilled:
          return "EventType: FieldFilled";
      }
    }();

    EXPECT_EQ(mqls_field_event.domain(), "myform_root.com") << event;
    EXPECT_EQ(mqls_field_event.form_signature(), form.form_signature().value())
        << event;
    EXPECT_EQ(
        mqls_field_event.form_session_identifier(),
        autofill::autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()))
        << event;
    EXPECT_EQ(mqls_field_event.form_session_event_order(), event_order)
        << event;
    EXPECT_EQ(mqls_field_event.field_signature(),
              field.GetFieldSignature().value())
        << event;
    EXPECT_EQ(
        mqls_field_event.field_session_identifier(),
        autofill::autofill_metrics::FieldGlobalIdToHash64Bit(field.global_id()))
        << event;
    EXPECT_EQ(mqls_field_event.field_rank(), field.rank()) << event;
    EXPECT_EQ(mqls_field_event.field_rank_in_signature_group(),
              field.rank_in_signature_group())
        << event;
    EXPECT_EQ(mqls_field_event.field_type(),
              static_cast<int>(field.Type().GetStorableType()))
        << event;
    EXPECT_EQ(
        mqls_field_event.ai_field_type(),
        static_cast<int>(field.GetAutofillAiServerTypePredictions().value_or(
            autofill::UNKNOWN_TYPE)))
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.format_string_source()),
              base::to_underlying(field.format_string_source()))
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.form_control_type()),
              base::to_underlying(field.form_control_type()) + 1)
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.event_type()),
              base::to_underlying(event_type))
        << event;
  }

  void ExpectCorrectMqlsKeyMetricsLogging(
      const optimization_guide::proto::AutofillAiKeyMetrics& mqls_key_metrics,
      const autofill::FormStructure& form,
      bool filling_readiness,
      bool filling_assistance,
      bool filling_acceptance,
      bool filling_correctness) {
    EXPECT_EQ(mqls_key_metrics.domain(), "myform_root.com");
    EXPECT_EQ(mqls_key_metrics.form_signature(), form.form_signature().value());
    EXPECT_EQ(
        mqls_key_metrics.form_session_identifier(),
        autofill::autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()));
    EXPECT_EQ(mqls_key_metrics.filling_readiness(), filling_readiness);
    EXPECT_EQ(mqls_key_metrics.filling_assistance(), filling_assistance);
    EXPECT_EQ(mqls_key_metrics.filling_acceptance(), filling_acceptance);
    EXPECT_EQ(mqls_key_metrics.filling_correctness(), filling_correctness);
  }

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
};

TEST_F(AutofillAiMqlsMetricsTest, FieldEvent) {
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 1u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionShown, /*event_order=*/0);

  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(0),
                                                   /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 2u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionFilled, /*event_order=*/1);

  test_api(manager()).logger().OnDidFillField(*form, *form->field(0),
                                              /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 3u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kFieldFilled, /*event_order=*/2);

  test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(0),
                                                       /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 4u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue,
      /*event_order=*/3);
}

TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics) {
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  test_api(manager()).logger().OnFormHasDataToFill(form->global_id());
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(0),
                                                   /*ukm_source_id=*/{});
  test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(0),
                                                       /*ukm_source_id=*/{});

  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/true);
  ASSERT_EQ(mqls_logs().size(), 4u);
  ExpectCorrectMqlsKeyMetricsLogging(
      GetKeyMetricsLogs(), *form, /*filling_readiness=*/true,
      /*filling_assistance=*/true,
      /*filling_acceptance=*/true, /*filling_correctness=*/false);
}

// Tests that KeyMetrics MQLS metrics aren't recorded if the user is not opted
// in for Autofill AI.
TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics_OptOut) {
  autofill::SetAutofillAiOptInStatus(autofill_client(), false);
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/false);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that KeyMetrics MQLS metrics aren't recorded if the form was abandoned
// and not submitted.
TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics_FormAbandoned) {
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();

  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/false,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that metrics are not recorded in MQLS if the enterprise policy forbids
// it.
TEST_F(AutofillAiMqlsMetricsTest, NoMqlsMetricsIfDisabledByEnterprisePolicy) {
  autofill_client().GetPrefs()->SetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed,
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable));

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that metrics are not recorded in MQLS when off-the-record.
TEST_F(AutofillAiMqlsMetricsTest, NoMqlsMetricsWhenOffTheRecord) {
  autofill_client().set_is_off_the_record(true);

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

}  // namespace

}  // namespace autofill_ai
