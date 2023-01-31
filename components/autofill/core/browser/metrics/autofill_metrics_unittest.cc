// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "base/check.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webdata/common/web_data_results.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_canon.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

using ::autofill::test::AddFieldPredictionToForm;
using ::base::ASCIIToUTF16;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::BucketsInclude;
using ::base::TimeTicks;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::UnorderedPointwise;

namespace autofill {

// This is defined in the autofill_metrics.cc implementation file.
int GetFieldTypeGroupPredictionQualityMetric(
    ServerFieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric);

}  // namespace autofill

namespace autofill::metrics {

using mojom::SubmissionSource;
using SyncSigninState = AutofillSyncSigninState;

namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldDidChangeType = ukm::builders::Autofill_TextFieldDidChange;
using UkmLogHiddenRepresentationalFieldSkipDecisionType =
    ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision;
using UkmLogRepeatedServerTypePredictionRationalized =
    ukm::builders::Autofill_RepeatedServerTypePredictionRationalized;
using UkmFormSubmittedType = ukm::builders::Autofill_FormSubmitted;
using UkmFieldTypeValidationType = ukm::builders::Autofill_FieldTypeValidation;
using UkmFieldFillStatusType = ukm::builders::Autofill_FieldFillStatus;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmEditedAutofilledFieldAtSubmission =
    ukm::builders::Autofill_EditedAutofilledFieldAtSubmission;
using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using UkmFieldInfoType = ukm::builders::Autofill2_FieldInfo;

struct ExpectedUkmMetricsPair : public std::pair<const char*, int64_t> {
  using std::pair<const char*, int64_t>::pair;
  ExpectedUkmMetricsPair(const char* str, HtmlFieldMode mode)
      : ExpectedUkmMetricsPair(str, static_cast<int64_t>(mode)) {}
  ExpectedUkmMetricsPair(const char* str, HtmlFieldType type)
      : ExpectedUkmMetricsPair(str, static_cast<int64_t>(type)) {}
};

using ExpectedUkmMetricsRecord = std::vector<ExpectedUkmMetricsPair>;
using ExpectedUkmMetrics = std::vector<ExpectedUkmMetricsRecord>;

using AddressImportRequirements =
    AutofillMetrics::AddressProfileImportRequirementMetric;

FormSignature Collapse(FormSignature sig) {
  return FormSignature(sig.value() % 1021);
}

FieldSignature Collapse(FieldSignature sig) {
  return FieldSignature(sig.value() % 1021);
}

struct AddressProfileImportRequirementExpectations {
  AddressImportRequirements requirement;
  bool fulfilled;
};

void VerifyDeveloperEngagementUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    const bool is_for_credit_card,
    const DenseSet<FormType>& form_types,
    const std::vector<int64_t>& expected_metric_values) {
  int expected_metric_value = 0;
  for (const auto it : expected_metric_values)
    expected_metric_value |= 1 << it;

  auto entries =
      ukm_recorder->GetEntriesByName(UkmDeveloperEngagementType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
    ukm_recorder->ExpectEntrySourceHasUrl(
        entry, GURL(form.main_frame_origin.GetURL()));
    EXPECT_EQ(4u, entry->metrics.size());
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
        expected_metric_value);
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kIsForCreditCardName,
        is_for_credit_card);
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormTypesName,
        AutofillMetrics::FormTypesToBitVector(form_types));
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormSignatureName,
        Collapse(CalculateFormSignature(form)).value());
  }
}

MATCHER(CompareMetricsIgnoringMillisecondsSinceFormParsed, "") {
  const auto& lhs = ::testing::get<0>(arg);
  const std::pair<const char*, int64_t>& rhs = ::testing::get<1>(arg);
  return lhs.first == base::HashMetricName(rhs.first) &&
         (lhs.second == rhs.second ||
          (lhs.second > 0 &&
           rhs.first ==
               UkmSuggestionFilledType::kMillisecondsSinceFormParsedName));
}

void VerifyUkm(const ukm::TestUkmRecorder* ukm_recorder,
               const FormData& form,
               const char* event_name,
               const ExpectedUkmMetrics& expected_metrics) {
  auto entries = ukm_recorder->GetEntriesByName(event_name);

  EXPECT_LE(entries.size(), expected_metrics.size());
  for (size_t i = 0; i < expected_metrics.size() && i < entries.size(); i++) {
    ukm_recorder->ExpectEntrySourceHasUrl(entries[i],
                                          form.main_frame_origin.GetURL());
    EXPECT_THAT(
        entries[i]->metrics,
        UnorderedPointwise(CompareMetricsIgnoringMillisecondsSinceFormParsed(),
                           expected_metrics[i]));
  }
}

void VerifySubmitFormUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    AutofillMetrics::AutofillFormSubmittedState state,
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    const FormInteractionCounts& form_interaction_counts = {}) {
  VerifyUkm(ukm_recorder, form, UkmFormSubmittedType::kEntryName,
            {{{UkmFormSubmittedType::kAutofillFormSubmittedStateName, state},
              {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
              {UkmFormSubmittedType::kIsForCreditCardName, is_for_credit_card},
              {UkmFormSubmittedType::kHasUpiVpaFieldName, has_upi_vpa_field},
              {UkmFormSubmittedType::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(form_types)},
              {UkmFormSubmittedType::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()},
              {UkmFormSubmittedType::kFormElementUserModificationsName,
               form_interaction_counts.form_element_user_modifications},
              {UkmFormSubmittedType::kAutofillFillsName,
               form_interaction_counts.autofill_fills}}});
}

void AppendFieldFillStatusUkm(const FormData& form,
                              ExpectedUkmMetrics* expected_metrics) {
  FormSignature form_signature = Collapse(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  for (const FormFieldData& field : form.fields) {
    FieldSignature field_signature =
        Collapse(CalculateFieldSignatureForField(field));
    expected_metrics->push_back(
        {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFieldFillStatusType::kFormSignatureName, form_signature.value()},
         {UkmFieldFillStatusType::kFieldSignatureName, field_signature.value()},
         {UkmFieldFillStatusType::kValidationEventName, metric_type},
         {UkmTextFieldDidChangeType::kIsAutofilledName,
          field.is_autofilled ? 1 : 0},
         {UkmFieldFillStatusType::kWasPreviouslyAutofilledName, 0}});
  }
}

void AppendFieldTypeUkm(const FormData& form,
                        const std::vector<ServerFieldType>& heuristic_types,
                        const std::vector<ServerFieldType>& server_types,
                        const std::vector<ServerFieldType>& actual_types,
                        ExpectedUkmMetrics* expected_metrics) {
  ASSERT_EQ(heuristic_types.size(), form.fields.size());
  ASSERT_EQ(server_types.size(), form.fields.size());
  ASSERT_EQ(actual_types.size(), form.fields.size());
  FormSignature form_signature = Collapse(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  std::vector<int64_t> prediction_sources{
      AutofillMetrics::PREDICTION_SOURCE_HEURISTIC,
      AutofillMetrics::PREDICTION_SOURCE_SERVER,
      AutofillMetrics::PREDICTION_SOURCE_OVERALL};
  for (size_t i = 0; i < form.fields.size(); ++i) {
    const FormFieldData& field = form.fields[i];
    FieldSignature field_signature =
        Collapse(CalculateFieldSignatureForField(field));
    for (int64_t source : prediction_sources) {
      int64_t predicted_type = static_cast<int64_t>(
          (source == AutofillMetrics::PREDICTION_SOURCE_SERVER
               ? server_types
               : heuristic_types)[i]);
      int64_t actual_type = static_cast<int64_t>(actual_types[i]);
      expected_metrics->push_back(
          {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
           {UkmFieldFillStatusType::kFormSignatureName, form_signature.value()},
           {UkmFieldFillStatusType::kFieldSignatureName,
            field_signature.value()},
           {UkmFieldFillStatusType::kValidationEventName, metric_type},
           {UkmFieldTypeValidationType::kPredictionSourceName, source},
           {UkmFieldTypeValidationType::kPredictedTypeName, predicted_type},
           {UkmFieldTypeValidationType::kActualTypeName, actual_type}});
    }
  }
}

// For a single submission, test if the right bucket was filled.
void TestAddressProfileImportRequirements(
    base::HistogramTester* histogram_tester,
    const std::vector<AddressProfileImportRequirementExpectations>&
        expectations) {
  std::string histogram = "Autofill.AddressProfileImportRequirements";

  for (auto& expectation : expectations) {
    histogram_tester->ExpectBucketCount(histogram, expectation.requirement,
                                        expectation.fulfilled ? 1 : 0);
  }
}

// For country specific address field requirements.
void TestAddressProfileImportCountrySpecificFieldRequirements(
    base::HistogramTester* histogram_tester,
    AutofillMetrics::AddressProfileImportCountrySpecificFieldRequirementsMetric
        metric) {
  std::string histogram =
      "Autofill.AddressProfileImportCountrySpecificFieldRequirements";

  // Test that the right bucket was populated.
  histogram_tester->ExpectBucketCount(histogram, metric, 1);
}

void CreateSimpleForm(const GURL& origin, FormData& form) {
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(origin);
}

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  std::string response_string;
  base::Base64Encode(unencoded_response_string, &response_string);
  return response_string;
}

}  // namespace

class AutofillMetricsTest : public AutofillMetricsBaseTest,
                            public testing::Test {
 public:
  using AutofillMetricsBaseTest::AutofillMetricsBaseTest;
  ~AutofillMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Test parameter indicates if the metrics are being logged for a form in an
// iframe or the main frame. True means the form is in the main frame.
class AutofillMetricsIFrameTest : public testing::WithParamInterface<bool>,
                                  public AutofillMetricsTest {
 public:
  AutofillMetricsIFrameTest()
      : AutofillMetricsTest(
            /*is_in_any_main_frame=*/GetParam()),
        credit_card_form_events_frame_histogram_(
            std::string("Autofill.FormEvents.CreditCard.") +
            (is_in_any_main_frame_ ? "IsInMainFrame" : "IsInIFrame")) {}

 protected:
  const std::string credit_card_form_events_frame_histogram_;
};

INSTANTIATE_TEST_SUITE_P(AutofillMetricsTest,
                         AutofillMetricsIFrameTest,
                         testing::Bool());

// Test the logging of the form filling stats for the different form types.
TEST_F(AutofillMetricsTest, FieldFillingStats) {
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "FieldFillingStats",
       .fields =
           {
               {.role = NAME_FULL,
                .value = u"First Middle Last",
                .is_autofilled = true},
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
               {.role = NAME_FIRST,
                .value = u"Presley",
                .is_autofilled = false},
               // This next three are manually filled to a value of
               // UNKNOWN_TYPE.
               {.role = NAME_FIRST,
                .value = u"Random Value",
                .is_autofilled = false},
               {.role = NAME_MIDDLE,
                .value = u"Random Value",
                .is_autofilled = false},
               {.role = NAME_LAST,
                .value = u"Random Value",
                .is_autofilled = false},
               // The last field is not autofilled and empty.
               {.role = ADDRESS_HOME_CITY,
                .value = u"",
                .is_autofilled = false},
               // We add two credit cards field to make sure those are counted
               // in separate statistics.
               {.role = CREDIT_CARD_NAME_FULL,
                .value = u"Test Name",
                .is_autofilled = true},
               {.role = CREDIT_CARD_NUMBER,
                .value = u"",
                .is_autofilled = false},
           },
       .unique_renderer_id = test::MakeFormRendererId(),
       .main_frame_origin =
           url::Origin::Create(autofill_client_->form_origin())});

  // Elvis is of type NAME_FIRST in the test profile.
  SimulateUserChangedTextFieldTo(form, form.fields[1], u"Elvis");
  // Presley is of type NAME_LAST in the test profile
  SimulateUserChangedTextFieldTo(form, form.fields[2], u"Presley");
  // Presley is of type NAME_LAST in the test profile
  SimulateUserChangedTextFieldTo(form, form.fields[3], u"Presley");
  // This is a random string of UNKNOWN_TYPE.
  SimulateUserChangedTextFieldTo(form, form.fields[4], u"something random");
  SimulateUserChangedTextFieldTo(form, form.fields[5], u"");

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  const std::string histogram_prefix = "Autofill.FieldFillingStats.Address.";

  histogram_tester.ExpectUniqueSample(histogram_prefix + "Accepted", 2, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "CorrectedToSameType",
                                      2, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "CorrectedToDifferentType", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "CorrectedToUnknownType", 1, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "CorrectedToEmpty", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "LeftEmpty", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToSameType", 2, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToDifferentType", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToUnknownType", 3, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalManuallyFilled",
                                      6, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalFilled", 7, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalCorrected", 5,
                                      1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalUnfilled", 7, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "Total", 14, 1);
}

// Test that we log the right number of autofilled fields at submission time.
TEST_F(AutofillMetricsTest, NumberOfAutofilledFieldsAtSubmission) {
  // Set up our form data with two autofilled fields.
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.role = NAME_FIRST,
                  .value = u"Elvis Aaron Presley",
                  .is_autofilled = true},
                 {.role = EMAIL_ADDRESS,
                  .value = u"buddy@gmail.com",
                  .is_autofilled = true},
                 {.role = NAME_FIRST, .value = u"", .is_autofilled = false},
                 {.role = EMAIL_ADDRESS,
                  .value = u"garbage",
                  .is_autofilled = false},
                 {.role = NO_SERVER_DATA,
                  .value = u"USA",
                  .form_control_type = "select-one",
                  .is_autofilled = false},
                 {.role = PHONE_HOME_CITY_AND_NUMBER,
                  .value = u"2345678901",
                  .form_control_type = "tel",
                  .is_autofilled = true}},
      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);
  SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(form,
                                                              form.fields[1]);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Test that the correct bucket for the number of filled fields received a
  // count while the others remain at zero counts.
  const size_t expected_number_of_accepted_fillings = 2;
  const size_t expected_number_of_corrected_fillings = 1;
  const size_t expected_number_of_total_fillings =
      expected_number_of_accepted_fillings +
      expected_number_of_corrected_fillings;
  for (int i = 0; i < 50; i++) {
    histogram_tester.ExpectBucketCount(
        "Autofill.NumberOfAutofilledFieldsAtSubmission.Total", i,
        i == expected_number_of_total_fillings ? 1 : 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.NumberOfAutofilledFieldsAtSubmission.Accepted", i,
        i == expected_number_of_accepted_fillings ? 1 : 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.NumberOfAutofilledFieldsAtSubmission.Corrected", i,
        i == expected_number_of_corrected_fillings ? 1 : 0);
  }
}

// Test that we log the right number of autofilled fields with an unrecognized
// autocomplete attribute at submission time.
TEST_F(AutofillMetricsTest,
       NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillFillAndImportFromMoreFields);
  // Set up our form data with two autofilled fields.
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.role = NAME_FULL,
                  .value = u"Elvis Aaron Presley",
                  .autocomplete_attribute = "garbage",
                  .is_autofilled = true},
                 {.role = EMAIL_ADDRESS,
                  .value = u"buddy@gmail.com",
                  .autocomplete_attribute = "garbage",
                  .is_autofilled = true},
                 {.role = NAME_FIRST, .value = u"", .is_autofilled = false},
                 {.role = EMAIL_ADDRESS,
                  .value = u"garbage",
                  .is_autofilled = false},
                 {.role = NO_SERVER_DATA,
                  .value = u"USA",
                  .form_control_type = "select-one",
                  .is_autofilled = false},
                 {.role = PHONE_HOME_CITY_AND_NUMBER,
                  .value = u"2345678901",
                  .form_control_type = "tel",
                  .is_autofilled = true}},
      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};
  FormData form = GetAndAddSeenForm(form_description);

  // Simulate user changing the second and forth field of the form.
  // TODO(crbug.com/1368096): Fix the metric to work independent of the final
  // value.
  SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(form,
                                                              form.fields[1]);
  SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(form,
                                                              form.fields[3]);
  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Test that the correct bucket for the number of filled fields with an
  // unrecognized autocomplete attriute received a count while the others remain
  // at zero counts.
  const size_t expected_number_of_accepted_fillings = 1;
  const size_t expected_number_of_corrected_fillings = 1;
  const size_t expected_number_of_total_fillings =
      expected_number_of_accepted_fillings +
      expected_number_of_corrected_fillings;
  for (int i = 0; i < 50; i++) {
    histogram_tester.ExpectBucketCount(
        "Autofill."
        "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission."
        "Total",
        i, i == expected_number_of_total_fillings ? 1 : 0);
    histogram_tester.ExpectBucketCount(
        "Autofill."
        "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission."
        "Accepted",
        i, i == expected_number_of_accepted_fillings ? 1 : 0);
    histogram_tester.ExpectBucketCount(
        "Autofill."
        "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission."
        "Corrected",
        i, i == expected_number_of_corrected_fillings ? 1 : 0);
  }
}

struct Field {
  ServerFieldType field_type;
  bool is_autofilled = true;
  absl::optional<std::u16string> value = absl::nullopt;
};

struct PerfectFillingTestCase {
  std::string description;
  std::vector<Field> fields;
  std::vector<Bucket> address_buckets;
  std::vector<Bucket> credit_card_buckets;
};

class AutofillPerfectFillingMetricsTest
    : public AutofillMetricsTest,
      public ::testing::WithParamInterface<PerfectFillingTestCase> {
 public:
  std::vector<test::FieldDescription> GetFields(std::vector<Field> fields) {
    std::vector<test::FieldDescription> fields_to_return;
    for (const auto& field : fields) {
      test::FieldDescription f;
      if (field.value) {
        f.value = field.value;
      } else if (field.field_type == NAME_FULL ||
                 field.field_type == CREDIT_CARD_NAME_FULL) {
        f.value = u"Elvis Aaron Presley";
      } else if (field.field_type == EMAIL_ADDRESS) {
        f.value = u"buddy@gmail.com";
      } else if (field.field_type == ADDRESS_HOME_CITY) {
        f.value = u"Munich";
      } else if (field.field_type == CREDIT_CARD_NUMBER) {
        f.value = u"01230123012399";
      } else {
        NOTREACHED();
      }
      f.role = field.field_type;
      f.is_autofilled = field.is_autofilled;
      fields_to_return.push_back(f);
    }
    return fields_to_return;
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTest,
    AutofillPerfectFillingMetricsTest,
    testing::Values(
        // Test that we log the perfect filling metric correctly for an address
        // form in which every field is autofilled.
        PerfectFillingTestCase{
            "PerfectFillingForAddresses_AllAutofillFilled",
            {{NAME_FULL}, {EMAIL_ADDRESS}, {ADDRESS_HOME_CITY}},
            {Bucket(false, 0), Bucket(true, 1)},
            {Bucket(false, 0), Bucket(true, 0)}},
        // Test that we log the perfect filling metric correctly for an address
        // form in which every field is autofilled or empty.
        PerfectFillingTestCase{
            "PerfectFillingForAddresses_AllAutofillFilledOrEmpty",
            {{NAME_FULL}, {EMAIL_ADDRESS}, {ADDRESS_HOME_CITY, false, u""}},
            {Bucket(false, 0), Bucket(true, 1)},
            {Bucket(false, 0), Bucket(true, 0)}},
        // Test that we log the perfect filling metric correctly for an address
        // form in which a non-empty field is not autofilled.
        PerfectFillingTestCase{
            "PerfectFillingForAddresses_NotAllAutofilled",
            {{NAME_FULL}, {EMAIL_ADDRESS}, {ADDRESS_HOME_CITY, false}},
            {Bucket(false, 1), Bucket(true, 0)},
            {Bucket(false, 0), Bucket(true, 0)}},
        // Test that we log the perfect filling metric correctly for a credit
        // card form in which every field is autofilled.
        PerfectFillingTestCase{"PerfectFillingForCreditCards_AllAutofilled",
                               {{CREDIT_CARD_NAME_FULL}, {CREDIT_CARD_NUMBER}},
                               {Bucket(false, 0), Bucket(true, 0)},
                               {Bucket(false, 0), Bucket(true, 1)}},
        // Test that we log the perfect filling metric correctly for a credit
        // card form in which not every field is autofilled or empty.
        PerfectFillingTestCase{
            "PerfectFillingForCreditCards_NotAllAutofilled",
            {{CREDIT_CARD_NAME_FULL}, {CREDIT_CARD_NUMBER, false}},
            {Bucket(false, 0), Bucket(true, 0)},
            {Bucket(false, 1), Bucket(true, 0)}},
        // Test that we log the perfect filling metric correctly for a form that
        // contains both credit card and address information. Here, the form is
        // fully autofilled resulting in a perfect count for both addresses and
        // credit cards.
        PerfectFillingTestCase{"PerfectFillingForMixedForm_AllAutofilled",
                               {{NAME_FULL}, {CREDIT_CARD_NUMBER}},
                               {Bucket(false, 0), Bucket(true, 1)},
                               {Bucket(false, 0), Bucket(true, 1)}},
        // Test that we log the perfect filling metric correctly for a form that
        // contains both credit card and address information.  Here, the form is
        // not fully autofilled resulting in a non-perfect count for both
        // addresses and credit cards.
        PerfectFillingTestCase{"PerfectFillingForMixedForm_NotAllAutofilled",
                               {{NAME_FULL}, {CREDIT_CARD_NUMBER, false}},
                               {Bucket(false, 1), Bucket(true, 0)},
                               {Bucket(false, 1), Bucket(true, 0)}}));

TEST_P(AutofillPerfectFillingMetricsTest,
       PerfectFilling_Addresses_CreditCards) {
  auto test_case = GetParam();
  FormData form =
      test::GetFormData({.description_for_logging = test_case.description,
                         .fields = GetFields(test_case.fields),
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> field_types;
  for (const auto& f : test_case.fields)
    field_types.push_back(f.field_type);

  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(test_case.address_buckets));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(test_case.credit_card_buckets));
}

struct TouchToFillForCreditCardsTestCase {
  std::vector<Field> fields;
  std::vector<bool> fields_is_autofilled_values;
  bool is_all_autofilled;
  bool is_all_accepted;
};

class TouchToFillForCreditCardsTest
    : public AutofillMetricsTest,
      public ::testing::WithParamInterface<TouchToFillForCreditCardsTestCase> {
 public:
  std::vector<FormFieldData> GetFields(std::vector<Field> fields) {
    std::vector<FormFieldData> fields_to_return;
    fields_to_return.reserve(fields.size());
    for (const auto& field : fields) {
      if (field.field_type == CREDIT_CARD_NAME_FULL) {
        fields_to_return.push_back(
            CreateField("Name on card", "cardName", "", "text"));
      } else if (field.field_type == CREDIT_CARD_NUMBER) {
        fields_to_return.push_back(
            CreateField("Credit card number", "cardNumber", "", "text"));
      } else if (field.field_type == CREDIT_CARD_EXP_MONTH) {
        fields_to_return.push_back(
            CreateField("Expiration date", "cc_exp", "", "text"));
      } else if (field.field_type == CREDIT_CARD_VERIFICATION_CODE) {
        fields_to_return.push_back(CreateField("CVC", "CVC", "", "text"));
      } else {
        NOTREACHED();
      }
    }
    return fields_to_return;
  }

  void SetFieldsAutofilledValues(FormData& form,
                                 std::vector<bool>& fields_is_autofilled_values,
                                 std::vector<Field>& server_field_types) {
    DCHECK(form.fields.size() == fields_is_autofilled_values.size());
    DCHECK(form.fields.size() == server_field_types.size());
    for (size_t i = 0; i < fields_is_autofilled_values.size(); i++) {
      form.fields[i].is_autofilled = fields_is_autofilled_values[i];
      CreditCard testCard = test::GetCreditCard();
      form.fields[i].value =
          server_field_types[i].field_type != CREDIT_CARD_VERIFICATION_CODE
              ? testCard.GetRawInfo(server_field_types[i].field_type)
              : u"123";
    }
  }
};

TEST_P(TouchToFillForCreditCardsTest,
       AllAutofilledAndAccepted_TouchToFill_CreditCards) {
  RecreateCreditCards(true, false, false, false);
  TouchToFillForCreditCardsTestCase test_case = GetParam();
  FormData form = CreateForm(GetFields(test_case.fields));

  SeeForm(form);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0], {},
                                              AutoselectFirstSuggestion(false),
                                              FormElementWasClicked(true));

  base::HistogramTester histogram_tester;
  // Simulate user selection in the payments bottom sheet
  touch_to_fill_delgate_->SuggestionSelected(kTestLocalCardId);
  // Simulate that fields were autofilled
  SetFieldsAutofilledValues(form, test_case.fields_is_autofilled_values,
                            test_case.fields);
  // Simulate user made change to autofilled field
  if (!test_case.is_all_accepted) {
    SimulateUserChangedTextField(form, form.fields[0]);
  }

  SubmitForm(form);
  ResetDriverToCommitMetrics();
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Autofill.TouchToFill.CreditCard.PerfectFilling",
                test_case.is_all_autofilled && test_case.is_all_accepted),
            1);
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Autofill.FillingCorrectnessByMethod.CreditCard.TouchToFill",
                test_case.is_all_accepted),
            1);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTest,
    TouchToFillForCreditCardsTest,
    testing::Values(
        // All autofilled and nothing edited manually
        TouchToFillForCreditCardsTestCase{
            {{CREDIT_CARD_NAME_FULL},
             {CREDIT_CARD_NUMBER},
             {CREDIT_CARD_EXP_MONTH}},
            /*fields_is_autofilled_values=*/{true, true, true},
            /*is_all_autofilled=*/true,
            /*is_all_accepted=*/true},
        // Not all autofilled and nothing edited manually
        TouchToFillForCreditCardsTestCase{
            {{CREDIT_CARD_NAME_FULL},
             {CREDIT_CARD_NUMBER},
             {CREDIT_CARD_EXP_MONTH},
             {CREDIT_CARD_VERIFICATION_CODE}},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/true},
        // Not all autofilled and something edited manually
        TouchToFillForCreditCardsTestCase{
            {{CREDIT_CARD_NAME_FULL},
             {CREDIT_CARD_NUMBER},
             {CREDIT_CARD_EXP_MONTH},
             {CREDIT_CARD_VERIFICATION_CODE}},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/false}));

// Test the emission of collisions between NUMERIC_QUANTITY and server
// predictions as well as the potential false positives.
TEST_F(AutofillMetricsTest, NumericQuantityCollision) {
  // Those metrics are only collected when the numeric quantities are not
  // getting precedence over server predictions.
  base::test::ScopedFeatureList numeric_quanity_feature_list;
  numeric_quanity_feature_list.InitAndDisableFeature(
      features::kAutofillGivePrecedenceToNumericQuantitites);

  // Set up our form data.
  test::FormDescription form_description = {
      .description_for_logging = "NumericQuantityCollision",
      .fields = {{.server_type = NO_SERVER_DATA,
                  .heuristic_type = NUMERIC_QUANTITY,
                  .is_autofilled = false},
                 // We add a second field to make sure the metrics are only
                 // recorded for the field with the numeric quantity prediction.
                 {.server_type = ADDRESS_HOME_LINE1,
                  .heuristic_type = ADDRESS_HOME_LINE1,
                  .is_autofilled = false}}};

  // Helper to submit the `form` and test the expectations. `collision`
  // indicates that there was a collision between the NUMERIC_QUANTITY
  // prediction and a server prediction.
  // If `autofill_used` and a `collision` exists, the histogram to
  // track `false_positive` is checked.
  auto SubmitAndTest = [this](const FormData& form, bool collision,
                              bool autofill_used, bool false_positive) {
    base::HistogramTester histogram_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.NumericQuantityCollidesWithServerPrediction", collision, 1);
    if (collision && autofill_used) {
      histogram_tester.ExpectUniqueSample(
          "Autofill.AcceptedFilledFieldWithNumericQuantityHeuristicPrediction",
          false_positive, 1);
    }
  };

  {
    SCOPED_TRACE(
        "No collision case - The numeric quanity does not collide with a "
        "server prediction.");
    FormData form = GetAndAddSeenForm(form_description);
    SubmitAndTest(form, /*collision=*/false, /*autofill_used=*/false,
                  /*false_positive=*/false);
  }
  {
    SCOPED_TRACE("Collision, but nothing is filled.");
    // Add a server prediction to create a collision.
    form_description.fields[0].server_type = NAME_FIRST;
    FormData form = GetAndAddSeenForm(form_description);
    SubmitAndTest(form, /*collision=*/true, /*autofill_used=*/false,
                  /*false_positive=*/false);
  }
  {
    SCOPED_TRACE("Collision, the field is autofilled.");
    form_description.fields[0].is_autofilled = true;
    FormData form = GetAndAddSeenForm(form_description);
    SubmitAndTest(form, /*collision=*/true, /*autofill_used=*/true,
                  /*false_positive=*/true);
  }
  {
    SCOPED_TRACE(
        "Collision, the field is autofilled and subsequently changed.");
    FormData form = GetAndAddSeenForm(form_description);
    SimulateUserChangedTextField(form, form.fields[0]);
    SubmitAndTest(form, /*collision=*/true, /*autofill_used=*/true,
                  /*false_positive=*/false);
  }
}

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
  // Set up our form data.
  test::FormDescription form_description = {
      .description_for_logging = "QualityMetrics",
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FULL,
                  .value = u"Elvis Aaron Presley",
                  .is_autofilled = true},
                 {.role = EMAIL_ADDRESS,
                  .heuristic_type = PHONE_HOME_NUMBER,
                  .value = u"buddy@gmail.com",
                  .is_autofilled = false},
                 {.role = NAME_FIRST,
                  .heuristic_type = NAME_FULL,
                  .value = u"",
                  .is_autofilled = false},
                 {.role = EMAIL_ADDRESS,
                  .heuristic_type = PHONE_HOME_NUMBER,
                  .value = u"garbage",
                  .is_autofilled = false},
                 {.role = NO_SERVER_DATA,
                  .heuristic_type = UNKNOWN_TYPE,
                  .value = u"USA",
                  .form_control_type = "select-one",
                  .is_autofilled = false},
                 {.role = PHONE_HOME_CITY_AND_NUMBER,
                  .heuristic_type = PHONE_HOME_CITY_AND_NUMBER,
                  .value = u"2345678901",
                  .form_control_type = "tel",
                  .is_autofilled = true}},
      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,         PHONE_HOME_NUMBER, NAME_FULL,
      PHONE_HOME_NUMBER, UNKNOWN_TYPE,      PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {
      NAME_FIRST,    EMAIL_ADDRESS,  NAME_FIRST,
      EMAIL_ADDRESS, NO_SERVER_DATA, PHONE_HOME_CITY_AND_NUMBER};

  FormData form = GetAndAddSeenForm(form_description);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Auxiliary function for GetAllSamples() expectations.
  auto b = [](ServerFieldType field_type,
              AutofillMetrics::FieldTypeQualityMetric metric,
              base::HistogramBase::Count count) {
    return Bucket(GetFieldTypeGroupPredictionQualityMetric(field_type, metric),
                  count);
  };

  // Heuristic predictions.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldPredictionQuality.Aggregate.Heuristic"),
              BucketsAre(Bucket(AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1),
                         Bucket(AutofillMetrics::TRUE_POSITIVE, 2),
                         Bucket(AutofillMetrics::FALSE_POSITIVE_EMPTY, 1),
                         Bucket(AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1),
                         Bucket(AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldPredictionQuality.ByFieldType.Heuristic"),
      BucketsAre(
          b(ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1),
          b(NAME_FULL, AutofillMetrics::TRUE_POSITIVE, 1),
          b(PHONE_HOME_CITY_AND_NUMBER, AutofillMetrics::TRUE_POSITIVE, 1),
          b(EMAIL_ADDRESS, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
          b(PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_MISMATCH, 1),
          b(PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1),
          b(NAME_FULL, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldPredictionQuality.Aggregate.Server"),
              BucketsAre(Bucket(AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1),
                         Bucket(AutofillMetrics::TRUE_POSITIVE, 2),
                         Bucket(AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
                         Bucket(AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1),
                         Bucket(AutofillMetrics::FALSE_POSITIVE_EMPTY, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldPredictionQuality.ByFieldType.Server"),
      BucketsAre(
          b(ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1),
          b(EMAIL_ADDRESS, AutofillMetrics::TRUE_POSITIVE, 1),
          b(PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::TRUE_POSITIVE, 1),
          b(NAME_FULL, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
          b(NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_MISMATCH, 1),
          b(EMAIL_ADDRESS, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1),
          b(NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1)));

  // Server overrides heuristic so Overall and Server are the same predictions
  // (as there were no test fields where server == NO_SERVER_DATA and heuristic
  // != UNKNOWN_TYPE).
  EXPECT_EQ(histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.Aggregate.Server"),
            histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.Aggregate.Overall"));
  EXPECT_EQ(histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.ByFieldType.Server"),
            histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.ByFieldType.Overall"));
}

// Test that the ProfileImportStatus logs a no import.
TEST_F(AutofillMetricsTest, ProfileImportStatus_NoImport) {
  // Set up our form data. Since a ZIP code is required for US profiles, this
  // import fails.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportStatus_NoImport",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"Invalid State"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  using Metric = AutofillMetrics::AddressProfileImportStatusMetric;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AddressProfileImportStatus"),
      BucketsAre(Bucket(Metric::REGULAR_IMPORT, 0),
                 Bucket(Metric::NO_IMPORT, 1),
                 Bucket(Metric::SECTION_UNION_IMPORT, 0)));
}

// Test that the ProfileImportStatus logs a regular import.
TEST_F(AutofillMetricsTest, ProfileImportStatus_RegularImport) {
  // Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportStatus_RegularImport",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  using Metric = AutofillMetrics::AddressProfileImportStatusMetric;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AddressProfileImportStatus"),
      BucketsAre(Bucket(Metric::REGULAR_IMPORT, 1),
                 Bucket(Metric::NO_IMPORT, 0),
                 Bucket(Metric::SECTION_UNION_IMPORT, 0)));
}

// Test that the ProfileImportStatus logs a section union mport.
TEST_F(AutofillMetricsTest, ProfileImportStatus_UnionImport) {
  // Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportStatus_UnionImport",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_CITY,
            .value = u"New York",
            .autocomplete_attribute = "section-billing locality"},
           // Add the last field of the form into a new section.
           {.role = ADDRESS_HOME_STATE,
            .value = u"CA",
            .autocomplete_attribute = "section-shipping address-level1"}}});

  FillTestProfile(form);

  base::HistogramTester histogram_tester;

  SubmitForm(form);

  using Metric = AutofillMetrics::AddressProfileImportStatusMetric;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AddressProfileImportStatus"),
      BucketsAre(Bucket(Metric::REGULAR_IMPORT, 0),
                 Bucket(Metric::NO_IMPORT, 0),
                 Bucket(Metric::SECTION_UNION_IMPORT, 1)));
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// 'perfect' profile import.
TEST_F(AutofillMetricsTest, ProfileImportRequirements_AllFulfilled) {
  // Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportRequirements_AllFulfilled",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are counted correctly if only the
// ADDRESS_HOME_LINE1 is missing.
TEST_F(AutofillMetricsTest, ProfileImportRequirements_MissingHomeLineOne) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_MissingHomeLineOne",
       .fields = {{.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
                  {.role = ADDRESS_HOME_LINE1, .value = u""},
                  {.role = ADDRESS_HOME_CITY, .value = u"New York"},
                  {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
                  {.role = ADDRESS_HOME_STATE, .value = u"CA"},
                  {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
                  {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // The country specific ADDRESS_HOME_LINE1 field requirement was violated.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::
              LINE1_REQUIREMENT_VIOLATED);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// 'perfect' profile import.
TEST_F(AutofillMetricsTest,
       ProfileImportRequirements_AllFulfilledForNonStateCountry) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_AllFulfilledForNonStateCountry",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u""},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"Germany"}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);
  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// completely filled profile but with invalid values.
TEST_F(AutofillMetricsTest,
       ProfileImportRequirements_FilledButInvalidZipEmailAndState) {
  // Set up our form data.
  test::FormDescription form_description = {
      .description_for_logging =
          "ProfileImportRequirements_FilledButInvalidZipEmailAndState",
      .fields = {
          {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
          {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
          {.role = ADDRESS_HOME_CITY, .value = u"New York"},
          {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
          {.role = ADDRESS_HOME_STATE, .value = u"DefNotAState"},
          {.role = ADDRESS_HOME_ZIP, .value = u"1234567890"},
          {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"},
          {.role = EMAIL_ADDRESS, .value = u"test_noat_test.io"}}};

  FormData form = GetAndAddSeenForm(form_description);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// profile with multiple email addresses.
TEST_F(AutofillMetricsTest, ProfileImportRequirements_NonUniqueEmail) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportRequirements_NonUniqueEmail",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = EMAIL_ADDRESS, .value = u"test_noat_test.io"},
           {.role = EMAIL_ADDRESS, .value = u"not_test@test.io"}}});

  std::vector<ServerFieldType> field_types = {NAME_FULL,
                                              ADDRESS_HOME_LINE1,
                                              ADDRESS_HOME_CITY,
                                              PHONE_HOME_CITY_AND_NUMBER,
                                              ADDRESS_HOME_STATE,
                                              ADDRESS_HOME_ZIP,
                                              ADDRESS_HOME_COUNTRY,
                                              EMAIL_ADDRESS,
                                              EMAIL_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test the correct ProfileImportRequirements logging if multiple fields are
// missing.
TEST_F(AutofillMetricsTest, ProfileImportRequirements_OnlyAddressLineOne) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_OnlyAddressLineOne",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u""},
           {.role = PHONE_HOME_NUMBER, .value = u""},
           {.role = ADDRESS_HOME_STATE, .value = u""},
           {.role = ADDRESS_HOME_ZIP, .value = u""},
           {.role = ADDRESS_HOME_COUNTRY, .value = u""}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED,
       true},
      {AddressImportRequirements::NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED,
       false},
      {AddressImportRequirements::CITY_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::CITY_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::ZIP_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::ZIP_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::STATE_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::STATE_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::OVERALL_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::OVERALL_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::LINE1_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric::
              ZIP_STATE_CITY_REQUIREMENT_VIOLATED);
}

// Test that we log quality metrics appropriately with fields having
// only_fill_when_focused and are supposed to log RATIONALIZATION_OK.
TEST_F(AutofillMetricsTest,
       QualityMetrics_LoggedCorrecltyForRationalizationOk) {
  FormData form = CreateForm(
      {CreateField("Name", "name", "Elvis Aaron Presley", "text"),
       CreateField("Address", "address", "3734 Elvis Presley Blvd.", "text"),
       CreateField("Phone", "phone", "2345678901", "text"),
       // RATIONALIZATION_OK because it's ambiguous value.
       CreateField("Phone1", "phone1", "nonsense value", "text"),
       // RATIONALIZATION_OK because it's same type but different to what is in
       // the profile.
       CreateField("Phone2", "phone2", "2345678902", "text"),
       // RATIONALIZATION_OK because it's a type mismatch.
       CreateField("Phone3", "phone3", "Elvis Aaron Presley", "text")});
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  ADDRESS_HOME_LINE1,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {NAME_FULL,
                                               ADDRESS_HOME_LINE1,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               PHONE_HOME_WHOLE_NUMBER,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               PHONE_HOME_WHOLE_NUMBER};

  base::UserActionTester user_action_tester;
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  // Trigger phone number rationalization at filling time.
  FillTestProfile(form);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Rationalization quality.
  {
    std::string rationalization_histogram =
        "Autofill.RationalizationQuality.PhoneNumber";
    // RATIONALIZATION_OK is logged 3 times.
    histogram_tester.ExpectBucketCount(rationalization_histogram,
                                       AutofillMetrics::RATIONALIZATION_OK, 3);
  }
}

// Test that we log quality metrics appropriately with fields having
// only_fill_when_focused and are supposed to log RATIONALIZATION_GOOD.
TEST_F(AutofillMetricsTest,
       QualityMetrics_LoggedCorrecltyForRationalizationGood) {
  FormData form = CreateForm(
      {CreateField("Name", "name", "Elvis Aaron Presley", "text"),
       CreateField("Address", "address", "3734 Elvis Presley Blvd.", "text"),
       CreateField("Phone", "phone", "2345678901", "text"),
       // RATIONALIZATION_GOOD because it's empty.
       CreateField("Phone1", "phone1", "", "text")});
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> field_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                              PHONE_HOME_CITY_AND_NUMBER,
                                              PHONE_HOME_CITY_AND_NUMBER};

  base::UserActionTester user_action_tester;
  autofill_manager().AddSeenForm(form, field_types);
  // Trigger phone number rationalization at filling time.
  FillTestProfile(form);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Rationalization quality.
  {
    std::string rationalization_histogram =
        "Autofill.RationalizationQuality.PhoneNumber";
    // RATIONALIZATION_GOOD is logged once.
    histogram_tester.ExpectBucketCount(
        rationalization_histogram, AutofillMetrics::RATIONALIZATION_GOOD, 1);
  }
}

// Test that we log the skip decisions for hidden/representational fields
// correctly.
TEST_F(AutofillMetricsTest, LogHiddenRepresentationalFieldSkipDecision) {
  // The old sectioning algorithm emits several different UKM metrics. Since we
  // will have various variants of the sectioning algorithm, we don't want to
  // adjust the expectations for each variant for now.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillUseParameterizedSectioning);
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm({
      CreateField("Name", "name", "", "text"),             // no decision
      CreateField("Street", "street", "", "text"),         // skips
      CreateField("City", "city", "", "text"),             // skips
      CreateField("State", "state", "", "select-one"),     // doesn't skip
      CreateField("Country", "country", "", "select-one")  // doesn't skip
  });

  form.fields[1].is_focusable = false;
  form.fields[2].role = FormFieldData::RoleAttribute::kPresentation;
  form.fields[3].is_focusable = false;
  form.fields[4].role = FormFieldData::RoleAttribute::kPresentation;

  std::vector<ServerFieldType> field_types = {
      NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
      ADDRESS_HOME_COUNTRY};

  std::vector<FieldSignature> field_signature;
  for (auto it = form.fields.begin() + 1; it != form.fields.end(); ++it)
    field_signature.push_back(Collapse(CalculateFieldSignatureForField(*it)));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate filling form.
  {
    base::UserActionTester user_action_tester;
    FillTestProfile(form);
  }

  VerifyUkm(
      test_ukm_recorder_, form,
      UkmLogHiddenRepresentationalFieldSkipDecisionType::kEntryName,
      {{{UkmLogHiddenRepresentationalFieldSkipDecisionType::kFormSignatureName,
         form_signature.value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldSignatureName,
         field_signature[0].value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::
             kFieldOverallTypeName,
         ADDRESS_HOME_LINE1},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHeuristicTypeName,
         ADDRESS_HOME_LINE1},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kServerTypeName,
         ADDRESS_HOME_LINE1},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kIsSkippedName,
         true}},
       {{UkmLogHiddenRepresentationalFieldSkipDecisionType::kFormSignatureName,
         form_signature.value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldSignatureName,
         field_signature[1].value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::
             kFieldOverallTypeName,
         ADDRESS_HOME_CITY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHeuristicTypeName,
         ADDRESS_HOME_CITY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kServerTypeName,
         ADDRESS_HOME_CITY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kIsSkippedName,
         true}},
       {{UkmLogHiddenRepresentationalFieldSkipDecisionType::kFormSignatureName,
         form_signature.value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldSignatureName,
         field_signature[2].value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::
             kFieldOverallTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHeuristicTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kServerTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kIsSkippedName,
         false}},
       {{UkmLogHiddenRepresentationalFieldSkipDecisionType::kFormSignatureName,
         form_signature.value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldSignatureName,
         field_signature[3].value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::
             kFieldOverallTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHeuristicTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kServerTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kIsSkippedName,
         false}}});
}

// Test that we log the address line fields whose server types are rationalized
TEST_F(AutofillMetricsTest, LogRepeatedAddressTypeRationalized) {
  FormData form = CreateEmptyForm();

  FieldSignature field_signature[2];

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"fullname";
  field.name = u"fullname";
  form.fields.push_back(field);

  field.label = u"Street 1";
  field.name = u"street1";
  form.fields.push_back(field);
  field_signature[0] = Collapse(CalculateFieldSignatureForField(field));

  field.label = u"Street 2";
  field.name = u"street2";
  form.fields.push_back(field);
  field_signature[1] = Collapse(CalculateFieldSignatureForField(field));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  FormStructure form_structure(form);

  std::vector<ServerFieldType> field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  autofill_manager().AddSeenForm(form, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(form.fields[1], ADDRESS_HOME_STREET_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields[2], ADDRESS_HOME_STREET_ADDRESS,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(
      response_string, {&form_structure},
      test::GetEncodedSignatures({&form_structure}),
      autofill_manager().form_interactions_ukm_logger(), nullptr);

  ASSERT_EQ(test_ukm_recorder_
                ->GetEntriesByName(
                    UkmLogRepeatedServerTypePredictionRationalized::kEntryName)
                .size(),
            (size_t)2);

  VerifyUkm(
      test_ukm_recorder_, form,
      UkmLogRepeatedServerTypePredictionRationalized::kEntryName,
      {{{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         form_signature.value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[0].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldOldOverallTypeName,
         ADDRESS_HOME_STREET_ADDRESS},
        {UkmLogRepeatedServerTypePredictionRationalized::kHeuristicTypeName,
         UNKNOWN_TYPE},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_LINE1},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_STREET_ADDRESS}},
       {{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         form_signature.value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[1].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldOldOverallTypeName,
         ADDRESS_HOME_STREET_ADDRESS},
        {UkmLogRepeatedServerTypePredictionRationalized::kHeuristicTypeName,
         UNKNOWN_TYPE},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_LINE2},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_STREET_ADDRESS}}});
}

// Test that we log the state/country fields whose server types are rationalized
TEST_F(AutofillMetricsTest, LogRepeatedStateCountryTypeRationalized) {
  FormData form = CreateEmptyForm();

  FieldSignature field_signature[3];

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"Country";
  field.name = u"country";
  form.fields.push_back(field);
  field_signature[0] = Collapse(CalculateFieldSignatureForField(field));

  field.label = u"fullname";
  field.name = u"fullname";
  form.fields.push_back(field);

  field.label = u"State";
  field.name = u"state";
  form.fields.push_back(field);
  field_signature[2] = Collapse(CalculateFieldSignatureForField(field));

  field.label = u"State";
  field.name = u"state";
  field.is_focusable = false;
  field.form_control_type = "select-one";
  form.fields.push_back(field);
  // Regardless of the order of appearance, hidden fields are rationalized
  // before their corresponding visible one.
  field_signature[1] = Collapse(CalculateFieldSignatureForField(field));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  FormStructure form_structure(form);

  std::vector<ServerFieldType> field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  autofill_manager().AddSeenForm(form, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], ADDRESS_HOME_COUNTRY,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields[1], NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(form.fields[2], ADDRESS_HOME_COUNTRY,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields[3], ADDRESS_HOME_COUNTRY,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(
      response_string, {&form_structure},
      test::GetEncodedSignatures({&form_structure}),
      autofill_manager().form_interactions_ukm_logger(), nullptr);

  ASSERT_EQ(test_ukm_recorder_
                ->GetEntriesByName(
                    UkmLogRepeatedServerTypePredictionRationalized::kEntryName)
                .size(),
            (size_t)3);

  VerifyUkm(
      test_ukm_recorder_, form,
      UkmLogRepeatedServerTypePredictionRationalized::kEntryName,
      {{{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         *form_signature},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[0].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldOldOverallTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogRepeatedServerTypePredictionRationalized::kHeuristicTypeName,
         UNKNOWN_TYPE},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_COUNTRY}},
       {{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         *form_signature},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[1].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldOldOverallTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogRepeatedServerTypePredictionRationalized::kHeuristicTypeName,
         UNKNOWN_TYPE},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_COUNTRY}},
       {{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         *form_signature},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[2].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddressHome)},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldOldOverallTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogRepeatedServerTypePredictionRationalized::kHeuristicTypeName,
         UNKNOWN_TYPE},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HtmlFieldMode::kNone},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_COUNTRY}}});
}

// Test that we log quality metrics appropriately with fields having
// only_fill_when_focused and are supposed to log RATIONALIZATION_BAD.
TEST_F(AutofillMetricsTest,
       QualityMetrics_LoggedCorrecltyForRationalizationBad) {
  FormData form = CreateForm({
      CreateField("Name", "name", "Elvis Aaron Presley", "text"),
      CreateField("Address", "address", "3734 Elvis Presley Blvd.", "text"),
      CreateField("Phone", "phone", "2345678901", "text"),
      // RATIONALIZATION_BAD because it's filled with the same value as filled
      // previously.
      CreateField("Phone1", "phone1", "2345678901", "text"),
  });
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               PHONE_HOME_WHOLE_NUMBER};

  base::UserActionTester user_action_tester;
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  // Trigger phone number rationalization at filling time.
  FillTestProfile(form);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Rationalization quality.
  {
    std::string rationalization_histogram =
        "Autofill.RationalizationQuality.PhoneNumber";
    // RATIONALIZATION_BAD is logged once.
    histogram_tester.ExpectBucketCount(rationalization_histogram,
                                       AutofillMetrics::RATIONALIZATION_BAD, 1);
  }
}

// Test that we log quality metrics appropriately with fields having
// only_fill_when_focused set to true.
TEST_F(AutofillMetricsTest,
       QualityMetrics_LoggedCorrecltyForOnlyFillWhenFocusedField) {
  FormData form = CreateForm(
      {// TRUE_POSITIVE + no rationalization logging
       CreateField("Name", "name", "Elvis Aaron Presley", "text"),
       // TRUE_POSITIVE + no rationalization logging
       CreateField("Address", "address", "3734 Elvis Presley Blvd.", "text"),
       // TRUE_POSITIVE + no rationalization logging
       CreateField("Phone", "phone", "2345678901", "text"),
       // TRUE_NEGATIVE_EMPTY + RATIONALIZATION_GOOD
       CreateField("Phone1", "phone1", "", "text"),
       // TRUE_POSITIVE + RATIONALIZATION_BAD
       CreateField("Phone2", "phone2", "2345678901", "text"),
       // FALSE_NEGATIVE_MISMATCH + RATIONALIZATION_OK
       CreateField("Phone3", "phone3", "Elvis Aaron Presley", "text")});
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  ADDRESS_HOME_LINE1,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {NAME_FULL,
                                               ADDRESS_HOME_LINE1,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               PHONE_HOME_WHOLE_NUMBER,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               PHONE_HOME_WHOLE_NUMBER};

  base::UserActionTester user_action_tester;
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  // Trigger phone number rationalization at filling time.
  FillTestProfile(form);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Auxiliary function for GetAllSamples() expectations.
  auto b = [](ServerFieldType field_type,
              AutofillMetrics::FieldTypeQualityMetric metric,
              base::HistogramBase::Count count) {
    return Bucket(GetFieldTypeGroupPredictionQualityMetric(field_type, metric),
                  count);
  };

  // Rationalization quality.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.RationalizationQuality.PhoneNumber"),
              BucketsAre(Bucket(AutofillMetrics::RATIONALIZATION_GOOD, 1),
                         Bucket(AutofillMetrics::RATIONALIZATION_OK, 1),
                         Bucket(AutofillMetrics::RATIONALIZATION_BAD, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldPredictionQuality.Aggregate.Heuristic"),
              BucketsAre(Bucket(AutofillMetrics::TRUE_POSITIVE, 4),
                         Bucket(AutofillMetrics::TRUE_NEGATIVE_EMPTY, 1),
                         Bucket(AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldPredictionQuality.ByFieldType.Heuristic"),
      BucketsAre(
          b(NAME_FULL, AutofillMetrics::TRUE_POSITIVE, 1),
          b(ADDRESS_HOME_LINE1, AutofillMetrics::TRUE_POSITIVE, 1),
          b(PHONE_HOME_CITY_AND_NUMBER, AutofillMetrics::TRUE_POSITIVE, 2),
          b(PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
            1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldPredictionQuality.Aggregate.Server"),
              BucketsAre(Bucket(AutofillMetrics::TRUE_POSITIVE, 4),
                         Bucket(AutofillMetrics::TRUE_NEGATIVE_EMPTY, 1),
                         Bucket(AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldPredictionQuality.ByFieldType.Server"),
      BucketsAre(
          b(NAME_FULL, AutofillMetrics::TRUE_POSITIVE, 1),
          b(ADDRESS_HOME_LINE1, AutofillMetrics::TRUE_POSITIVE, 1),
          b(PHONE_HOME_CITY_AND_NUMBER, AutofillMetrics::TRUE_POSITIVE, 2),
          b(PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
            1)));

  // Server overrides heuristic so Overall and Server are the same predictions
  // (as there were no test fields where server == NO_SERVER_DATA and heuristic
  // != UNKNOWN_TYPE).
  EXPECT_EQ(histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.Aggregate.Server"),
            histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.Aggregate.Overall"));
  EXPECT_EQ(histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.FieldType.Server"),
            histogram_tester.GetAllSamples(
                "Autofill.FieldPredictionQuality.FieldType.Overall"));
}

// Tests the true negatives (empty + no prediction and unknown + no prediction)
// and false positives (empty + bad prediction and unknown + bad prediction)
// are counted correctly.

struct QualityMetricsTestCase {
  const ServerFieldType predicted_field_type;
  const ServerFieldType actual_field_type;
};

class QualityMetricsTest
    : public AutofillMetricsTest,
      public testing::WithParamInterface<QualityMetricsTestCase> {
 public:
  const char* ValueForType(ServerFieldType type) {
    switch (type) {
      case EMPTY_TYPE:
        return "";
      case NO_SERVER_DATA:
      case UNKNOWN_TYPE:
        return "unknown";
      case COMPANY_NAME:
        return "RCA";
      case NAME_FIRST:
        return "Elvis";
      case NAME_MIDDLE:
        return "Aaron";
      case NAME_LAST:
        return "Presley";
      case NAME_FULL:
        return "Elvis Aaron Presley";
      case EMAIL_ADDRESS:
        return "buddy@gmail.com";
      case PHONE_HOME_NUMBER:
      case PHONE_HOME_WHOLE_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER:
        return "2345678901";
      case ADDRESS_HOME_STREET_ADDRESS:
        return "123 Apple St.\nunit 6";
      case ADDRESS_HOME_LINE1:
        return "123 Apple St.";
      case ADDRESS_HOME_LINE2:
        return "unit 6";
      case ADDRESS_HOME_CITY:
        return "Lubbock";
      case ADDRESS_HOME_STATE:
        return "Texas";
      case ADDRESS_HOME_ZIP:
        return "79401";
      case ADDRESS_HOME_COUNTRY:
        return "US";
      case AMBIGUOUS_TYPE:
        // This occurs as both a company name and a middle name once ambiguous
        // profiles are created.
        CreateAmbiguousProfiles();
        return "Decca";

      default:
        NOTREACHED();  // Fall through
        return "unexpected!";
    }
  }

  bool IsExampleOf(AutofillMetrics::FieldTypeQualityMetric metric,
                   ServerFieldType predicted_type,
                   ServerFieldType actual_type) {
    // The server can send either NO_SERVER_DATA or UNKNOWN_TYPE to indicate
    // that a field is not autofillable:
    //
    //   NO_SERVER_DATA
    //     => A type cannot be determined based on available data.
    //   UNKNOWN_TYPE
    //     => field is believed to not have an autofill type.
    //
    // Both of these are tabulated as "negative" predictions; so, to simplify
    // the logic below, map them both to UNKNOWN_TYPE.
    if (predicted_type == NO_SERVER_DATA)
      predicted_type = UNKNOWN_TYPE;
    switch (metric) {
      case AutofillMetrics::TRUE_POSITIVE:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               predicted_type == actual_type;

      case AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS:
        return actual_type == AMBIGUOUS_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::TRUE_NEGATIVE_UNKNOWN:
        return actual_type == UNKNOWN_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::TRUE_NEGATIVE_EMPTY:
        return actual_type == EMPTY_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_AMBIGUOUS:
        return actual_type == AMBIGUOUS_TYPE && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_UNKNOWN:
        return actual_type == UNKNOWN_TYPE && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_EMPTY:
        return actual_type == EMPTY_TYPE && predicted_type != UNKNOWN_TYPE;

      // False negative mismatch and false positive mismatch trigger on the same
      // conditions:
      //   - False positive prediction of predicted type
      //   - False negative prediction of actual type
      case AutofillMetrics::FALSE_POSITIVE_MISMATCH:
      case AutofillMetrics::FALSE_NEGATIVE_MISMATCH:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               actual_type != predicted_type && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_NEGATIVE_UNKNOWN:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               actual_type != predicted_type && predicted_type == UNKNOWN_TYPE;

      default:
        NOTREACHED();
    }
    return false;
  }

  static int FieldTypeCross(ServerFieldType predicted_type,
                            ServerFieldType actual_type) {
    EXPECT_LE(predicted_type, UINT16_MAX);
    EXPECT_LE(actual_type, UINT16_MAX);
    return (predicted_type << 16) | actual_type;
  }

  const ServerFieldTypeSet unknown_equivalent_types_{UNKNOWN_TYPE, EMPTY_TYPE,
                                                     AMBIGUOUS_TYPE};
};

TEST_P(QualityMetricsTest, Classification) {
  const std::vector<std::string> prediction_sources{"Heuristic", "Server",
                                                    "Overall"};
  // Setup the test parameters.
  ServerFieldType actual_field_type = GetParam().actual_field_type;
  ServerFieldType predicted_type = GetParam().predicted_field_type;

  DVLOG(2) << "Test Case = Predicted: "
           << AutofillType::ServerFieldTypeToString(predicted_type) << "; "
           << "Actual: "
           << AutofillType::ServerFieldTypeToString(actual_field_type);

  FormData form = CreateForm(
      {CreateField("first", "first", ValueForType(NAME_FIRST), "text"),
       CreateField("last", "last", ValueForType(NAME_LAST), "test"),
       CreateField("Unknown", "Unknown", ValueForType(actual_field_type),
                   "text")});

  // Resolve any field type ambiguity.
  if (actual_field_type == AMBIGUOUS_TYPE) {
    if (predicted_type == COMPANY_NAME || predicted_type == NAME_MIDDLE)
      actual_field_type = predicted_type;
  }

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FIRST, NAME_LAST,
      predicted_type == NO_SERVER_DATA ? UNKNOWN_TYPE : predicted_type};
  std::vector<ServerFieldType> server_types = {NAME_FIRST, NAME_LAST,
                                               predicted_type};
  std::vector<ServerFieldType> actual_types = {NAME_FIRST, NAME_LAST,
                                               actual_field_type};

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Run the form submission code while tracking the histograms.
  base::HistogramTester histogram_tester;
  SubmitForm(form);

  ExpectedUkmMetrics expected_ukm_metrics;
  AppendFieldTypeUkm(form, heuristic_types, server_types, actual_types,
                     &expected_ukm_metrics);
  VerifyUkm(test_ukm_recorder_, form, UkmFieldTypeValidationType::kEntryName,
            expected_ukm_metrics);

  // Validate the total samples and the crossed (predicted-to-actual) samples.
  for (const auto& source : prediction_sources) {
    const std::string crossed_histogram = "Autofill.FieldPrediction." + source;
    const std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source;
    const std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;

    // Sanity Check:
    histogram_tester.ExpectTotalCount(crossed_histogram, 3);
    histogram_tester.ExpectTotalCount(aggregate_histogram, 3);
    histogram_tester.ExpectTotalCount(
        by_field_type_histogram,
        2 +
            (predicted_type != UNKNOWN_TYPE &&
             predicted_type != NO_SERVER_DATA &&
             predicted_type != actual_field_type) +
            (unknown_equivalent_types_.count(actual_field_type) == 0));

    // The Crossed Histogram:
    EXPECT_THAT(histogram_tester.GetAllSamples(crossed_histogram),
                BucketsInclude(
                    Bucket(FieldTypeCross(NAME_FIRST, NAME_FIRST), 1),
                    Bucket(FieldTypeCross(NAME_LAST, NAME_LAST), 1),
                    Bucket(FieldTypeCross((predicted_type == NO_SERVER_DATA &&
                                                   source != "Server"
                                               ? UNKNOWN_TYPE
                                               : predicted_type),
                                          actual_field_type),
                           1)));
  }

  // Validate the individual histogram counter values.
  for (int i = 0; i < AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS; ++i) {
    // The metric enum value we're currently examining.
    auto metric = static_cast<AutofillMetrics::FieldTypeQualityMetric>(i);

    // The type specific expected count is 1 if (predicted, actual) is an
    // example
    int basic_expected_count =
        IsExampleOf(metric, predicted_type, actual_field_type) ? 1 : 0;

    // For aggregate metrics don't capture aggregate FALSE_POSITIVE_MISMATCH.
    // Note there are two true positive values (first and last name) hard-
    // coded into the test.
    int aggregate_expected_count =
        (metric == AutofillMetrics::TRUE_POSITIVE ? 2 : 0) +
        (metric == AutofillMetrics::FALSE_POSITIVE_MISMATCH
             ? 0
             : basic_expected_count);

    // If this test exercises the ambiguous middle name match, then validation
    // of the name-specific metrics must include the true-positives created by
    // the first and last name fields.
    if (metric == AutofillMetrics::TRUE_POSITIVE &&
        predicted_type == NAME_MIDDLE && actual_field_type == NAME_MIDDLE) {
      basic_expected_count += 2;
    }

    // For metrics keyed to the actual field type, we don't capture unknown,
    // empty or ambiguous and we don't capture false positive mismatches.
    int expected_count_for_actual_type =
        (unknown_equivalent_types_.count(actual_field_type) == 0 &&
         metric != AutofillMetrics::FALSE_POSITIVE_MISMATCH)
            ? basic_expected_count
            : 0;

    // For metrics keyed to the predicted field type, we don't capture unknown
    // (empty is not a predictable value) and we don't capture false negative
    // mismatches.
    int expected_count_for_predicted_type =
        (predicted_type != UNKNOWN_TYPE && predicted_type != NO_SERVER_DATA &&
         metric != AutofillMetrics::FALSE_NEGATIVE_MISMATCH)
            ? basic_expected_count
            : 0;

    for (const auto& source : prediction_sources) {
      std::string aggregate_histogram =
          "Autofill.FieldPredictionQuality.Aggregate." + source;
      std::string by_field_type_histogram =
          "Autofill.FieldPredictionQuality.ByFieldType." + source;
      histogram_tester.ExpectBucketCount(aggregate_histogram, metric,
                                         aggregate_expected_count);
      histogram_tester.ExpectBucketCount(
          by_field_type_histogram,
          GetFieldTypeGroupPredictionQualityMetric(actual_field_type, metric),
          expected_count_for_actual_type);
      histogram_tester.ExpectBucketCount(
          by_field_type_histogram,
          GetFieldTypeGroupPredictionQualityMetric(predicted_type, metric),
          expected_count_for_predicted_type);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTest,
    QualityMetricsTest,
    testing::Values(QualityMetricsTestCase{NO_SERVER_DATA, EMPTY_TYPE},
                    QualityMetricsTestCase{NO_SERVER_DATA, UNKNOWN_TYPE},
                    QualityMetricsTestCase{NO_SERVER_DATA, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{NO_SERVER_DATA, EMAIL_ADDRESS},
                    QualityMetricsTestCase{EMAIL_ADDRESS, EMPTY_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, UNKNOWN_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, EMAIL_ADDRESS},
                    QualityMetricsTestCase{EMAIL_ADDRESS, COMPANY_NAME},
                    QualityMetricsTestCase{COMPANY_NAME, EMAIL_ADDRESS},
                    QualityMetricsTestCase{NAME_MIDDLE, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{COMPANY_NAME, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, EMPTY_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, UNKNOWN_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, EMAIL_ADDRESS}));

// Ensures that metrics that measure timing some important Autofill functions
// actually are recorded and retrieved.
TEST_F(AutofillMetricsTest, TimingMetrics) {
  base::HistogramTester histogram_tester;
  FormData form = CreateForm(
      {CreateField("Autofilled", "autofilled", "Elvis Aaron Presley", "text"),
       CreateField("Autofill Failed", "autofillfailed", "buddy@gmail.com",
                   "text"),
       CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = false;
  form.fields[2].is_autofilled = false;

  SeeForm(form);

  // Because these metrics are related to timing, it is not possible to know in
  // advance which bucket the sample will fall into, so we just need to make
  // sure we have valid samples.
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.DetermineHeuristicTypes")
          .empty());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    EXPECT_FALSE(
        histogram_tester.GetAllSamples("Autofill.Timing.ParseForm").empty());
  } else {
    EXPECT_FALSE(
        histogram_tester.GetAllSamples("Autofill.Timing.ParseFormsAsync")
            .empty());
    EXPECT_FALSE(
        histogram_tester
            .GetAllSamples("Autofill.Timing.ParseFormsAsync.RunHeuristics")
            .empty());
    EXPECT_FALSE(
        histogram_tester
            .GetAllSamples("Autofill.Timing.ParseFormsAsync.UpdateCache")
            .empty());
  }
}

// Test that we log quality metrics appropriately when an upload is triggered
// but no submission event is sent.
TEST_F(AutofillMetricsTest, QualityMetrics_NoSubmission) {
  FormData form =
      CreateForm({CreateField("Autofilled", "autofilled", "Elvis", "text"),
                  CreateField("Autofill Failed", "autofillfailed",
                              "buddy@gmail.com", "text"),
                  CreateField("Empty", "empty", "", "text"),
                  CreateField("Unknown", "unknown", "garbage", "text"),
                  CreateField("Select", "select", "USA", "select-one"),
                  CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields.front().is_autofilled = true;
  form.fields.back().is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,         PHONE_HOME_NUMBER, NAME_FULL,
      PHONE_HOME_NUMBER, UNKNOWN_TYPE,      PHONE_HOME_CITY_AND_NUMBER};

  std::vector<ServerFieldType> server_types = {
      NAME_FIRST,    EMAIL_ADDRESS,  NAME_FIRST,
      EMAIL_ADDRESS, NO_SERVER_DATA, PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  // Changes the name field to match the full name.
  SimulateUserChangedTextFieldTo(form, form.fields[0], u"Elvis Aaron Presley");

  base::HistogramTester histogram_tester;

  // Triggers the metrics.
  autofill_manager().Reset();

  auto Buck = [](ServerFieldType field_type,
                 AutofillMetrics::FieldTypeQualityMetric metric, size_t n) {
    return Bucket(GetFieldTypeGroupPredictionQualityMetric(field_type, metric),
                  n);
  };

  for (const std::string source : {"Heuristic", "Server", "Overall"}) {
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.FieldPredictionQuality.Aggregate." + source +
                    ".NoSubmission"),
                BucketsAre(Bucket(AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1),
                           Bucket(AutofillMetrics::TRUE_POSITIVE, 2),
                           Bucket(AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
                           Bucket(AutofillMetrics::FALSE_POSITIVE_EMPTY, 1),
                           Bucket(AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1)))
        << "source=" << source;
  }

  // Heuristic predictions.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldPredictionQuality.ByFieldType.Heuristic.NoSubmission"),
      BucketsAre(
          Buck(ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN,
               1),
          Buck(NAME_FULL, AutofillMetrics::TRUE_POSITIVE, 1),
          Buck(PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::TRUE_POSITIVE, 1),
          Buck(EMAIL_ADDRESS, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
          Buck(PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_MISMATCH, 1),
          Buck(NAME_FULL, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1),
          Buck(PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1)));

  // Server predictions override heuristics, so server and overall will be the
  // same.
  for (const std::string source : {"Server", "Overall"}) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FieldPredictionQuality.ByFieldType." + source +
            ".NoSubmission"),
        BucketsAre(
            Buck(ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN,
                 1),
            Buck(EMAIL_ADDRESS, AutofillMetrics::TRUE_POSITIVE, 1),
            Buck(PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::TRUE_POSITIVE, 1),
            Buck(NAME_FULL, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1),
            Buck(NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_MISMATCH, 1),
            Buck(NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1),
            Buck(EMAIL_ADDRESS, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1)))
        << "source=" << source;
  }
}

// Test that we log quality metrics for heuristics and server predictions based
// on autocomplete attributes present on the fields.
TEST_F(AutofillMetricsTest, QualityMetrics_BasedOnAutocomplete) {
  FormData form = CreateForm(
      {// Heuristic value will match with Autocomplete attribute.
       CreateField("Last Name", "lastname", "", "text", "family-name"),
       // Heuristic value will NOT match with Autocomplete attribute.
       CreateField("First Name", "firstname", "", "text", "additional-name"),
       // Heuristic value will be unknown.
       CreateField("Garbage label", "garbage", "", "text", "postal-code"),
       // No autocomplete attribute. No metric logged.
       CreateField("Address", "address", "", "text", "")});

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(form);
  FormStructure* form_structure_ptr = form_structure.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  ASSERT_TRUE(
      autofill_manager()
          .mutable_form_structures_for_test()
          ->emplace(form_structure_ptr->global_id(), std::move(form_structure))
          .second);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // Server response will match with autocomplete.
  AddFieldPredictionToForm(form.fields[0], NAME_LAST, form_suggestion);
  // Server response will NOT match with autocomplete.
  AddFieldPredictionToForm(form.fields[1], NAME_FIRST, form_suggestion);
  // Server response will have no data.
  AddFieldPredictionToForm(form.fields[2], NO_SERVER_DATA, form_suggestion);
  // Not logged.
  AddFieldPredictionToForm(form.fields[3], NAME_MIDDLE, form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  autofill_manager().OnLoadedServerPredictionsForTest(
      response_string, test::GetEncodedSignatures(*form_structure_ptr));

  // Verify that FormStructure::ParseApiQueryResponse was called (here and
  // below).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerQueryResponse"),
      BucketsInclude(Bucket(AutofillMetrics::QUERY_RESPONSE_RECEIVED, 1),
                     Bucket(AutofillMetrics::QUERY_RESPONSE_PARSED, 1)));

  // Autocomplete-derived types are eventually what's inferred.
  EXPECT_EQ(NAME_LAST, form_structure_ptr->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE,
            form_structure_ptr->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure_ptr->field(2)->Type().GetStorableType());

  for (const std::string source : {"Heuristic", "Server"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source +
        ".BasedOnAutocomplete";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source +
        ".BasedOnAutocomplete";

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            ADDRESS_HOME_ZIP, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_LAST, AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_MIDDLE, AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);

    // Sanity check.
    histogram_tester.ExpectTotalCount(aggregate_histogram, 3);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 4);
  }
}

// Test that we log UPI Virtual Payment Address.
TEST_F(AutofillMetricsTest, UpiVirtualPaymentAddress) {
  FormData form = CreateForm(
      {// Heuristic value will match with Autocomplete attribute.
       CreateField("Last Name", "lastname", "", "text"),
       // Heuristic value will NOT match with Autocomplete attribute.
       CreateField("First Name", "firstname", "", "text"),
       // Heuristic value will NOT match with Autocomplete attribute.
       CreateField("Payment Address", "payment_address", "user@upi", "text")});

  std::vector<ServerFieldType> field_types = {NAME_LAST, NAME_FIRST,
                                              ADDRESS_HOME_LINE1};

  autofill_manager().AddSeenForm(form, field_types);
  base::HistogramTester histogram_tester;
  SubmitForm(form);

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::USER_DID_ENTER_UPI_VPA, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::USER_DID_ENTER_UPI_VPA,
                                     1);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Unknown", 0);
}

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(AutofillMetricsTest, SaneMetricsWithCacheMismatch) {
  FormData form = CreateForm(
      {CreateField("Both match", "match", "Elvis Aaron Presley", "text"),
       CreateField("Both mismatch", "mismatch", "buddy@gmail.com", "text"),
       CreateField("Only heuristics match", "mixed", "Memphis", "text"),
       CreateField("Unknown", "unknown", "garbage", "text")});
  form.fields.front().is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL, PHONE_HOME_NUMBER, ADDRESS_HOME_CITY, UNKNOWN_TYPE};
  std::vector<ServerFieldType> server_types = {NAME_FULL, PHONE_HOME_NUMBER,
                                               PHONE_HOME_NUMBER, UNKNOWN_TYPE};

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Add a field and re-arrange the remaining form fields before submitting.
  std::vector<FormFieldData> cached_fields = form.fields;
  form.fields = {CreateField("New field", "new field", "Tennessee", "text"),
                 cached_fields[2], cached_fields[1], cached_fields[3],
                 cached_fields[0]};

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  for (const std::string source : {"Heuristic", "Server", "Overall"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source;
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            ADDRESS_HOME_STATE, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE,
                                       source == "Heuristic" ? 2 : 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FULL, AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
                                       source == "Heuristic" ? 1 : 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            EMAIL_ADDRESS, AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    // Source dependent:
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            ADDRESS_HOME_CITY, source == "Heuristic"
                                   ? AutofillMetrics::TRUE_POSITIVE
                                   : AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
  }
}

// Verify that when submitting an autofillable form, the stored profile metric
// is logged.
TEST_F(AutofillMetricsTest, StoredProfileCountAutofillableFormSubmission) {
  // Three fields is enough to make it an autofillable form.
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text"),
                              CreateField("Phone", "phone", "", "text")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  SubmitForm(form);

  // An autofillable form was submitted, and the number of stored profiles is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 2, 1);
}

// Verify that when submitting a non-autofillable form, the stored profile
// metric is not logged.
TEST_F(AutofillMetricsTest, StoredProfileCountNonAutofillableFormSubmission) {
  // Two fields is not enough to make it an autofillable form.
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  SubmitForm(form);

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Verify that when submitting an autofillable form, the proper tppe of
// the edited fields is correctly logged to UKM.
TEST_F(AutofillMetricsTest, TypeOfEditedAutofilledFieldsUkmLogging) {
  FormData form = CreateForm(
      {CreateField("Autofilled", "autofilled", "Elvis Aaron Presley", "text"),
       CreateField("Autofill Failed", "autofillfailed", "buddy@gmail.com",
                   "text"),
       CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = true;
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS,
                                                  PHONE_HOME_CITY_AND_NUMBER};

  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS,
                                               PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Verify that there are no counts before form submission.

  EXPECT_EQ(0U, test_ukm_recorder_->entries_count());

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields[0]);

  SubmitForm(form);
  ExpectedUkmMetricsRecord name_field_ukm_record{
      {UkmEditedAutofilledFieldAtSubmission::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
      {UkmEditedAutofilledFieldAtSubmission::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UkmEditedAutofilledFieldAtSubmission::kOverallTypeName,
       static_cast<int64_t>(NAME_FULL)}};

  VerifyUkm(test_ukm_recorder_, form,
            UkmEditedAutofilledFieldAtSubmission::kEntryName,
            {name_field_ukm_record});
}

// Tests the logging of type-specific field-wise correctness.
TEST_F(AutofillMetricsTest, EditedAutofilledFieldAtSubmission) {
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.role = NAME_FULL,
                  .value = u"Elvis Aaron Presley",
                  .is_autofilled = true},
                 {.role = EMAIL_ADDRESS,
                  .value = u"buddy@gmail.com",
                  .is_autofilled = true},
                 {.role = PHONE_HOME_CITY_AND_NUMBER, .is_autofilled = true}},
      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields[0]);
  SimulateUserChangedTextField(form, form.fields[1]);

  SubmitForm(form);

  // The |NAME_FULL| field was edited (bucket 112).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType", 112, 1);

  // The |EMAIL_ADDRESS| field was edited (bucket 144).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType", 144, 1);

  // The |PHONE_HOME_CITY_AND_NUMBER| field was not edited (bucket 209).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType", 209, 1);

  // The aggregated histogram should have two counts on edited fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission2.Aggregate", 0, 2);

  // The aggregated histogram should have one count on accepted fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission2.Aggregate", 1, 1);

  // The autocomplete!=off histogram should have one count on accepted fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.Autocomplete.NotOff.EditedAutofilledFieldAtSubmission2."
      "Address",
      1, 1);

  // The autocomplete!=off histogram should have no count on accepted fields.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.Off.EditedAutofilledFieldAtSubmission2.Address",
      0);
}

// Verify that when submitting an autofillable form, the proper tppe of
// the edited fields is correctly logged to UMA.
TEST_F(AutofillMetricsTest, TypeOfEditedAutofilledFieldsUmaLogging_Deprecated) {
  FormData form = CreateForm(
      {CreateField("Autofilled", "autofilled", "Elvis Aaron Presley", "text"),
       CreateField("Autofill Failed", "autofillfailed", "buddy@gmail.com",
                   "text"),
       CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = true;
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> field_types = {NAME_FULL, EMAIL_ADDRESS,
                                              PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  // TODO(crbug.com/1368096): Fix the metric to work independent of the final
  // value.
  SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(form,
                                                              form.fields[0]);
  SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(form,
                                                              form.fields[1]);

  SubmitForm(form);

  // The |NAME_FULL| field was edited (bucket 112).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission.ByFieldType", 112, 1);

  // The |NAME_FULL| field was edited (bucket 144).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission.ByFieldType", 144, 1);

  // The |PHONE_HOME_CITY_AND_NUMBER| field was not edited (bucket 209).
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission.ByFieldType", 209, 1);

  // The aggregated histogram should have two counts on edited fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission.Aggregate", 0, 2);

  // The aggregated histogram should have one count on accepted fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.EditedAutofilledFieldAtSubmission.Aggregate", 1, 1);

  // The autocomplete!=off histogram should have one count on accepted fields.
  histogram_tester.ExpectBucketCount(
      "Autofill.Autocomplete.NotOff.EditedAutofilledFieldAtSubmission.Address",
      1, 1);

  // The autocomplete!=off histogram should have no count on accepted fields.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.Off.EditedAutofilledFieldAtSubmission.Address", 0);
}

// Verify that when submitting an autofillable form, the proper number of edited
// fields is logged.
TEST_F(AutofillMetricsTest, NumberOfEditedAutofilledFields) {
  // Three fields is enough to make it an autofillable form.
  FormData form = CreateForm(
      {CreateField("Autofilled", "autofilled", "Elvis Aaron Presley", "text"),
       CreateField("Autofill Failed", "autofillfailed", "buddy@gmail.com",
                   "text"),
       CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = true;
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> field_types = {NAME_FULL, EMAIL_ADDRESS,
                                              PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields[0]);
  SimulateUserChangedTextField(form, form.fields[1]);

  SubmitForm(form);

  // An autofillable form was submitted, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission", 2, 1);
}

// Verify that when resetting the autofill manager (such as during a
// navigation), the proper number of edited fields is logged.
TEST_F(AutofillMetricsTest, NumberOfEditedAutofilledFields_NoSubmission) {
  // Three fields is enough to make it an autofillable form.
  FormData form = CreateForm(
      {CreateField("Autofilled", "autofilled", "Elvis Aaron Presley", "text"),
       CreateField("Autofill Failed", "autofillfailed", "buddy@gmail.com",
                   "text"),
       CreateField("Phone", "phone", "2345678901", "tel")});
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = true;
  form.fields[2].is_autofilled = true;

  std::vector<ServerFieldType> field_types = {NAME_FULL, EMAIL_ADDRESS,
                                              PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first field.
  SimulateUserChangedTextField(form, form.fields[0]);

  // We expect metrics to be logged when the manager is reset.
  autofill_manager().Reset();

  // An autofillable form was uploaded, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission", 1, 1);
}

// Verify that we correctly log metrics regarding developer engagement.
TEST_F(AutofillMetricsTest, DeveloperEngagement) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text")});

  // Ensure no metrics are logged when small form support is disabled (min
  // number of fields enforced).
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    autofill_manager().Reset();
    histogram_tester.ExpectTotalCount("Autofill.DeveloperEngagement", 0);
  }

  // Add another field to the form, so that it becomes fillable.
  form.fields.push_back(CreateField("Phone", "phone", "", "text"));

  // Expect the "form parsed without hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    autofill_manager().Reset();
    histogram_tester.ExpectUniqueSample(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS, 1);
  }

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  form.fields.push_back(CreateField("", "", "", "text", "given-name"));
  form.fields.push_back(CreateField("", "", "", "text", "email"));
  form.fields.push_back(CreateField("", "", "", "text", "address-line1"));

  // Expect the "form parsed with field type hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    autofill_manager().Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);

    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 0);
  }

  // Add a field with an author-specified UPI-VPA field type in the form.
  form.fields.push_back(CreateField("", "", "", "text", "upi-vpa"));

  // Expect the "form parsed with type hints" metric, and the
  // "author-specified upi-vpa type" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    autofill_manager().Reset();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.DeveloperEngagement"),
        BucketsInclude(
            Bucket(AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1),
            Bucket(AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 1)));
  }
}

// Verify that we correctly log UKM for form parsed without type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithoutTypeHints) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text")});

  // Ensure no entries are logged when loading a non-fillable form.
  {
    SeeForm(form);
    autofill_manager().Reset();

    EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
  }

  // Add another field to the form, so that it becomes fillable.
  form.fields.push_back(CreateField("Phone", "phone", "", "text"));

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    SeeForm(form);
    autofill_manager().Reset();

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form, /*is_for_credit_card=*/false,
        {FormType::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithTypeHints) {
  // The latter three fields have an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  FormData form =
      CreateForm({CreateField("Name", "name", "", "text"),
                  CreateField("Email", "email", "", "text"),
                  CreateField("Phone", "phone", "", "text"),
                  CreateField("", "", "", "text", "given-name"),
                  CreateField("", "", "", "text", "email"),
                  CreateField("", "", "", "text", "address-line1")});

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    SeeForm(form);
    autofill_manager().Reset();

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form, /*is_for_credit_card=*/false,
        {FormType::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest, UkmDeveloperEngagement_LogUpiVpaTypeHint) {
  FormData form =
      CreateForm({CreateField("Name", "name", "", "text"),
                  CreateField("Email", "email", "", "text"),
                  CreateField("Payment", "payment", "", "text", "upi-vpa"),
                  CreateField("", "", "", "text", "address-line1")});

  {
    SCOPED_TRACE("VPA and other autocomplete hint present");
    SeeForm(form);

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form, /*is_for_credit_card=*/false,
        /* UPI VPA has Unknown form type.*/
        {FormType::kAddressForm, FormType::kUnknownFormType},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS,
         AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT});
    PurgeUKM();
  }
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardMetrics) {
  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded now_exploded;
  base::Time::Exploded one_month_ago_exploded;
  now.LocalExplode(&now_exploded);
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type: 1 of each for local,
  // 2 of each for masked, and 3 of each for unmasked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::LOCAL_CARD, CreditCard::MASKED_SERVER_CARD,
      CreditCard::FULL_SERVER_CARD};
  int num_cards_of_type = 0;
  for (auto record_type : record_types) {
    num_cards_of_type += 1;
    for (int i = 0; i < num_cards_of_type; ++i) {
      // Create a card that's still in active use.
      CreditCard card_in_use = test::GetRandomCreditCard(record_type);
      card_in_use.set_use_date(now - base::Days(30));
      card_in_use.set_use_count(10);

      // Create a card that's not in active use.
      CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
      card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
      card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
      card_in_disuse.set_use_date(now - base::Days(200));
      card_in_disuse.set_use_count(10);

      // Add the cards to the personal data manager in the appropriate way.
      auto& repo =
          (record_type == CreditCard::LOCAL_CARD) ? local_cards : server_cards;
      repo.push_back(std::make_unique<CreditCard>(std::move(card_in_use)));
      repo.push_back(std::make_unique<CreditCard>(std::move(card_in_disuse)));
    }
  }

  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/2,
      base::Days(180));

  // Validate the basic count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 12, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server",
                                     10, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 4, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 6, 1);

  // Validate the disused count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardDisusedCount", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardDisusedCount", 6,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 5, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Masked", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Unmasked", 3, 1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 12);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 2);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 10);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 4);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 30, 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 200, 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 30, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 200, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 30, 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 200, 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 200, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 30, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 200, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage", 2, 1);
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardWithNicknameMetrics) {
  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(4);

  // Create cards with and without nickname of each record type: 1 of each for
  // local, 2 of each for masked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::LOCAL_CARD, CreditCard::MASKED_SERVER_CARD};
  int num_cards_of_type = 0;
  for (auto record_type : record_types) {
    num_cards_of_type += 1;
    for (int i = 0; i < num_cards_of_type; ++i) {
      // Create a card with a nickname.
      CreditCard card_with_nickname = test::GetRandomCreditCard(record_type);
      card_with_nickname.SetNickname(u"Valid nickname");

      // Create a card that doesn't have a nickname.
      CreditCard card_without_nickname = test::GetRandomCreditCard(record_type);
      // Set nickname to empty.
      card_without_nickname.SetNickname(u"");

      // Add the cards to the personal data manager in the appropriate way.
      auto& repo =
          (record_type == CreditCard::LOCAL_CARD) ? local_cards : server_cards;
      repo.push_back(
          std::make_unique<CreditCard>(std::move(card_with_nickname)));
      repo.push_back(
          std::make_unique<CreditCard>(std::move(card_without_nickname)));
    }
  }

  // Log the stored credit card metrics for the cards configured above.
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/0,
      base::Days(180));

  // Validate the count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Local.WithNickname", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked.WithNickname", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Local.WithNickname", 1, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 4, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked.WithNickname", 2, 1);
}

// Test that we correctly log when Profile Autofill is enabled at startup.
TEST_F(AutofillMetricsTest, AutofillProfileIsEnabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data().SetAutofillProfileEnabled(true);
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr,
                       /*is_off_the_record=*/false);
  histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.Startup",
                                      true, 1);
}

// Test that we correctly log when Profile Autofill is disabled at startup.
TEST_F(AutofillMetricsTest, AutofillProfileIsDisabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data().SetAutofillProfileEnabled(false);
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr,
                       /*is_off_the_record=*/false);
  histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.Startup",
                                      false, 1);
}

// Test that we correctly log when CreditCard Autofill is enabled at startup.
TEST_F(AutofillMetricsTest, AutofillCreditCardIsEnabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data().SetAutofillCreditCardEnabled(true);
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr,
                       /*is_off_the_record=*/false);
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.Startup",
                                      true, 1);
}

// Test that we correctly log when CreditCard Autofill is disabled at startup.
TEST_F(AutofillMetricsTest, AutofillCreditCardIsDisabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data().SetAutofillCreditCardEnabled(false);
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr,
                       /*is_off_the_record=*/false);
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.Startup",
                                      false, 1);
}

// Test that we log the number of Autofill suggestions when filling a form.
TEST_F(AutofillMetricsTest, AddressSuggestionsCount) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "email"),
                              CreateField("Phone", "phone", "", "tel")});
  std::vector<ServerFieldType> field_types = {NAME_FULL, EMAIL_ADDRESS,
                                              PHONE_HOME_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }

  FormFieldData email_field = CreateField("Email", "email", "b", "email");
  {
    // Simulate activating the autofill popup for the email field after typing.
    // No new metric should be logged, since we're still on the same page.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, email_field);
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, email_field);
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 1,
                                        1);
  }

  // Reset the autofill manager state again.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the email field after a fill.
    form.fields[0].is_autofilled = true;
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, email_field);
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 1);
  }
}

// Test that we log the correct number of Company Name Autofill suggestions when
// filling a form.
TEST_F(AutofillMetricsTest, CompanyNameSuggestions) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "email"),
                              CreateField("Company", "company", "", "text")});

  std::vector<ServerFieldType> field_types = {NAME_FULL, EMAIL_ADDRESS,
                                              COMPANY_NAME};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }
}

// Test that the credit card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, CreditCardCheckoutFlowUserActions) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Name on card", "cc-name", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text"),
                  CreateField("Expiration date", "expdate", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Name on card" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate_->OnQuery(form, form.fields.front(), gfx::RectF());

    external_delegate_->DidAcceptSuggestion(
        test::CreateAutofillSuggestion(
            MakeFrontendId({.credit_card_id = kTestLocalCardId}), u"Test",
            Suggestion::BackendId(kTestLocalCardId)),
        0);

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field along with a "Clear form" footer suggestion.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a "Clear form" suggestion.
  {
    base::UserActionTester user_action_tester;
    external_delegate_->OnQuery(form, form.fields.front(), gfx::RectF());

    external_delegate_->DidAcceptSuggestion(
        Suggestion(POPUP_ITEM_ID_CLEAR_FORM), 0);

    EXPECT_EQ(1, user_action_tester.GetActionCount("Autofill_ClearedForm"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field, this time to submit the form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate_->OnQuery(form, form.fields.front(), gfx::RectF());

    external_delegate_->DidAcceptSuggestion(
        test::CreateAutofillSuggestion(
            MakeFrontendId({.credit_card_id = kTestLocalCardId}), u"Test",
            Suggestion::BackendId(kTestLocalCardId)),
        0);

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a credit card suggestion.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledCreditCardSuggestion"));
  }

  // Simulate submitting the credit card form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));
  }

  // Expect one record for a click on the cardholder name field and one record
  // for each of the 3 clicks on the card number field.
  ExpectedUkmMetricsRecord name_field_record{
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
      {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
       HtmlFieldType::kUnspecified},
      {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NAME_FULL},
      {UkmSuggestionsShownType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
      {UkmSuggestionsShownType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  ExpectedUkmMetricsRecord number_field_record{
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
      {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
       HtmlFieldType::kUnspecified},
      {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
      {UkmSuggestionsShownType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields[1])).value()},
      {UkmSuggestionsShownType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  VerifyUkm(test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
            {name_field_record, number_field_record, number_field_record,
             number_field_record});

  // Expect 3 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate_->DidAcceptSuggestion|. Second and third, from
  // ExpectedUkmMetrics |autofill_manager().FillOrPreviewForm|.
  ExpectedUkmMetricsRecord from_did_accept_suggestion{
      {UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmSuggestionFilledType::kIsForCreditCardName, true},
      {UkmSuggestionFilledType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields.front())).value()},
      {UkmSuggestionFilledType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  ExpectedUkmMetricsRecord from_fill_or_preview_form{
      {UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmSuggestionFilledType::kIsForCreditCardName, true},
      {UkmSuggestionFilledType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields.front())).value()},
      {UkmSuggestionFilledType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  VerifyUkm(test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
            {from_did_accept_suggestion, from_fill_or_preview_form,
             from_fill_or_preview_form});

  // Expect |NON_FILLABLE_FORM_OR_NEW_DATA| in |AutofillFormSubmittedState|
  // because |field.value| is empty in |DeterminePossibleFieldTypesForUpload|.
  VerifySubmitFormUkm(test_ukm_recorder_, form,
                      AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                      /*is_for_credit_card=*/true, /*has_upi_vpa_field=*/false,
                      {FormType::kCreditCardForm}, {.autofill_fills = 3});
}

// Test that the UPI Checkout flow form submit is correctly logged
TEST_F(AutofillMetricsTest, UpiVpaUkmTest) {
  FormData form = CreateForm(
      {CreateField("Enter VPA", "upi-vpa", "unique_id@upi", "text")});

  {
    SeeForm(form);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /*has_upi_vpa_field=*/true,
                        // UPI VPA has Unknown form type.
                        {FormType::kAddressForm, FormType::kUnknownFormType});
    PurgeUKM();
  }
}

// Test that the profile checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, ProfileCheckoutFlowUserActions) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a profile field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "State" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "City" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate selecting a profile suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate_->OnQuery(form, form.fields.front(), gfx::RectF());

    external_delegate_->DidAcceptSuggestion(
        test::CreateAutofillSuggestion(
            MakeFrontendId({.profile_id = kTestProfileId}), u"Test",
            Suggestion::BackendId(kTestProfileId)),
        0);

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a profile suggestion.
  {
    base::UserActionTester user_action_tester;
    FillTestProfile(form);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledProfileSuggestion"));
  }

  // Simulate submitting the profile form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));
  }

  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_STATE},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_STATE},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_CITY},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_CITY},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[1])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
  // Expect 2 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate_->DidAcceptSuggestion|. Second, from call to
  // |autofill_manager().FillOrPreviewForm|.
  VerifyUkm(test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
            {{{UkmSuggestionFilledType::kRecordTypeName,
               AutofillProfile::LOCAL_PROFILE},
              {UkmSuggestionFilledType::kIsForCreditCardName, false},
              {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
              {UkmSuggestionFilledType::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields.front()))
                   .value()},
              {UkmSuggestionFilledType::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}},
             {{UkmSuggestionFilledType::kRecordTypeName,
               AutofillProfile::LOCAL_PROFILE},
              {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
              {UkmSuggestionFilledType::kIsForCreditCardName, false},
              {UkmSuggestionsShownType::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields.front()))
                   .value()},
              {UkmSuggestionsShownType::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}}});
  // Expect |NON_FILLABLE_FORM_OR_NEW_DATA| in |AutofillFormSubmittedState|
  // because |field.value| is empty in |DeterminePossibleFieldTypesForUpload|.
  VerifySubmitFormUkm(test_ukm_recorder_, form,
                      AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                      /*is_for_credit_card=*/false,
                      /*has_upi_vpa_field=*/false, {FormType::kAddressForm},
                      {.autofill_fills = 2});
}

// Tests that the Autofill_PolledCreditCardSuggestions user action is only
// logged once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledCreditCardSuggestions_DebounceLogs) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Name on card", "cc-name", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text"),
                  CreateField("Expiration date", "expdate", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a second query on the same field. There should still only be one
  // logged poll.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[1]);
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));
}

// Tests that the Autofill.QueriedCreditCardFormIsSecure histogram is logged
// properly.
TEST_F(AutofillMetricsTest, QueriedCreditCardFormIsSecure) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  {
    // Simulate having seen this insecure form on page load.
    form.host_frame = test::MakeLocalFrameToken();
    form.unique_renderer_id = test::MakeFormRendererId();
    form.url = GURL("http://example.com/form.html");
    form.action = GURL("http://example.com/submit.html");
    // In order to test that the QueriedCreditCardFormIsSecure is logged as
    // false, we need to set the main frame origin, otherwise this fill is
    // skipped due to the form being detected as mixed content.
    GURL client_form_origin = autofill_client_->form_origin();
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpScheme);
    autofill_client_->set_form_origin(
        client_form_origin.ReplaceComponents(replacements));
    form.main_frame_origin =
        url::Origin::Create(autofill_client_->form_origin());
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTP, non-secure
    // form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[1]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", false, 1);
    // Reset the main frame origin to secure for other tests
    autofill_client_->set_form_origin(client_form_origin);
  }

  {
    autofill_manager().Reset();
    form.host_frame = test::MakeLocalFrameToken();
    form.unique_renderer_id = test::MakeFormRendererId();
    form.url = GURL("https://example.com/form.html");
    form.action = GURL("https://example.com/submit.html");
    form.main_frame_origin =
        url::Origin::Create(autofill_client_->form_origin());
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTPS form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[1]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", true, 1);
  }
}

// Tests that the Autofill_PolledProfileSuggestions user action is only logged
// once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledProfileSuggestions_DebounceLogs) {
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a profile field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a second query on the same field. There should still only be poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[1]);
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));
}

// Test that we log parsed form event for credit card forms.
TEST_P(AutofillMetricsIFrameTest, CreditCardParsedFormEvents) {
  FormData form =
      CreateForm({CreateField("Card Number", "card_number", "", "text"),
                  CreateField("Expiration", "cc_exp", "", "text"),
                  CreateField("Verification", "verification", "", "text")});

  std::vector<ServerFieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                              CREDIT_CARD_EXP_MONTH,
                                              CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that we log interacted form event for credit cards related.
TEST_P(AutofillMetricsIFrameTest, CreditCardInteractedFormEvents) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardPopupSuppressedFormEvents) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating popup being suppressed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 1),
                       Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 1),
                               Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating popup being suppressed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 2),
                       Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 2),
                               Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardShownFormEvents) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/false, form,
                                          form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsAre(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                   Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsAre(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                           Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
  }
}

// Test that we log specific suggestion shown form events for virtual credit
// cards.
TEST_P(AutofillMetricsIFrameTest, VirtualCreditCardShownFormEvents) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("CVC", "cvc", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_VERIFICATION_CODE, CREDIT_CARD_NUMBER};

  // Creating cards, including a virtual card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*include_virtual_credit_card=*/true);

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/false, form,
                                          form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
  }

  // Recreate cards, this time *without* a virtual card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*include_virtual_credit_card=*/false);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load. Suggestions shown should be
    // logged, but suggestions shown with virtual card should not.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
  }
}

// Test that we log selected form event for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardSelectedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields[2],
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a masked server card multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields[2],
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields[2],
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual server suggestion by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual card multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }
}

// Test that we log filled form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardFilledFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a virtual card suggestion by selecting the option
    // based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(
                    Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1),
                    Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a full card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestFullServerCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }
}

// Test to log when an unique local card is autofilled, when other duplicated
// server cards exist.
TEST_P(
    AutofillMetricsIFrameTest,
    CreditCardFilledFormEventsUsingUniqueLocalCardWhenOtherDuplicateServerCardsPresent) {
  CreateLocalAndDuplicateServerCreditCard();
  // Creating a local mastercard credit card.
  std::string local_guid = CreateLocalMasterCard();

  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a unique local card suggestion.
  base::HistogramTester histogram_tester;
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(),
      MakeFrontendId({.credit_card_id = local_guid}));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(credit_card_form_events_frame_histogram_),
      BucketsInclude(
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              0)));
}

// Test to log when a local card is autofilled and its duplicated
// server card exists.
TEST_P(AutofillMetricsIFrameTest,
       CreditCardFilledFormEventsUsingDuplicateServerCard) {
  // Creating a local and a duplicate server card.
  std::string local_guid = CreateLocalAndDuplicateServerCreditCard()[0];
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a local card suggestion with a duplicate server card.
  base::HistogramTester histogram_tester;
  // Local card with a duplicate server card present at index 0.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(),
      MakeFrontendId({.credit_card_id = local_guid}));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(credit_card_form_events_frame_histogram_),
      BucketsInclude(
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              1)));
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_ServerCard) {
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Success", 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kPermanentFailure,
                    std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Failure", 1);
  }
}

// Test that a malformed or non-HTTP_OK response doesn't cause problems, per
// crbug/1267105.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_BadServerResponse) {
  // Creating masked card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  // Set up our form data.
  FormData form;
  test::CreateTestCreditCardFormData(&form,
                                     /*is_https=*/true,
                                     /*use_month_type=*/true,
                                     /*split_names=*/false);
  std::vector<ServerFieldType> field_types{
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE};
  ASSERT_EQ(form.fields.size(), field_types.size());

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnDidGetRealPanWithNonHttpOkResponse();
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.UnknownCard.Failure", 1);
  }
}

TEST_F(AutofillMetricsTest, CreditCardGetRealPanResult_ServerCard) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kTryAgainFailure,
        AutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kPermanentFailure,
        AutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_PERMANENT_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_PERMANENT_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kSuccess,
        AutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount("Autofill.UnmaskPrompt.GetRealPanResult",
                                       AutofillMetrics::PAYMENTS_RESULT_SUCCESS,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
  }
}

TEST_F(AutofillMetricsTest, CreditCardGetRealPanResult_VirtualCard) {
  base::HistogramTester histogram_tester;
  {
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kTryAgainFailure,
        AutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
  }

  {
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
        AutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
  }

  {
    AutofillMetrics::LogRealPanResult(
        AutofillClient::PaymentsRpcResult::kSuccess,
        AutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount("Autofill.UnmaskPrompt.GetRealPanResult",
                                       AutofillMetrics::PAYMENTS_RESULT_SUCCESS,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
  }
}

TEST_F(AutofillMetricsTest,
       CreditCardSubmittedWithoutSelectingSuggestionsNoCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       CreditCardSubmittedWithoutSelectingSuggestionsWrongSizeCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "411111111", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD, 1);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       CreditCardSubmittedWithoutSelectingSuggestionsFailLuhnCheckCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateField("Month", "card_month", "", "text"),
       CreateField("Year", "card_year", "", "text"),
       CreateField("Credit card", "cardnum", "4444444444444444", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD, 1);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       CreditCardSubmittedWithoutSelectingSuggestionsUnknownCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateField("Month", "card_month", "", "text"),
       CreateField("Year", "card_year", "", "text"),
       CreateField("Credit card", "cardnum", "5105105105105100", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD, 1);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       CreditCardSubmittedWithoutSelectingSuggestionsKnownCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateField("Month", "card_month", "", "text"),
       CreateField("Year", "card_year", "", "text"),
       CreateField("Credit card", "cardnum", "4111111111111111", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 1);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       ShouldNotLogSubmitWithoutSelectingSuggestionsIfSuggestionFilled) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateField("Month", "card_month", "", "text"),
       CreateField("Year", "card_year", "", "text"),
       CreateField("Credit card", "cardnum", "4111111111111111", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown and selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form, form.fields.back(),
      MakeFrontendId({.credit_card_id = kTestLocalCardId}));

  SubmitForm(form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD,
                 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(credit_card_form_events_frame_histogram_),
      BucketsInclude(
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD,
                 0)));
}

TEST_F(AutofillMetricsTest, ShouldNotLogFormEventNoCardForAddressForm) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with no filled data.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0);
}

// Test that we log submitted form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardSubmittedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(
                    Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
                    Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager().Reset();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(
                    Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
                    Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields.front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {.autofill_fills = 1});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName, CreditCard::VIRTUAL_CARD},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields.front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {.autofill_fills = 1});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestFullServerCardId}));
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifyUkm(test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
              {{{UkmSuggestionFilledType::kRecordTypeName,
                 CreditCard::FULL_SERVER_CARD},
                {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
                {UkmSuggestionFilledType::kIsForCreditCardName, true},
                {UkmSuggestionFilledType::kFieldSignatureName,
                 Collapse(CalculateFieldSignatureForField(form.fields.front()))
                     .value()},
                {UkmSuggestionFilledType::kFormSignatureName,
                 Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {.autofill_fills = 1});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));

    VerifyUkm(test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
              {{{UkmSuggestionFilledType::kRecordTypeName,
                 CreditCard::MASKED_SERVER_CARD},
                {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
                {UkmSuggestionFilledType::kIsForCreditCardName, true},
                {UkmSuggestionFilledType::kFieldSignatureName,
                 Collapse(CalculateFieldSignatureForField(form.fields.back()))
                     .value()},
                {UkmSuggestionFilledType::kFormSignatureName,
                 Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {.autofill_fills = 1});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    SubmitForm(form);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});

    SubmitForm(form);

    VerifyUkm(
        test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
        {{{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
           AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmFormSubmittedType::kIsForCreditCardName, true},
          {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
          {UkmFormSubmittedType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kCreditCardForm})},
          {UkmFormSubmittedType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()},
          {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
          {UkmFormSubmittedType::kAutofillFillsName, 0}},
         {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
           AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmFormSubmittedType::kIsForCreditCardName, true},
          {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
          {UkmFormSubmittedType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kCreditCardForm})},
          {UkmFormSubmittedType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()},
          {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
          {UkmFormSubmittedType::kAutofillFillsName, 0}}});

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /*has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }
}

// Test that we log "will submit" and "submitted" form events for credit
// cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardWillSubmitFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kTestMaskedCardId, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    // Full server card.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestFullServerCardId}));
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
  }
}

// Test that we log form events for masked server card with offers.
TEST_F(AutofillMetricsTest, LogServerOfferFormEvents) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  // Creating all kinds of cards. None of them have offers.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  const std::string kMaskedServerCardIds[] = {
      "12340000-0000-0000-0000-000000000001",
      "12340000-0000-0000-0000-000000000002",
      "12340000-0000-0000-0000-000000000003"};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1)));
    // Check that the offer sub-histogram was not recorded.
    // ExpectBucketCount() can't be used here because it expects the histogram
    // to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.WithOffer"]);

    // Ensure offers were not shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/0, 1);

    // Since no offers were shown, we should not track offer selection or
    // submission.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SelectedCardHasOffer"]);
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SubmittedCardHasOffer"]);
  }

  // Add another masked server card, this time with a linked offer.
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[0], "$4",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x4fff);
  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kMaskedServerCardIds[0]}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected and form was submitted with that card.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*selected=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*submitted=*/true, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    // Select another card, and still log to offer
    // sub-histogram because user has another masked server card with offer.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestMaskedCardId}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was not selected.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*selected=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*submitted=*/false, 1);
  }

  // Recreate cards and add card that is linked to an expired offer.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[1], "$4",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x3fff, /*offer_expired=*/true);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating activating the autofill popup for the credit card field,
    // new popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    // Select the card with linked offer, though metrics should not record it
    // since the offer is expired.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kMaskedServerCardIds[1]}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    SubmitForm(form);
    // Histograms without ".WithOffer" should be recorded.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Check that the offer sub-histogram was not recorded.
    // ExpectBucketCount() can't be used here because it expects the
    // histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.WithOffer"]);

    // Ensure offers were not shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/0, 1);

    // Since no offers were shown, we should not track offer selection or
    // submission.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SelectedCardHasOffer"]);
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SubmittedCardHasOffer"]);
  }

  // Recreate cards and add card that is linked to an offer.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[2], "$5",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x5fff);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, showing the
    // suggestions again, and then submitting the form with previously filled
    // card. Verify that all related form events are correctly logged to offer
    // sub-histogram. Making suggestions reappear tests confirmation of a fix
    // for crbug/1198751.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kMaskedServerCardIds[2]}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");

    // Simulate user showing suggestions but then submitting form with
    // previously filled card info.
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected and form was submitted with that card.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*selected=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*submitted=*/true, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // failing the CVC check and submitting the form anyways. Verify that all
    // related form events are correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    // Select the masked server card with the linked offer, but fail the CVC
    // check.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kMaskedServerCardIds[2]}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kPermanentFailure,
                    std::string());

    // Submitting the form without the filled suggestion.
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected once, but not submitted.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*selected=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SubmittedCardHasOffer",
                                       /*submitted=*/true, 0);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // selecting a local card instead. Verify that all related form events are
    // correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;

    // Show suggestions and select the card with offer.
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kMaskedServerCardIds[2]}));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");

    // Show suggestions again, and select a local card instead.
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields.back());
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.back(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SuggestedCardsHaveOffer",
                                       /*suggestions with offers=*/1, 1);

    // Should track card was only selected once.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*selected=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*selected=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*submitted=*/false, 1);
  }
}

class AutofillMetricsTestForCardMetadata
    : public AutofillMetricsTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  AutofillMetricsTestForCardMetadata()
      : card_product_name_enabled_(std::get<0>(GetParam())),
        card_art_image_enabled_(std::get<1>(GetParam())),
        card_metadata_available_(std::get<2>(GetParam())),
        card_has_linked_virtual_card_(std::get<3>(GetParam())) {
    feature_list_card_product_name_.InitWithFeatureState(
        features::kAutofillEnableCardProductName, card_product_name_enabled_);
    feature_list_card_art_image_.InitWithFeatureState(
        features::kAutofillEnableCardArtImage, card_art_image_enabled_);
  }

  ~AutofillMetricsTestForCardMetadata() override = default;

  bool card_product_name_enabled() { return card_product_name_enabled_; }
  bool card_art_image_enabled() { return card_art_image_enabled_; }
  bool card_metadata_available() { return card_metadata_available_; }
  bool card_has_linked_virtual_card() { return card_has_linked_virtual_card_; }

 private:
  const bool card_product_name_enabled_;
  const bool card_art_image_enabled_;
  const bool card_metadata_available_;
  const bool card_has_linked_virtual_card_;
  base::test::ScopedFeatureList feature_list_card_product_name_;
  base::test::ScopedFeatureList feature_list_card_art_image_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillMetricsTestForCardMetadata,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Test that we log card metadata related metrics only when card metadata is
// available.
TEST_P(AutofillMetricsTestForCardMetadata, LogCardMetadataMetrics) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  // Set up the form data. Reset form action to skip the IsFormMixedContent
  // check.
  FormData form =
      GetAndAddSeenForm({.description_for_logging = "CardMetadata",
                         .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_MONTH},
                                    {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR}},
                         .action = ""});

  CreditCard masked_server_card = test::GetMaskedServerCard();
  masked_server_card.set_guid(kTestMaskedCardId);
  masked_server_card.set_issuer_id("amex");
  if (card_metadata_available()) {
    masked_server_card.set_product_description(u"card_description");
    masked_server_card.set_card_art_url(
        GURL("https://www.example.com/cardart.png"));
  }
  if (card_has_linked_virtual_card()) {
    masked_server_card.set_virtual_card_enrollment_state(
        CreditCard::VirtualCardEnrollmentState::ENROLLED);
  }
  personal_data().AddServerCreditCard(masked_server_card);
  personal_data().Refresh();

  // Simulate activating the autofill popup for the credit card field.
  base::HistogramTester histogram_tester;
  autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
  autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields.back());
  test_clock.SetNowTicks(now + base::Seconds(2));
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(),
      MakeFrontendId({.credit_card_id = kTestMaskedCardId}));

  std::string latency_histogram_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown";

  std::string latency_histogram_suffix;
  if (card_metadata_available()) {
    // Verify the suggestion acceptance latency was logged when metadata was
    // available or the suggestions had one virtual card.
    if (card_product_name_enabled() &&
        (card_art_image_enabled() || card_has_linked_virtual_card())) {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageBothShownSuffix;
    } else if (card_product_name_enabled()) {
      latency_histogram_suffix = autofill_metrics::kProductNameShownOnlySuffix;
    } else if (card_art_image_enabled() || card_has_linked_virtual_card()) {
      latency_histogram_suffix = autofill_metrics::kArtImageShownOnlySuffix;
    } else {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageNotShownSuffix;
    }

    histogram_tester.ExpectUniqueSample(latency_histogram_prefix +
                                            ".AnyCardWithMetadata" +
                                            latency_histogram_suffix,
                                        2000, 1);
    histogram_tester.ExpectUniqueSample(latency_histogram_prefix +
                                            ".SelectedCardWithMetadata" +
                                            latency_histogram_suffix + ".Amex",
                                        2000, 1);
  } else {
    // Verify that no histogram should be logged when metadata was not
    // available and the suggestions had no virtual card.
    EXPECT_TRUE(
        histogram_tester.GetTotalCountsForPrefix(latency_histogram_prefix)
            .empty());
  }
}

// Test that we log parsed form events for address and cards in the same form.
TEST_F(AutofillMetricsTest, MixedParsedFormEvents) {
  FormData form =
      CreateForm({CreateField("State", "state", "", "text"),
                  CreateField("City", "city", "", "text"),
                  CreateField("Street", "street", "", "text"),
                  CreateField("Card Number", "card_number", "", "text"),
                  CreateField("Expiration", "cc_exp", "", "text"),
                  CreateField("Verification", "verification", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE,          ADDRESS_HOME_CITY,
      ADDRESS_HOME_STREET_ADDRESS, CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_EXP_MONTH,       CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address.WithNoData",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that we log parsed form events for address.
TEST_F(AutofillMetricsTest, AddressParsedFormEvents) {
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address.WithNoData",
                                      FORM_EVENT_DID_PARSE_FORM, 1);

  // Check if FormEvent UKM is logged properly
  auto entries =
      test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  VerifyUkm(
      test_ukm_recorder_, form, UkmFormEventType::kEntryName,
      {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
        {UkmFormEventType::kFormTypesName,
         AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
}

// Test that we log interacted form events for address.
TEST_F(AutofillMetricsTest, AddressInteractedFormEvents) {
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[2]);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(1u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_INTERACTED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[2]);
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[2]);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(1u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_INTERACTED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }
}

// Test that popup suppressed form events for address are logged.
TEST_F(AutofillMetricsTest, AddressSuppressedFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 1),
                               Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_POPUP_SUPPRESSED},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_POPUP_SUPPRESSED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    autofill_manager().DidSuppressPopup(form, form.fields[0]);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_POPUP_SUPPRESSED, 2),
                               Bucket(FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1)));

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_POPUP_SUPPRESSED},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_POPUP_SUPPRESSED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_POPUP_SUPPRESSED},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }
}

// Test that we log suggestion shown form events for address.
TEST_F(AutofillMetricsTest, AddressShownFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/false, form,
                                          form.fields[0]);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(0u, entries.size());
  }
}

// Test that we log filled form events for address.
TEST_F(AutofillMetricsTest, AddressFilledFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
    VerifyUkm(
        test_ukm_recorder_, form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_LOCAL_SUGGESTION_FILLED},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting/filling a local profile suggestion more than once.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Create a server profile and reset the autofill manager state.
  RecreateProfile(/*is_server=*/true);
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate selecting/filling a server profile suggestion.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1)));
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate selecting/filling a server profile suggestion more than once.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1)));
  }
}

// Test that we log submitted form events for address.
TEST_F(AutofillMetricsTest, AddressSubmittedFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /*has_upi_vpa_field=*/false, {FormType::kAddressForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager().Reset();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /*has_upi_vpa_field=*/false, {FormType::kAddressForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
                       Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion show but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
  }
}

// Test that we log "will submit" and "submitted" form events for address.
TEST_F(AutofillMetricsTest, AddressWillSubmitFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                          form.fields[0]);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder_->GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
  }
}

// Test that we log the phone field.
TEST_F(AutofillMetricsTest, RecordStandalonePhoneField) {
  FormData form = CreateForm({CreateField("Phone", "phone", "", "tel")});

  std::vector<ServerFieldType> field_types = {PHONE_HOME_NUMBER};
  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_INTERACTED_ONCE, 1);
}

// Test that we log interacted form event for credit cards only once.
TEST_F(AutofillMetricsTest, CreditCardFormEventsAreSegmented) {
  FormData form =
      CreateForm({CreateField("Month", "card_month", "", "text"),
                  CreateField("Year", "card_year", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log interacted form event for address only once.
TEST_F(AutofillMetricsTest, AddressFormEventsAreSegmented) {
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);
  personal_data().ClearProfiles();

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[2]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateProfile(/*is_server=*/false);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[2]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithOnlyLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log that Profile Autofill is enabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillProfileIsEnabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager().SetAutofillProfileEnabled(true);
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.PageLoad",
                                      true, 1);
}

// Test that we log that Profile Autofill is disabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillProfileIsDisabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager().SetAutofillProfileEnabled(false);
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.PageLoad",
                                      false, 1);
}

// Test that we log that CreditCard Autofill is enabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillCreditCardIsEnabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager().SetAutofillCreditCardEnabled(true);
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.PageLoad",
                                      true, 1);
}

// Test that we log that CreditCard Autofill is disabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillCreditCardIsDisabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager().SetAutofillCreditCardEnabled(false);
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.PageLoad",
                                      false, 1);
}

// Test that we log the days since last use of a credit card when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_CreditCard) {
  base::HistogramTester histogram_tester;
  CreditCard credit_card;
  credit_card.set_use_date(AutofillClock::Now() - base::Days(21));
  credit_card.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.CreditCard", 21,
                                     1);
}

// Test that we log the days since last use of a profile when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_Profile) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.set_use_date(AutofillClock::Now() - base::Days(13));
  profile.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.Profile", 13,
                                     1);
}

// Test that we log the verification status of name tokens.
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfNameTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"First Last",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"First",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Last",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Last",
                                           VerificationStatus::kParsed);

  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(profile);

  std::string base_histo =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histo + "Full",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histo + "First",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histo + "Last",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histo + "SecondLast",
                                      VerificationStatus::kParsed, 1);

  histogram_tester.ExpectTotalCount(base_histo + "Middle", 0);
  histogram_tester.ExpectTotalCount(base_histo + "FirstLast", 0);

  histogram_tester.ExpectTotalCount(base_histo + "Any", 4);
  histogram_tester.ExpectBucketCount(base_histo + "Any",
                                     VerificationStatus::kObserved, 1);
  histogram_tester.ExpectBucketCount(base_histo + "Any",
                                     VerificationStatus::kParsed, 3);
}

// Test that we log the verification status of address tokens..
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfAddressTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"123 StreetName",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"123",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"StreetName", VerificationStatus::kObserved);

  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(profile);

  std::string base_histo =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histo + "StreetAddress",
                                      VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectUniqueSample(base_histo + "StreetName",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histo + "HouseNumber",
                                      VerificationStatus::kObserved, 1);

  histogram_tester.ExpectTotalCount(base_histo + "FloorNumber", 0);
  histogram_tester.ExpectTotalCount(base_histo + "ApartmentNumber", 0);
  histogram_tester.ExpectTotalCount(base_histo + "Premise", 0);
  histogram_tester.ExpectTotalCount(base_histo + "SubPremise", 0);

  histogram_tester.ExpectTotalCount(base_histo + "Any", 3);
  histogram_tester.ExpectBucketCount(base_histo + "Any",
                                     VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectBucketCount(base_histo + "Any",
                                     VerificationStatus::kObserved, 2);
}

// Verify that we correctly log the submitted form's state.
TEST_F(AutofillMetricsTest, AutofillFormSubmittedState) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text"),
                              CreateField("Phone", "phone", "", "text"),
                              CreateField("Unknown", "unknown", "", "text")});

  // Expect no notifications when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    histogram_tester.ExpectTotalCount("Autofill.FormSubmittedState", 0);

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form, /*is_for_credit_card=*/false,
        {FormType::kAddressForm, FormType::kUnknownFormType},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
  }

  ExpectedUkmMetrics expected_form_submission_ukm_metrics;
  ExpectedUkmMetrics expected_field_fill_status_ukm_metrics;

  // No data entered in the form.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Non fillable form.
  form.fields[0].value = u"Unknown Person";
  form.fields[1].value = u"unknown.person@gmail.com";

  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Fillable form.
  form.fields[0].value = u"Elvis Aaron Presley";
  form.fields[1].value = u"theking@gmail.com";
  form.fields[2].value = u"12345678901";

  // Autofilled none with no suggestions shown.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS,
        1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledNone_SuggestionsNotShown"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::
              FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});

    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Autofilled none with suggestions shown.
  autofill_manager().DidShowSuggestions(true, form, form.fields[2]);
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledNone_SuggestionsShown"));

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()},
          {UkmTextFieldDidChangeType::kHeuristicTypeName,
           PHONE_HOME_WHOLE_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}}});

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Mark one of the fields as autofilled.
  form.fields[1].is_autofilled = true;

  // Autofilled some of the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledSome"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Mark all of the fillable fields as autofilled.
  form.fields[0].is_autofilled = true;
  form.fields[2].is_autofilled = true;

  // Autofilled all the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledAll"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector(
              {FormType::kAddressForm, FormType::kUnknownFormType})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }
}

// Verify that we correctly log the submitted form's state with fields
// having |only_fill_when_focused|=true.
TEST_F(
    AutofillMetricsTest,
    AutofillFormSubmittedState_DontCountUnfilledFieldsWithOnlyFillWhenFocused) {
  FormData form =
      CreateForm({CreateField("Name", "name", "", "text"),
                  CreateField("Email", "email", "", "text"),
                  CreateField("Phone", "phone", "", "text"),
                  CreateField("Billing Phone", "billing_phone", "", "text")});

  // Verify if the form is otherwise filled with a field having
  // |only_fill_when_focused|=true, we consider the form is all filled.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    SeeForm(form);
    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form, /*is_for_credit_card=*/false,
        {FormType::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
    histogram_tester.ExpectTotalCount("Autofill.FormSubmittedState", 0);

    form.fields[0].value = u"Elvis Aaron Presley";
    form.fields[0].is_autofilled = true;
    form.fields[1].value = u"theking@gmail.com";
    form.fields[1].is_autofilled = true;
    form.fields[2].value = u"12345678901";
    form.fields[2].is_autofilled = true;

    SubmitForm(form);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledAll"));

    ExpectedUkmMetrics expected_form_submission_ukm_metrics;
    ExpectedUkmMetrics expected_field_fill_status_ukm_metrics;

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFormSubmittedType::kIsForCreditCardName, false},
         {UkmFormSubmittedType::kHasUpiVpaFieldName, false},
         {UkmFormSubmittedType::kFormTypesName,
          AutofillMetrics::FormTypesToBitVector({FormType::kAddressForm})},
         {UkmFormSubmittedType::kFormSignatureName,
          Collapse(CalculateFormSignature(form)).value()},
         {UkmFormSubmittedType::kFormElementUserModificationsName, 0},
         {UkmFormSubmittedType::kAutofillFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }
}

TEST_F(AutofillMetricsTest, LogUserHappinessMetric_PasswordForm) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL, FieldTypeGroup::kPasswordField,
        security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
        /*profile_form_bitmask=*/0);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Password",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Unknown", 0);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL, FieldTypeGroup::kUsernameField,
        security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
        /*profile_form_bitmask=*/0);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Password",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Unknown", 0);
  }
}

TEST_F(AutofillMetricsTest, LogUserHappinessMetric_UnknownForm) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL, FieldTypeGroup::kNoGroup,
        security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
        /*profile_form_bitmask=*/0);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Unknown",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL, FieldTypeGroup::kTransaction,
        security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
        /*profile_form_bitmask=*/0);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Unknown",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  }
}

// Verify that nothing is logging in happiness metrics if no fields in form.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_EmptyForm) {
  FormData form = CreateEmptyForm();

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_CreditCardForm) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Card Number", "card_number", "", "text"),
                  CreateField("Expiration", "cc_exp", "", "text"),
                  CreateField("Verification", "verification", "", "text")});

  // Expect a notification when the form is first seen.
  {
    SCOPED_TRACE("First seen");
    base::HistogramTester histogram_tester;
    SeeForm(form);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    SCOPED_TRACE("Initial typing");
    base::HistogramTester histogram_tester;
    SimulateUserChangedTextField(form, form.fields.front());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  autofill_manager().Reset();
  SeeForm(form);

  // Simulate suggestions shown twice with separate popups.
  {
    SCOPED_TRACE("Separate pop-ups");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields[0]);
    autofill_manager().DidShowSuggestions(true, form, form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 2),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.CreditCard"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 2),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  autofill_manager().Reset();
  SeeForm(form);

  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    SCOPED_TRACE("Multiple keystrokes");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields[0]);
    autofill_manager().DidShowSuggestions(false, form, form.fields[0]);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 1),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.CreditCard"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 1),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  // Simulate suggestions shown for a different field.
  {
    SCOPED_TRACE("Different field");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  // Simulate invoking autofill.
  {
    SCOPED_TRACE("Invoke autofill");
    base::HistogramTester histogram_tester;
    FillAutofillFormData(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::USER_DID_AUTOFILL, 1),
                       Bucket(AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.CreditCard"),
        BucketsInclude(Bucket(AutofillMetrics::USER_DID_AUTOFILL, 1),
                       Bucket(AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1)));
  }

  // Simulate editing an autofilled field.
  {
    SCOPED_TRACE("Edit autofilled field");
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, form.fields.front(),
        MakeFrontendId({.credit_card_id = kTestLocalCardId}));
    SimulateUserChangedTextField(form, form.fields.front());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    SimulateUserChangedTextField(form, form.fields.front());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1),
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.CreditCard"),
        BucketsInclude(
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1),
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1)));
  }

  // Simulate invoking autofill again.
  {
    SCOPED_TRACE("Invoke autofill again");
    base::HistogramTester histogram_tester;
    FillAutofillFormData(form);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    SCOPED_TRACE("Edit another autofilled field");
    base::HistogramTester histogram_tester;
    SimulateUserChangedTextField(form, form.fields[1]);
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_AddressForm) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text"),
                              CreateField("Phone", "phone", "", "text")});

  {
    SCOPED_TRACE("Expect a notification when the form is first seen.");
    base::HistogramTester histogram_tester;
    SeeForm(form);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  {
    SCOPED_TRACE("Simulate typing.");
    base::HistogramTester histogram_tester;
    SimulateUserChangedTextFieldTo(form, form.fields.front(), u"new value");
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  {
    SCOPED_TRACE("Simulate suggestions shown twice with separate popups.");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields.back());
    autofill_manager().DidShowSuggestions(true, form, form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 2),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.Address"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 2),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  autofill_manager().Reset();
  SeeForm(form);
  {
    SCOPED_TRACE(
        "Simulate suggestions shown twice for a single edit "
        "(i.e. multiple keystrokes in a single field).");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields.back());
    autofill_manager().DidShowSuggestions(false, form, form.fields.back());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 1),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.Address"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 1),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  {
    SCOPED_TRACE("Simulate suggestions shown for a different field.");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  {
    SCOPED_TRACE("Simulate invoking autofill.");
    base::HistogramTester histogram_tester;
    FillAutofillFormData(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(Bucket(AutofillMetrics::USER_DID_AUTOFILL, 1),
                       Bucket(AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.Address"),
        BucketsInclude(Bucket(AutofillMetrics::USER_DID_AUTOFILL, 1),
                       Bucket(AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1)));
  }

  {
    SCOPED_TRACE("Simulate editing an autofilled field.");
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    SimulateUserChangedTextFieldTo(form, form.fields.front(), u"to some value");
    // Simulate a second keystroke; make sure we don't log the metric twice.
    SimulateUserChangedTextFieldTo(form, form.fields.front(),
                                   u"to some other value");
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness"),
        BucketsInclude(
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1),
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.UserHappiness.Address"),
        BucketsInclude(
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1),
            Bucket(AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1)));
  }

  {
    SCOPED_TRACE("Simulate invoking autofill again.");
    base::HistogramTester histogram_tester;
    FillAutofillFormData(form);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  {
    SCOPED_TRACE("Simulate editing another autofilled field.");
    base::HistogramTester histogram_tester;
    SimulateUserChangedTextFieldTo(form, form.fields[1], u"some value");
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
  }

  autofill_manager().Reset();

  VerifyUkm(test_ukm_recorder_, form, UkmInteractedWithFormType::kEntryName,
            {{{UkmInteractedWithFormType::kIsForCreditCardName, false},
              {UkmInteractedWithFormType::kLocalRecordTypeCountName, 0},
              {UkmInteractedWithFormType::kServerRecordTypeCountName, 0}}});
  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[1])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
      {{{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmSuggestionFilledType::kIsForCreditCardName, false},
        {UkmSuggestionFilledType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmSuggestionFilledType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmSuggestionFilledType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionFilledType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
  VerifyUkm(
      test_ukm_recorder_, form, UkmTextFieldDidChangeType::kEntryName,
      {{{UkmTextFieldDidChangeType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kName)},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, NAME_FULL},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HtmlFieldMode::kNone},
        {UkmTextFieldDidChangeType::kIsAutofilledName, false},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmTextFieldDidChangeType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmTextFieldDidChangeType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kName)},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, NAME_FULL},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HtmlFieldMode::kNone},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, false},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmTextFieldDidChangeType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmTextFieldDidChangeType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kEmail)},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HtmlFieldMode::kNone},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[1])).value()},
        {UkmTextFieldDidChangeType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
}

// Verify that we correctly log metrics tracking the duration of form fill.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  FormData empty_form = CreateForm({CreateField("Name", "name", "", "text"),
                                    CreateField("Email", "email", "", "text"),
                                    CreateField("Phone", "phone", "", "text")});

  FormData filled_form = empty_form;
  filled_form.fields[0].value = u"Elvis Aaron Presley";
  filled_form.fields[1].value = u"theking@gmail.com";
  filled_form.fields[2].value = u"12345678901";

  // Fill additional form.
  FormData second_form = empty_form;
  second_form.host_frame = test::MakeLocalFrameToken();
  second_form.unique_renderer_id = test::MakeFormRendererId();
  second_form.fields.push_back(
      CreateField("Second Phone", "second_phone", "", "text"));

  // Fill the field values for form submission.
  second_form.fields[0].value = u"Elvis Aaron Presley";
  second_form.fields[1].value = u"theking@gmail.com";
  second_form.fields[2].value = u"12345678901";
  second_form.fields[3].value = u"51512345678";

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    SCOPED_TRACE("Test 1 - no interaction, fields are prefilled");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(filled_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager().Reset();
  }

  // Expect metric to be logged if the user manually edited a form field.
  {
    SCOPED_TRACE("Test 2 - all fields are filled by the user");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();

    FormData user_filled_form = filled_form;
    SimulateUserChangedTextField(user_filled_form,
                                 user_filled_form.fields.front(),
                                 parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(filled_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 14, 1);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager().Reset();
  }

  // Expect metric to be logged if the user autofilled the form.
  {
    SCOPED_TRACE("Test 3 - all fields are autofilled");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();

    FormData autofilled_form = test::AsAutofilled(filled_form);
    FillAutofillFormData(autofilled_form, parse_time + base::Microseconds(5));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(autofilled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager().Reset();
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    SCOPED_TRACE(
        "Test 4 - mixed case: some fields are autofille, some fields are "
        "edited.");
    base::HistogramTester histogram_tester;

    SeeForm(empty_form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();

    FormData mixed_filled_form = test::AsAutofilled(filled_form);
    FillAutofillFormData(mixed_filled_form, parse_time + base::Microseconds(5));
    SimulateUserChangedTextField(mixed_filled_form,
                                 mixed_filled_form.fields.front(),
                                 parse_time + base::Microseconds(3));

    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(mixed_filled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager().Reset();
  }

  // Make sure that loading another form doesn't affect metrics from the first
  // form.
  {
    SCOPED_TRACE("Test 5 - load a second form before submitting the first");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();

    SeeForm(test::WithoutValues(second_form));

    FormData mixed_filled_form = test::AsAutofilled(filled_form);
    FillAutofillFormData(mixed_filled_form, parse_time + base::Microseconds(5));
    SimulateUserChangedTextField(mixed_filled_form,
                                 mixed_filled_form.fields.front(),
                                 parse_time + base::Microseconds(3));

    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(mixed_filled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager().Reset();
  }

  // Make sure that submitting a form that was loaded later will report the
  // later loading time.
  {
    SCOPED_TRACE("Test 6 - submit the second seen form first");
    base::HistogramTester histogram_tester;
    SeeForm(test::WithoutValues(empty_form));
    SeeForm(test::WithoutValues(second_form));
    base::TimeTicks parse_time{};
    for (const auto& kv : autofill_manager().form_structures()) {
      if (kv.second->form_parsed_timestamp() > parse_time)
        parse_time = kv.second->form_parsed_timestamp();
    }

    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(second_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager().Reset();
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_CreditCardForm) {
  // Should log time duration with autofill for credit card form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard", 0);
  }

  // Should log time duration without autofill for credit card form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard", 0);
  }

  // Should not log time duration for credit card form if credit card form is
  // not detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_AddressForm) {
  // Should log time duration with autofill for address form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address", 0);
  }

  // Should log time duration without autofill for address form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address", 0);
  }

  // Should not log time duration for address form if address form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_PasswordForm) {
  // Should log time duration with autofill for password form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kPasswordForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password", 0);
  }

  // Should log time duration without autofill for password form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kPasswordForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password", 0);
  }

  // Should not log time duration for password form if password form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_UnknownForm) {
  // Should log time duration with autofill for unknown form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown", 0);
  }

  // Should log time duration without autofill for unknown form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown", 0);
  }

  // Should not log time duration for unknown form if unknown form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_MultipleForms) {
  // Should log time duration with autofill for all forms.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm, FormType::kAddressForm,
         FormType::kPasswordForm, FormType::kUnknownFormType},
        /*used_autofill=*/true, base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown",
        base::Milliseconds(2000), 1);
  }

  // Should log time duration without autofill for all forms.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm, FormType::kAddressForm,
         FormType::kPasswordForm, FormType::kUnknownFormType},
        /*used_autofill=*/false, base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown",
        base::Milliseconds(2000), 1);
  }
}

// Verify that we correctly log metrics for profile action on form submission.
TEST_F(AutofillMetricsTest, ProfileActionOnFormSubmitted) {
  base::HistogramTester histogram_tester;

  // Create the form's fields.
  FormData form =
      CreateForm({CreateField("Name", "name", "", "text"),
                  CreateField("Email", "email", "", "text"),
                  CreateField("Phone", "phone", "", "text"),
                  CreateField("Address", "address", "", "text"),
                  CreateField("City", "city", "", "text"),
                  CreateField("Country", "country", "", "text"),
                  CreateField("State", "state", "", "text"),
                  CreateField("Zip", "zip", "", "text"),
                  CreateField("Organization", "organization", "", "text")});

  FormData second_form = form;
  FormData third_form = form;
  FormData fourth_form = form;

  // Fill the field values for the first form submission.
  form.fields[0].value = u"Albert Canuck";
  form.fields[1].value = u"can@gmail.com";
  form.fields[2].value = u"12345678901";
  form.fields[3].value = u"1234 McGill street.";
  form.fields[4].value = u"Montreal";
  form.fields[5].value = u"Canada";
  form.fields[6].value = u"Quebec";
  form.fields[7].value = u"A1A 1A1";

  // Fill the field values for the second form submission (same as first form).
  second_form.fields = form.fields;

  // Fill the field values for the third form submission.
  third_form.fields[0].value = u"Jean-Paul Canuck";
  third_form.fields[1].value = u"can2@gmail.com";
  third_form.fields[2].value = u"";
  third_form.fields[3].value = u"1234 McGill street.";
  third_form.fields[4].value = u"Montreal";
  third_form.fields[5].value = u"Canada";
  third_form.fields[6].value = u"Quebec";
  third_form.fields[7].value = u"A1A 1A1";

  // Fill the field values for the fourth form submission (same as third form
  // plus phone info).
  fourth_form.fields = third_form.fields;
  fourth_form.fields[2].value = u"12345678901";

  // Expect to log NEW_PROFILE_CREATED for the metric since a new profile is
  // submitted.
  SeeForm(test::WithoutValues(form));
  SubmitForm(form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 0),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log EXISTING_PROFILE_USED for the metric since the same profile
  // is submitted.
  SeeForm(test::WithoutValues(second_form));
  SubmitForm(second_form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log NEW_PROFILE_CREATED for the metric since a new profile is
  // submitted.
  SeeForm(test::WithoutValues(third_form));
  SubmitForm(third_form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 2),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log EXISTING_PROFILE_UPDATED for the metric since the profile was
  // updated.
  SeeForm(test::WithoutValues(fourth_form));
  SubmitForm(fourth_form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 2),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 1)));
}

// Test class that shares setup code for testing ParseQueryResponse.
class AutofillMetricsParseQueryResponseTest : public testing::Test {
 public:
  void SetUp() override {
    FormData form;
    form.host_frame = test::MakeLocalFrameToken();
    form.unique_renderer_id = test::MakeFormRendererId();
    form.url = GURL("http://foo.com");
    form.main_frame_origin = url::Origin::Create(GURL("http://foo_root.com"));
    FormFieldData field;
    field.form_control_type = "text";

    field.label = u"fullname";
    field.name = u"fullname";
    form.fields.push_back(field);

    field.label = u"address";
    field.name = u"address";
    form.fields.push_back(field);

    // Checkable fields should be ignored in parsing.
    FormFieldData checkable_field;
    checkable_field.label = u"radio_button";
    checkable_field.form_control_type = "radio";
    checkable_field.check_status =
        FormFieldData::CheckStatus::kCheckableButUnchecked;
    form.fields.push_back(checkable_field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());

    field.label = u"email";
    field.name = u"email";
    form.fields.push_back(field);

    field.label = u"password";
    field.name = u"password";
    field.form_control_type = "password";
    form.fields.push_back(field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());
  }

 protected:
  test::AutofillEnvironment autofill_environment_;
  std::vector<std::unique_ptr<FormStructure>> owned_forms_;
  std::vector<FormStructure*> forms_;
};

TEST_F(AutofillMetricsParseQueryResponseTest, ServerHasData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), ADDRESS_HOME_LINE1,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), EMAIL_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), NO_SERVER_DATA,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  FormStructure::ParseApiQueryResponse(response_string, forms_,
                                       test::GetEncodedSignatures(forms_),
                                       nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// If the server returns NO_SERVER_DATA for one of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, OneFormNoServerData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), NO_SERVER_DATA,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), EMAIL_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), NO_SERVER_DATA,
                           form_suggestion);
  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  FormStructure::ParseApiQueryResponse(response_string, forms_,
                                       test::GetEncodedSignatures(forms_),
                                       nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 1), Bucket(true, 1)));
}

// If the server returns NO_SERVER_DATA for both of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, AllFormsNoServerData) {
  AutofillQueryResponse response;
  for (int form_idx = 0; form_idx < 2; ++form_idx) {
    auto* form_suggestion = response.add_form_suggestions();
    for (int field_idx = 0; field_idx < 2; ++field_idx) {
      AddFieldPredictionToForm(*forms_[form_idx]->field(field_idx),
                               NO_SERVER_DATA, form_suggestion);
    }
  }

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  FormStructure::ParseApiQueryResponse(response_string, forms_,
                                       test::GetEncodedSignatures(forms_),
                                       nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 2)));
}

// If the server returns NO_SERVER_DATA for only some of the fields, expect the
// UMA metric to say there is data.
TEST_F(AutofillMetricsParseQueryResponseTest, PartialNoServerData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), PHONE_HOME_NUMBER,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), PHONE_HOME_CITY_CODE,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  FormStructure::ParseApiQueryResponse(response_string, forms_,
                                       test::GetEncodedSignatures(forms_),
                                       nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// Tests that credit card form submissions are logged specially when the form is
// on a non-secure page.
TEST_F(AutofillMetricsTest, NonsecureCreditCardForm) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Name on card", "cc-name", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text"),
                  CreateField("Month", "cardmonth", "", "text"),
                  CreateField("Expiration date", "expdate", "", "text")});
  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  // Non-https origin.
  GURL frame_origin("http://example_root.com/form.html");
  form.main_frame_origin = url::Origin::Create(frame_origin);
  autofill_client_->set_form_origin(frame_origin);

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    SubmitForm(form);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }
}

// Tests that credit card form submissions are *not* logged specially when the
// form is *not* on a non-secure page.
TEST_F(AutofillMetricsTest,
       NonsecureCreditCardFormMetricsNotRecordedOnSecurePage) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateField("Name on card", "cc-name", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text"),
                  CreateField("Expiration date", "expdate", "", "text")});

  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    SubmitForm(form);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }
}

// Tests that logging CardUploadDecision UKM works as expected.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric) {
  GURL url("https://www.google.com");
  int upload_decision = 1;
  autofill_client_->set_form_origin(url);

  autofill_metrics::LogCardUploadDecisionsUkm(
      test_ukm_recorder_, autofill_client_->GetUkmSourceId(), url,
      upload_decision);
  auto entries = test_ukm_recorder_->GetEntriesByName(
      UkmCardUploadDecisionType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
    EXPECT_EQ(1u, entry->metrics.size());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, UkmCardUploadDecisionType::kUploadDecisionName, upload_decision);
  }
}

// Tests that logging DeveloperEngagement UKM works as expected.
TEST_F(AutofillMetricsTest, RecordDeveloperEngagementMetric) {
  GURL url("https://www.google.com");
  int form_structure_metric = 1;
  FormSignature form_signature(100);
  autofill_client_->set_form_origin(url);

  AutofillMetrics::LogDeveloperEngagementUkm(
      test_ukm_recorder_, autofill_client_->GetUkmSourceId(), url, true,
      {FormType::kCreditCardForm}, form_structure_metric, form_signature);
  auto entries = test_ukm_recorder_->GetEntriesByName(
      UkmDeveloperEngagementType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
    EXPECT_EQ(4u, entry->metrics.size());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
        form_structure_metric);
    test_ukm_recorder_->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kIsForCreditCardName, true);
    test_ukm_recorder_->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormTypesName,
        AutofillMetrics::FormTypesToBitVector({FormType::kCreditCardForm}));
    test_ukm_recorder_->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormSignatureName,
        form_signature.value());
  }
}

// Tests that no UKM is logged when the URL is not valid.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_InvalidUrl) {
  GURL url("");
  test_ukm_recorder_->Purge();
  autofill_metrics::LogCardUploadDecisionsUkm(test_ukm_recorder_, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder_->sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
}

// Tests that no UKM is logged when the ukm service is null.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_NoUkmService) {
  GURL url("https://www.google.com");
  test_ukm_recorder_->Purge();
  autofill_metrics::LogCardUploadDecisionsUkm(nullptr, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder_->sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
}

// Test the ukm recorded when Suggestion is shown.
//
// Flaky on all platforms. TODO(crbug.com/876897): Fix it.
TEST_F(AutofillMetricsTest, DISABLED_AutofillSuggestionShownTest) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  FormData form =
      CreateForm({CreateField("Name on card", "cc-name", "", "text"),
                  CreateField("Credit card", "cardnum", "", "text"),
                  CreateField("Month", "card_month", "", "text")});

  FormFieldData field;
  std::vector<ServerFieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH};
  autofill_manager().AddSeenForm(form, field_types);

  // Simulate and Autofill query on credit card name field.
  autofill_manager().DidShowSuggestions(/*is_new_popup=*/true, form,
                                        form.fields[0]);
  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionsShownType::kMillisecondsSinceFormParsedName, 0},
        {UkmSuggestionsShownType::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmSuggestionsShownType::kServerTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
}

TEST_F(AutofillMetricsTest, DynamicFormMetrics) {
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  // Simulate seeing.
  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types);

  // Simulate checking whether to fill a dynamic form before the form was filled
  // initially.
  FormStructure form_structure(form);
  autofill_manager().ShouldTriggerRefillForTest(form_structure);
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.Address", 0);

  // Simulate filling the form.
  FillTestProfile(form);

  // Simulate checking whether to fill a dynamic form after the form was filled
  // initially.
  autofill_manager().ShouldTriggerRefillForTest(form_structure);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, 1),
                     Bucket(FORM_EVENT_DID_DYNAMIC_REFILL, 0),
                     Bucket(FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL, 0)));

  // Trigger a refill, the refill metric should be updated.
  autofill_manager().TriggerRefillForTest(form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, 1),
                     Bucket(FORM_EVENT_DID_DYNAMIC_REFILL, 1),
                     Bucket(FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL, 0)));

  // Trigger a check to see whether a refill should happen. The
  autofill_manager().ShouldTriggerRefillForTest(form_structure);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, 2),
                     Bucket(FORM_EVENT_DID_DYNAMIC_REFILL, 1),
                     Bucket(FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL, 1)));
}

// Tests that the LogUserHappinessBySecurityLevel are recorded correctly.
TEST_F(AutofillMetricsTest, LogUserHappinessBySecurityLevel) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessBySecurityLevel(
        AutofillMetrics::USER_DID_AUTOFILL, FormType::kCreditCardForm,
        security_state::SecurityLevel::SECURE);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.CreditCard.SECURE",
        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessBySecurityLevel(
        AutofillMetrics::SUGGESTIONS_SHOWN, FormType::kAddressForm,
        security_state::SecurityLevel::DANGEROUS);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address.DANGEROUS",
        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessBySecurityLevel(
        AutofillMetrics::FIELD_WAS_AUTOFILLED, FormType::kPasswordForm,
        security_state::SecurityLevel::WARNING);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Password.WARNING",
        AutofillMetrics::FIELD_WAS_AUTOFILLED, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessBySecurityLevel(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE, FormType::kUnknownFormType,
        security_state::SecurityLevel::SECURE);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Unknown.SECURE",
                                       AutofillMetrics::USER_DID_AUTOFILL_ONCE,
                                       1);
  }

  {
    // No metric should be recorded if the security level is
    // SECURITY_LEVEL_COUNT.
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessBySecurityLevel(
        AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME,
        FormType::kCreditCardForm,
        security_state::SecurityLevel::SECURITY_LEVEL_COUNT);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard.OTHER",
                                      0);
  }
}

// Verify that we correctly log LogUserHappinessBySecurityLevel dealing form the
// form event metrics.
TEST_F(AutofillMetricsTest, LogUserHappinessBySecurityLevel_FromFormEvents) {
  FormData form = CreateForm({CreateField("Name", "name", "", "text"),
                              CreateField("Email", "email", "", "text"),
                              CreateField("Phone", "phone", "", "text")});

  // Simulate seeing the form.
  {
    base::HistogramTester histogram_tester;
    autofill_client_->set_security_level(
        security_state::SecurityLevel::DANGEROUS);
    SeeForm(form);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address.DANGEROUS",
        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate suggestions shown twice with separate popups.
  {
    base::HistogramTester histogram_tester;
    autofill_client_->set_security_level(
        security_state::SecurityLevel::WARNING);
    autofill_manager().DidShowSuggestions(true, form, form.fields.front());
    autofill_manager().DidShowSuggestions(true, form, form.fields.front());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.UserHappiness.Address.WARNING"),
        BucketsInclude(Bucket(AutofillMetrics::SUGGESTIONS_SHOWN, 2),
                       Bucket(AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1)));
  }
}

TEST_F(AutofillMetricsTest, LogUserHappinessByProfileFormType_AddressOnly) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                  ADDRESS_HOME_DEPENDENT_LOCALITY}));

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressOnly",
      AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusContact"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest, LogUserHappinessByProfileFormType_ContactOnly) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}));

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.ContactOnly",
      AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusContact"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest,
       LogUserHappinessByProfileFormType_AddressPlusPhone) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups(
          {NAME_FULL, ADDRESS_HOME_ZIP, PHONE_HOME_CITY_AND_NUMBER}));

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusPhone",
      AutofillMetrics::USER_DID_TYPE, 1);

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusContact",
      AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest,
       LogUserHappinessByProfileFormType_AddressPlusEmail) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({NAME_FULL, ADDRESS_HOME_ZIP, EMAIL_ADDRESS}));

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusEmail",
      AutofillMetrics::USER_DID_TYPE, 1);

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusContact",
      AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest,
       LogUserHappinessByProfileFormType_AddressPlusEmailPlusPhone) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({NAME_FULL, ADDRESS_HOME_ZIP, EMAIL_ADDRESS,
                                  PHONE_HOME_WHOLE_NUMBER}));

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone",
      AutofillMetrics::USER_DID_TYPE, 1);

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness.Address.AddressPlusContact",
      AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.UserHappiness.CreditCard"),
                HasSubstr("Autofill.UserHappiness.Password"),
                HasSubstr("Autofill.UserHappiness.Unknown"),
                HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
                HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
                HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
                HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
                HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
                HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest, LogUserHappinessByProfileFormType_Other) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({NAME_FIRST, NAME_MIDDLE, NAME_LAST}));

  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address.Other",
                                     AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusContact"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr(
              "Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"))));
}

TEST_F(AutofillMetricsTest, LogUserHappinessByProfileFormType_PhoneOnly) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_TYPE, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({PHONE_HOME_NUMBER}));

  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address.PhoneOnly",
                                     AutofillMetrics::USER_DID_TYPE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusContact"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail"),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.Other"))));
}

TEST_F(AutofillMetricsTest,
       LogUserHappinessByProfileFormType_FormsLoadedNotLogged) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::FORMS_LOADED, {FormType::kAddressForm},
      security_state::SecurityLevel::NONE,
      data_util::DetermineGroups({NAME_FIRST, NAME_MIDDLE, NAME_LAST}));

  // Logging is not done in the profile form histograms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.UserHappiness.CreditCard"),
          HasSubstr("Autofill.UserHappiness.Password"),
          HasSubstr("Autofill.UserHappiness.Unknown"),
          HasSubstr("Autofill.UserHappiness.Address.Other"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusContact"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusPhone"),
          HasSubstr("Autofill.UserHappiness.Address.AddressPlusEmail "),
          HasSubstr("Autofill.UserHappiness.Address.ContactOnly"),
          HasSubstr("Autofill.UserHappiness.Address.AddressOnly"),
          HasSubstr("Autofill.UserHappiness.Address.PhoneOnly"),
          HasSubstr(
              "Autofill.UserHappiness.Address.AddressPlusEmailPlusPhone"))));
}

TEST_F(AutofillMetricsTest,
       LogUserHappinessByProfileFormType_NoAddressFormType) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED,
                                          {FormType::kCreditCardForm},
                                          security_state::SecurityLevel::NONE,
                                          /*profile_form_bitmask=*/0);

  // Logging is not done in the profile form histograms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness.Address"))));
}

// Verify that we don't log Autofill.WebOTP.OneTimeCode.FromAutocomplete if the
// frame has no form.
TEST_F(AutofillMetricsTest, FrameHasNoForm) {
  base::HistogramTester histogram_tester;
  autofill_driver_.reset();
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 0);
}

// Verify that we correctly log metrics if a frame has
// autocomplete="one-time-code".
TEST_F(AutofillMetricsTest, FrameHasAutocompleteOneTimeCode) {
  FormData form =
      CreateForm({CreateField("", "", "", "password", "one-time-code"),
                  CreateField("", "", "", "password")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  // Verifies that autocomplete="one-time-code" in a form is correctly recorded.
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete",
      /* has_one_time_code */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 1);
}

// Verify that we correctly log metrics if a frame does not have
// autocomplete="one-time-code".
TEST_F(AutofillMetricsTest, FrameDoesNotHaveAutocompleteOneTimeCode) {
  FormData form = CreateForm({CreateField("", "", "", "password")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete",
      /* has_one_time_code */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 1);
}

// Verify that we correctly log metrics when a phone number field does not have
// autocomplete attribute but there are at least 3 fields in the form.
TEST_F(AutofillMetricsTest, FrameHasPhoneNumberFieldWithoutAutocomplete) {
  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  FormData form =
      CreateForm({CreateField("Phone", "phone", "", "tel"),
                  CreateField("Last Name", "lastname", "", "text"),
                  CreateField("First Name", "firstname", "", "text")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a phone number field does not have
// autocomplete attribute and there are less than 3 fields in the form.
TEST_F(AutofillMetricsTest, FrameHasSinglePhoneNumberFieldWithoutAutocomplete) {
  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  FormData form = CreateForm({CreateField("Phone", "phone", "", "tel")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a phone number field has
// autocomplete attribute.
TEST_F(AutofillMetricsTest, FrameHasPhoneNumberFieldWithAutocomplete) {
  FormData form;  // Form with phone number.
  CreateSimpleForm(autofill_client_->form_origin(), form);
  form.fields = {CreateField("", "", "", "", "phone")};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a form does not have phone number
// field.
TEST_F(AutofillMetricsTest, FrameDoesNotHavePhoneNumberField) {
  FormData form = CreateForm({CreateField("", "", "", "password")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_driver_.reset();
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// ContentAutofillDriver is not visible to TestAutofillDriver on iOS.
// In addition, WebOTP will not ship on iOS.
#if !BUILDFLAG(IS_IOS)

struct WebOTPPhoneCollectionMetricsTestCase {
  std::vector<const char*> autocomplete_field;
  PhoneCollectionMetricState phone_collection_metric_state;
  bool report_autofill_web_otp_metrics = false;
};

class WebOTPPhoneCollectionMetricsTest
    : public AutofillMetricsTest,
      public ::testing::WithParamInterface<
          WebOTPPhoneCollectionMetricsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    WebOTPPhoneCollectionMetricsTest,
    WebOTPPhoneCollectionMetricsTest,
    testing::Values(
        // Verify that we correctly log PhoneCollectionMetricState::kNone.
        WebOTPPhoneCollectionMetricsTestCase{{"password"},
                                             PhoneCollectionMetricState::kNone},
        // Verify that we correctly log PhoneCollectionMetricState::kOTC.
        WebOTPPhoneCollectionMetricsTestCase{{"one-time-code"},
                                             PhoneCollectionMetricState::kOTC},
        // Verify that we correctly log PhoneCollectionMetricState::kWebOTP.
        WebOTPPhoneCollectionMetricsTestCase{
            {},
            PhoneCollectionMetricState::kWebOTP,
            true},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kWebOTPPlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"one-time-code"},
            PhoneCollectionMetricState::kWebOTPPlusOTC,
            true},
        // Verify that we correctly log PhoneCollectionMetricState::kPhone.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel"},
            PhoneCollectionMetricState::kPhone},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel", "one-time-code"},
            PhoneCollectionMetricState::kPhonePlusOTC},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusWebOTP.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel"},
            PhoneCollectionMetricState::kPhonePlusWebOTP,
            true},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel", "one-time-code"},
            PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC,
            true}));

TEST_P(WebOTPPhoneCollectionMetricsTest,
       TestWebOTPPhoneCollectionMetricsState) {
  auto test_case = GetParam();

  if (!test_case.autocomplete_field.empty()) {
    FormData form;
    CreateSimpleForm(autofill_client_->form_origin(), form);
    for (const char* autocomplete : test_case.autocomplete_field)
      form.fields.push_back(CreateField("", "", "", "", autocomplete));

    SeeForm(form);
  }

  base::HistogramTester histogram_tester;
  autofill_manager().ReportAutofillWebOTPMetrics(
      test_case.report_autofill_web_otp_metrics);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.WebOTP.PhonePlusWebOTPPlusOTC"),
      BucketsAre(Bucket(test_case.phone_collection_metric_state, 1)));
}

// Verify that proper PhoneCollectionMetricsState is logged to UKM.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateLoggedToUKM) {
  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::WebOTPImpact::kEntryName);
  ASSERT_TRUE(entries.empty());

  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  // Document collects phone number
  form.fields.push_back(CreateField("", "", "", "", "tel"));
  // Document uses OntTimeCode
  form.fields.push_back(CreateField("", "", "", "", "one-time-code"));

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_manager().ReportAutofillWebOTPMetrics(true);

  entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::WebOTPImpact::kEntryName);
  ASSERT_EQ(1u, entries.size());

  const int64_t* metric =
      test_ukm_recorder_->GetEntryMetric(entries[0], "PhoneCollection");
  EXPECT_EQ(*metric, static_cast<int>(
                         PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC));
}

TEST_F(AutofillMetricsTest, AutocompleteOneTimeCodeFormFilledDuration) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  FormData form =
      CreateForm({CreateField("", "", "", "password", "one-time-code")});
  form.fields[0].value = u"123456";

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(form);

    histogram_tester.ExpectTotalCount(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 16, 1);
    autofill_manager().Reset();
  }

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    FillAutofillFormData(form, parse_time + base::Microseconds(5));
    SimulateUserChangedTextField(form, form.fields.front(),
                                 parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    SubmitForm(form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", 14, 1);
    autofill_manager().Reset();
  }
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(AutofillMetricsTest, LogAutocompleteSuggestionAcceptedIndex_WithIndex) {
  base::HistogramTester histogram_tester;
  const int test_index = 3;
  AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(test_index);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", test_index,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTION_SELECTED,
      /*expected_count=*/1);
}

TEST_F(AutofillMetricsTest, LogAutocompleteSuggestionAcceptedIndex_IndexCap) {
  base::HistogramTester histogram_tester;
  const int test_index = 9000;
  AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(test_index);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", kMaxBucketsCount,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTION_SELECTED,
      /*expected_count=*/1);
}

TEST_F(AutofillMetricsTest, OnAutocompleteSuggestionsShown) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::OnAutocompleteSuggestionsShown();
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      /*expected_count=*/1);
}

// Verify that we correctly log FormEvent metrics with the appropriate sync
// state.
TEST_F(AutofillMetricsTest, FormEventMetrics_BySyncState) {
  FormData form;
  FormStructure form_structure(form);
  SeeForm(form);
  autofill_manager().Reset();

  {
    base::HistogramTester histogram_tester;
    AddressFormEventLogger logger(
        /*is_in_any_main_frame=*/true,
        /*form_interactions_ukm_logger=*/nullptr,
        /*client=*/autofill_client_.get());
    logger.OnDidSeeFillableDynamicForm(AutofillSyncSigninState::kSignedOut,
                                       form_structure);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address.WithNoData.SignedOut",
        FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AddressFormEventLogger logger(
        /*is_in_any_main_frame=*/true,
        /*form_interactions_ukm_logger=*/nullptr,
        /*client=*/autofill_client_.get());
    logger.OnDidRefill(AutofillSyncSigninState::kSignedIn, form_structure);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address.WithNoData.SignedIn",
        FORM_EVENT_DID_DYNAMIC_REFILL, 1);
  }
}

// Verify that we correctly log the IsEnabled metrics with the appropriate sync
// state.
TEST_F(AutofillMetricsTest, LogIsAutofillEnabledAtPageLoad_BySyncState) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(/*enabled=*/true,
                                                    SyncSigninState::kSignedIn);
    histogram_tester.ExpectBucketCount("Autofill.IsEnabled.PageLoad.SignedIn",
                                       true, 1);
    // Make sure the metric without the sync state is still recorded.
    histogram_tester.ExpectBucketCount("Autofill.IsEnabled.PageLoad", true, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(
        /*enabled=*/false, SyncSigninState::kSignedOut);
    histogram_tester.ExpectBucketCount("Autofill.IsEnabled.PageLoad.SignedOut",
                                       false, 1);
    // Make sure the metric without the sync state is still recorded.
    histogram_tester.ExpectBucketCount("Autofill.IsEnabled.PageLoad", false, 1);
  }
}

TEST_F(AutofillMetricsTest, LogServerCardLinkClicked) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(
        AutofillSyncSigninState::kSignedIn);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       AutofillSyncSigninState::kSignedIn, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(
        AutofillSyncSigninState::kSignedOut);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       AutofillSyncSigninState::kSignedOut, 1);
  }
}

// Parameterized test where the parameter indicates how far we went through
// the funnel:
// 0 = Site contained form but user did not focus it (did not interact).
// 1 = User interacted with form (focused a field).
// 2 = User saw a suggestion to fill the form.
// 3 = User accepted the suggestion.
// 4 = User submitted the form.
class AutofillMetricsFunnelTest : public AutofillMetricsTest,
                                  public testing::WithParamInterface<int> {
 public:
  AutofillMetricsFunnelTest() = default;
  ~AutofillMetricsFunnelTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(AutofillMetricsTest,
                         AutofillMetricsFunnelTest,
                         testing::Values(0, 1, 2, 3, 4));

TEST_P(AutofillMetricsFunnelTest, LogFunnelMetrics) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;

  // Phase 1: Simulate events according to GetParam().
  const bool user_interacted_with_form = GetParam() >= 1;
  const bool user_saw_suggestion = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  if (!user_saw_suggestion) {
    // Remove the profile to prevent suggestion from being shown.
    personal_data().ClearProfiles();
  }

  // Simulate interacting with the form.
  if (user_interacted_with_form) {
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  }

  // Simulate seeing a suggestion.
  if (user_saw_suggestion) {
    autofill_manager().DidShowSuggestions(
        /*has_autofill_suggestions=*/true, form, form.fields[0]);
  }

  // Simulate filling the form.
  if (user_accepted_suggestion)
    FillTestProfile(form);

  if (user_submitted_form)
    SubmitForm(form);

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  histogram_tester.ExpectBucketCount("Autofill.Funnel.ParsedAsType.Address", 1,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.Funnel.ParsedAsType.CreditCard",
                                     0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Funnel.InteractionAfterParsedAsType.Address",
      user_interacted_with_form ? 1 : 0, 1);
  if (user_interacted_with_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SuggestionAfterInteraction.Address",
        user_saw_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SuggestionAfterInteraction.Address", 0);
  }

  if (user_saw_suggestion) {
    // If the suggestion was shown, we should record whether the user
    // accepted it.
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.FillAfterSuggestion.Address",
        user_accepted_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.FillAfterSuggestion.Address", 0);
  }

  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SubmissionAfterFill.Address",
        user_submitted_form ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SubmissionAfterFill.Address", 0);
  }

  // Phase 3: Validate KeyMetrics expectations.
  if (user_submitted_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAcceptance.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingCorrectness.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.Address", 1, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.Address", 0);
    VerifyUkm(
        test_ukm_recorder_, form, UkmAutofillKeyMetricsType::kEntryName,
        {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
          {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 1},
          {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 1},
          {UkmAutofillKeyMetricsType::kFillingAssistanceName, 1},
          {UkmAutofillKeyMetricsType::kAutofillFillsName, 1},
          {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
          {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
          {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingReadiness.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.Address", 0);
  }
  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FormSubmission.Autofilled.Address",
        user_submitted_form ? 1 : 0, 1);
  }
}

// Verify that no key metrics are logged in the ablation state.
TEST_F(AutofillMetricsFunnelTest, AblationState) {
  base::FieldTrialParams feature_parameters{
      {features::kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {features::kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
       "1000"}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableAblationStudy, feature_parameters);

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("City", "city", "", "text"),
                              CreateField("Street", "street", "", "text")});
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  // Simulate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);

  // Don't simulate a suggestion but simulate the user typing.
  SimulateUserChangedTextField(form, form.fields[0]);

  SubmitForm(form);

  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  const char* kMetrics[] = {"Autofill.Funnel.ParsedAsType",
                            "Autofill.Funnel.InteractionAfterParsedAsType",
                            "Autofill.Funnel.SuggestionAfterInteraction",
                            "Autofill.Funnel.FillAfterSuggestion",
                            "Autofill.Funnel.SubmissionAfterFill",
                            "Autofill.KeyMetrics.FillingReadiness",
                            "Autofill.KeyMetrics.FillingAcceptance",
                            "Autofill.KeyMetrics.FillingCorrectness",
                            "Autofill.KeyMetrics.FillingAssistance",
                            "Autofill.Autocomplete.NotOff.FillingAcceptance",
                            "Autofill.Autocomplete.Off.FillingAcceptance"};
  for (const char* metric : kMetrics) {
    histogram_tester.ExpectTotalCount(base::StrCat({metric, ".Address"}), 0);
    histogram_tester.ExpectTotalCount(base::StrCat({metric, ".CreditCard"}), 0);
  }
}

// Tests for Autofill.KeyMetrics.* metrics.
class AutofillMetricsKeyMetricsTest : public AutofillMetricsTest {
 public:
  AutofillMetricsKeyMetricsTest() = default;
  ~AutofillMetricsKeyMetricsTest() override = default;

  void SetUp() override;

  // Fillable form.
  FormData form_;
};

void AutofillMetricsKeyMetricsTest::SetUp() {
  AutofillMetricsTest::SetUp();

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Load a fillable form.
  form_ = CreateEmptyForm();
  form_.fields = {CreateField("State", "state", "", "text"),
                  CreateField("City", "city", "", "text"),
                  CreateField("Street", "street", "", "text")};
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form_, field_types, field_types);
}

// Validate Autofill.KeyMetrics.* in case the user submits the empty form.
// Empty in the sense that the user did not fill/type into the fields (not that
// it has no fields).
TEST_F(AutofillMetricsKeyMetricsTest, LogEmptyForm) {
  base::HistogramTester histogram_tester;

  // Simulate page load.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);

  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 0);

  VerifyUkm(test_ukm_recorder_, form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user has no address profile on
// file, so nothing can be filled.
TEST_F(AutofillMetricsKeyMetricsTest, LogNoProfile) {
  base::HistogramTester histogram_tester;

  // Simulate that no data is available.
  personal_data().ClearProfiles();
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);

  SimulateUserChangedTextField(form_, form_.fields[0]);
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 1, 1);

  VerifyUkm(test_ukm_recorder_, form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 2},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user does not accept a suggestion.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserDoesNotAcceptSuggestion) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown but user does not accept it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);

  SimulateUserChangedTextField(form_, form_.fields[0]);
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 1, 1);

  VerifyUkm(test_ukm_recorder_, form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 2},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user has to fix the filled data.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserFixesFilledData) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);
  FillTestProfile(form_);

  // Simulate user fixing the address.
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.Address", 1, 1);

  VerifyUkm(test_ukm_recorder_, form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 1},
              {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 1},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 1},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 1},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user fixes the filled data but
// then does not submit the form.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserFixesFilledDataButDoesNotSubmit) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);
  FillTestProfile(form_);

  // Simulate user fixing the address.
  SimulateUserChangedTextField(form_, form_.fields[1]);

  // Don't submit form.

  FormInteractionsFlowId flow_id =
      autofill_manager().address_form_interactions_flow_id_for_test();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.Address", 0, 1);

  VerifyUkm(test_ukm_recorder_, form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 0},
              {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

TEST_F(AutofillMetricsTest, GetFieldTypeUserEditStatusMetric) {
  // The id of ADDRESS_HOME_COUNTRY is 36 = 0b10'0100.
  ServerFieldType server_type = ADDRESS_HOME_COUNTRY;
  // The id of AUTOFILL_FIELD_WAS_NOT_EDITED is 1.
  AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric =
      AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
          AUTOFILLED_FIELD_WAS_NOT_EDITED;

  int expected_result = 0b10'0100'0001;
  int actual_result = GetFieldTypeUserEditStatusMetric(server_type, metric);
  EXPECT_EQ(expected_result, actual_result);
}

// Validate that correct page language values are taken from
// |AutofillClient| and logged upon form submission.
TEST_F(AutofillMetricsTest, PageLanguageMetricsExpectedCase) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);

  // Set up language state.
  translate::LanguageDetectionDetails language_detection_details;
  language_detection_details.adopted_language = "ub";
  autofill_manager().OnLanguageDetermined(language_detection_details);
  autofill_client_->GetLanguageState()->SetSourceLanguage("ub");
  autofill_client_->GetLanguageState()->SetCurrentLanguage("ub");
  int language_code = 'u' * 256 + 'b';

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesUsingTranslatedPageLanguage", language_code, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesWasPageTranslated", false, 1);
}

// Validate that invalid language codes (with disallowed symbols in this case)
// get logged as invalid.
TEST_F(AutofillMetricsTest, PageLanguageMetricsInvalidLanguage) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);

  // Set up language state.
  translate::LanguageDetectionDetails language_detection_details;
  language_detection_details.adopted_language = "en";
  autofill_manager().OnLanguageDetermined(language_detection_details);
  autofill_client_->GetLanguageState()->SetSourceLanguage("en");
  autofill_client_->GetLanguageState()->SetCurrentLanguage("other");

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesUsingTranslatedPageLanguage", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesWasPageTranslated", true, 1);
}

// Tests the following 4 cases when |kAutofillPreventOverridingPrefilledValues|
// is enabled:
// 1. The field is not autofilled since it has an initial value but the value
//    is edited before the form submission and is same as the value that was
//    to be autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2|
//    should emit true for this case.
// 2. The field is not autofilled since it has an initial value but the value
//    is edited before the form submission and is different than the value that
//    was to be autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2|
//    should emit false for this case.
// 3. The field had an initial value that was similar to the value to be
//    autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2|
//    should not record anything in this case.
// 4. Selection fields are always overridden by Autofill.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2|
//    should not record anything in this case.
TEST_F(AutofillMetricsTest,
       IsValueNotAutofilledOverExistingValueSameAsSubmittedValue) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillPreventOverridingPrefilledValues);
  RecreateProfile(false);

  FormData form = test::GetFormData(
      {.description_for_logging =
           "IsValueNotAutofilledOverExistingValueSameAsSubmittedValue",
       .fields = {
           {.role = NAME_FULL},
           {.role = ADDRESS_HOME_CITY,
            .value = u"Sacremento",
            .properties_mask = FieldPropertiesFlags::kUserTyped},  // Case #1
           {.role = ADDRESS_HOME_STATE,
            .value = u"CA",
            .form_control_type = "select-one",
            .select_options = {{u"TN", u"Tennesse"},
                               {u"CA", u"California"},
                               {u"WA", u"Washington DC"}}},  // Case #4
           {.role = ADDRESS_HOME_ZIP,
            .value = u"00000",
            .properties_mask = FieldPropertiesFlags::kUserTyped},  // Case #2
           {.role = PHONE_HOME_WHOLE_NUMBER,
            .value = u"12345678901"},  // Case #3
           {.role = ADDRESS_HOME_COUNTRY}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,        ADDRESS_HOME_CITY,       ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP, PHONE_HOME_WHOLE_NUMBER, ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types, field_types,
                                 /*preserve_values_in_form_structure=*/true);

  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

  FillTestProfile(form);

  // Case #1: Change submitted value to expected autofilled value for the field.
  // The histogram should emit true for this.
  SimulateUserChangedTextFieldTo(form, form.fields[1], u"Memphis");

  // Case #2: Change submitted value such that it different than expected
  // autofilled value for the field. The histogram should emit false for this.
  SimulateUserChangedTextFieldTo(form, form.fields[3], u"00001");

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill."
                  "IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2"),
              BucketsInclude(Bucket(true, 1), Bucket(false, 1)));
}

TEST_F(AutofillMetricsTest, FormInteractionsAreCounted) {
  // GIVEN
  FormData form = test::GetFormData({.fields = {{.role = NAME_FULL}}});
  CreateSimpleForm(autofill_client_->form_origin(), form);

  std::vector<ServerFieldType> field_types = {NAME_FULL};
  autofill_manager().AddSeenForm(form, field_types);

  // WHEN
  // Simulate manual text field change.
  auto field = form.fields[0];
  SimulateUserChangedTextField(form, field);
  // Simulate Autocomplete filling twice.
  autofill_manager().OnSingleFieldSuggestionSelected(
      u"", POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
  autofill_manager().OnSingleFieldSuggestionSelected(
      u"", POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
  // Simulate Autofill filling.
  FillTestProfile(form);
  SubmitForm(form);

  // THEN
  VerifySubmitFormUkm(
      test_ukm_recorder_, form, AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
      /*is_for_credit_card=*/false,
      /*has_upi_vpa_field=*/false, {FormType::kAddressForm},
      {.form_element_user_modifications = 1, .autofill_fills = 1});
}

TEST_F(AutofillMetricsTest, FormInteractionsAreInitiallyZero) {
  // GIVEN
  FormData form = test::GetFormData({.fields = {{.role = NAME_FULL}}});
  CreateSimpleForm(autofill_client_->form_origin(), form);

  std::vector<ServerFieldType> field_types = {NAME_FULL};
  autofill_manager().AddSeenForm(form, field_types);

  // WHEN
  SubmitForm(form);

  // THEN
  VerifySubmitFormUkm(test_ukm_recorder_, form,
                      AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                      /*is_for_credit_card=*/false,
                      /*has_upi_vpa_field=*/false, {FormType::kAddressForm});
}

// Base class for cross-frame filling metrics, in particular for
// Autofill.CreditCard.SeamlessFills.*.
class AutofillMetricsCrossFrameFormTest : public AutofillMetricsTest {
 public:
  struct CreditCardAndCvc {
    CreditCard credit_card;
    std::u16string cvc;
  };

  AutofillMetricsCrossFrameFormTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(features::kAutofillAcrossIframes, {}),
         base::test::FeatureRefAndParams(features::kAutofillSharedAutofill,
                                         {{"relax_shared_autofill", "true"}})},
        {});
  }
  ~AutofillMetricsCrossFrameFormTest() override = default;

  void SetUp() override {
    AutofillMetricsTest::SetUp();

    RecreateCreditCards(/*include_local_credit_card=*/true,
                        /*include_masked_server_credit_card=*/false,
                        /*include_full_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);

    credit_card_with_cvc_ = {
        .credit_card = *autofill_manager()
                            .personal_data_manager_for_test()
                            .GetCreditCardsToSuggest()
                            .front(),
        .cvc = u"123"};

    url::Origin main_origin =
        url::Origin::Create(GURL("https://example.test/"));
    url::Origin other_origin = url::Origin::Create(GURL("https://other.test/"));
    form_ = test::GetFormData(
        {.description_for_logging = "CrossFrameFillingMetrics",
         .fields =
             {
                 {.label = u"Cardholder name",
                  .name = u"card_name",
                  .is_autofilled = false},
                 {.label = u"CCNumber",
                  .name = u"ccnumber",
                  .is_autofilled = false,
                  .origin = other_origin},
                 {.label = u"ExpDate",
                  .name = u"expdate",
                  .is_autofilled = false},
                 {.is_visible = false,
                  .label = u"CVC",
                  .name = u"cvc",
                  .is_autofilled = false,
                  .origin = other_origin},
             },
         .unique_renderer_id = test::MakeFormRendererId(),
         .main_frame_origin = main_origin});

    ASSERT_EQ(form_.main_frame_origin, form_.fields[0].origin);
    ASSERT_EQ(form_.main_frame_origin, form_.fields[2].origin);
    ASSERT_NE(form_.main_frame_origin, form_.fields[1].origin);
    ASSERT_NE(form_.main_frame_origin, form_.fields[3].origin);
    ASSERT_EQ(form_.fields[1].origin, form_.fields[3].origin);

    // Mock a simplified security model which allows to filter (only) fields
    // from the same origin.
    autofill_driver_->SetFieldTypeMapFilter(base::BindRepeating(
        [](AutofillMetricsCrossFrameFormTest* self,
           const url::Origin& triggered_origin, FieldGlobalId field,
           ServerFieldType) {
          return triggered_origin == self->GetFieldById(field).origin;
        },
        this));
  }

  CreditCardAndCvc& fill_data() { return credit_card_with_cvc_; }

  // Any call to FillForm() should be followed by a SetFormValues() call to
  // mimic its effect on |form_|.
  void FillForm(const FormFieldData& triggering_field) {
    autofill_manager().FillCreditCardForm(
        form_, triggering_field, fill_data().credit_card, fill_data().cvc);
  }

  // Sets the field values of |form_| according to the parameters.
  //
  // Since this test suite doesn't use mocks, we can't intercept the autofilled
  // form. Therefore, after each manual fill or autofill, we shall call
  // SetFormValues()
  void SetFormValues(const ServerFieldTypeSet& fill_field_types,
                     bool is_autofilled,
                     bool is_user_typed) {
    auto type_to_index = base::MakeFixedFlatMap<ServerFieldType, size_t>(
        {{CREDIT_CARD_NAME_FULL, 0},
         {CREDIT_CARD_NUMBER, 1},
         {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, 2},
         {CREDIT_CARD_VERIFICATION_CODE, 3}});

    for (ServerFieldType fill_type : fill_field_types) {
      auto* index_it = type_to_index.find(fill_type);
      ASSERT_NE(index_it, type_to_index.end());
      FormFieldData& field = form_.fields[index_it->second];
      field.value = fill_type != CREDIT_CARD_VERIFICATION_CODE
                        ? fill_data().credit_card.GetRawInfo(fill_type)
                        : fill_data().cvc;
      field.is_autofilled = is_autofilled;
      field.properties_mask = (field.properties_mask & ~kUserTyped) |
                              (is_user_typed ? kUserTyped : 0);
    }
  }

  FormFieldData& GetFieldById(FieldGlobalId field) {
    auto it =
        base::ranges::find(form_.fields, field, &FormFieldData::global_id);
    CHECK(it != form_.fields.end());
    return *it;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FormData form_;
  CreditCardAndCvc credit_card_with_cvc_;
};

// This fixture adds utilities for the seamlessness metric names.
//
// These metric names get very long, and with >16 variants the tests become
// unreadable otherwise.
class AutofillMetricsSeamlessnessTest
    : public AutofillMetricsCrossFrameFormTest {
 public:
  struct MetricName {
    enum class Fill { kFills, kFillable };
    enum class Time { kBefore, kAfter, kSubmission };
    enum class Visibility { kAll, kVisible };
    enum class Variant { kQualitative, kBitmask };

    Fill fill;
    Time time;
    Visibility visibility;
    Variant variant;

    std::string str() const {
      return base::StringPrintf(
          "Autofill.CreditCard.Seamless%s.%s%s%s",
          fill == Fill::kFills ? "Fills" : "Fillable",
          time == Time::kSubmission ? "AtSubmissionTime"
          : time == Time::kBefore   ? "AtFillTimeBeforeSecurityPolicy"
                                    : "AtFillTimeAfterSecurityPolicy",
          visibility == Visibility::kAll ? "" : ".Visible",
          variant == Variant::kQualitative ? "" : ".Bitmask");
    }
  };

  static constexpr auto kFills = MetricName::Fill::kFills;
  static constexpr auto kFillable = MetricName::Fill::kFillable;
  static constexpr auto kBefore = MetricName::Time::kBefore;
  static constexpr auto kAfter = MetricName::Time::kAfter;
  static constexpr auto kSubmission = MetricName::Time::kSubmission;
  static constexpr auto kAll = MetricName::Visibility::kAll;
  static constexpr auto kVisible = MetricName::Visibility::kVisible;
  static constexpr auto kQualitative = MetricName::Variant::kQualitative;
  static constexpr auto kBitmask = MetricName::Variant::kBitmask;
};

// Tests that Autofill.CreditCard.SeamlessFills.* is not emitted for manual
// fills.
TEST_F(AutofillMetricsSeamlessnessTest,
       DoNotLogCreditCardSeamlessFillsMetricIfNotAutofilled) {
  using UkmBuilder = ukm::builders::Autofill_CreditCardFill;
  base::HistogramTester histogram_tester;
  SeeForm(form_);

  // Fake manual fill.
  SetFormValues(
      {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE},
      /*is_autofilled=*/false, /*is_user_typed=*/true);

  // Fakes an Autofill.
  // This fills nothing because all fields have been manually filled.
  FillForm(FormFieldData());
  SubmitForm(form_);
  ResetDriverToCommitMetrics();

  for (auto fill : {kFills, kFillable}) {
    for (auto time : {kBefore, kAfter, kSubmission}) {
      for (auto visibility : {kAll, kVisible}) {
        for (auto variant : {kQualitative, kBitmask}) {
          histogram_tester.ExpectTotalCount(
              MetricName{fill, time, visibility, variant}.str(), 0);
        }
      }
    }
  }

  VerifyUkm(test_ukm_recorder_, form_, UkmBuilder::kEntryName, {});
}

// Tests that Autofill.CreditCard.SeamlessFills.* are emitted.
TEST_F(AutofillMetricsSeamlessnessTest,
       LogCreditCardSeamlessFillsMetricIfAutofilledWithoutCvc) {
  using Metric = AutofillMetrics::CreditCardSeamlessness::Metric;
  using UkmBuilder = ukm::builders::Autofill_CreditCardFill;

  // `Metric` as raw integer for UKM.
  constexpr auto kFullFill = static_cast<uint64_t>(Metric::kFullFill);
  constexpr auto kOptionalCvcMissing =
      static_cast<uint64_t>(Metric::kOptionalCvcMissing);
  constexpr auto kPartialFill = static_cast<uint64_t>(Metric::kPartialFill);
  // Bits of the bitmask.
  constexpr uint8_t kName = true << 3;
  constexpr uint8_t kNumber = true << 2;
  constexpr uint8_t kExp = true << 1;
  constexpr uint8_t kCvc = true << 0;
  // The shared-autofill metric.
  enum SharedAutofillMetric : uint64_t {
    kSharedAutofillIsIrrelevant = 0,
    kSharedAutofillWouldHelp = 1,
    kSharedAutofillDidHelp = 2,
  };

  base::HistogramTester histogram_tester;
  auto SamplesOf = [&histogram_tester](MetricName metric) {
    return histogram_tester.GetAllSamples(metric.str());
  };

  SeeForm(form_);

  fill_data().cvc = u"";

  // Fakes an Autofill with the following behavior:
  // - before security and assuming a complete profile: kFullFill;
  // - before security and without a CVC:               kOptionalCvcMissing;
  // - after security  and assuming a complete profile: kPartialFill;
  // - after security  and without a CVC:               kPartialFill;
  // because due to the security policy, only NAME and EXP_DATE are filled.
  // The CVC field is invisible.
  FillForm(form_.fields[0]);
  SetFormValues({CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Fakes an Autofill with the following behavior:
  // - before security and assuming a complete profile: kFullFill;
  // - before security and without a CVC:               kPartialFill;
  // - after security  and assuming a complete profile: kPartialFill;
  // - after security  and without a CVC:               kPartialFill;
  // because the due to the security policy, only NUMBER and CVC could be
  // filled.
  // The CVC field is invisible.
  FillForm(form_.fields[1]);
  SetFormValues({CREDIT_CARD_NUMBER},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  SubmitForm(form_);
  ResetDriverToCommitMetrics();

  // Bitmask metrics.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp | kCvc, 2)));
  EXPECT_THAT(SamplesOf({kFillable, kAfter, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber | kCvc, 1)));
  EXPECT_THAT(
      SamplesOf({kFills, kBefore, kAll, kBitmask}),
      BucketsAre(Bucket(kName | kNumber | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kSubmission, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp, 1)));
  // Bitmask metrics restricted to visible fields.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kVisible, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp, 2)));
  EXPECT_THAT(SamplesOf({kFillable, kAfter, kVisible, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(
      SamplesOf({kFills, kBefore, kVisible, kBitmask}),
      BucketsAre(Bucket(kName | kNumber | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kVisible, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));

  // Qualitative metrics.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kFullFill, 2)));
  EXPECT_THAT(SamplesOf({kFillable, kAfter, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf({kFills, kBefore, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1),
                         Bucket(Metric::kPartialFill, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf({kFills, kSubmission, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1)));
  // Qualitative metrics restricted to visible fields.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 2)));
  EXPECT_THAT(SamplesOf({kFillable, kAfter, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf({kFills, kBefore, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1),
                         Bucket(Metric::kPartialFill, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));

  VerifyUkm(
      test_ukm_recorder_, form_, UkmBuilder::kEntryName,
      {{
           {UkmBuilder::kFillable_BeforeSecurity_QualitativeName, kFullFill},
           {UkmBuilder::kFillable_AfterSecurity_QualitativeName, kPartialFill},
           {UkmBuilder::kFilled_BeforeSecurity_QualitativeName,
            kOptionalCvcMissing},
           {UkmBuilder::kFilled_AfterSecurity_QualitativeName, kPartialFill},

           {UkmBuilder::kFillable_BeforeSecurity_BitmaskName,
            kName | kNumber | kExp | kCvc},
           {UkmBuilder::kFillable_AfterSecurity_BitmaskName, kName | kExp},
           {UkmBuilder::kFilled_BeforeSecurity_BitmaskName,
            kName | kNumber | kExp},
           {UkmBuilder::kFilled_AfterSecurity_BitmaskName, kName | kExp},

           {UkmBuilder::kFillable_BeforeSecurity_Visible_QualitativeName,
            kOptionalCvcMissing},
           {UkmBuilder::kFillable_AfterSecurity_Visible_QualitativeName,
            kPartialFill},
           {UkmBuilder::kFilled_BeforeSecurity_Visible_QualitativeName,
            kOptionalCvcMissing},
           {UkmBuilder::kFilled_AfterSecurity_Visible_QualitativeName,
            kPartialFill},

           {UkmBuilder::kFillable_BeforeSecurity_Visible_BitmaskName,
            kName | kNumber | kExp},
           {UkmBuilder::kFillable_AfterSecurity_Visible_BitmaskName,
            kName | kExp},
           {UkmBuilder::kFilled_BeforeSecurity_Visible_BitmaskName,
            kName | kNumber | kExp},
           {UkmBuilder::kFilled_AfterSecurity_Visible_BitmaskName,
            kName | kExp},

           {UkmBuilder::kSharedAutofillName, kSharedAutofillWouldHelp},

           {UkmBuilder::kFormSignatureName,
            *Collapse(CalculateFormSignature(form_))},
       },
       {
           {UkmBuilder::kFillable_BeforeSecurity_QualitativeName, kFullFill},
           {UkmBuilder::kFillable_AfterSecurity_QualitativeName, kPartialFill},
           {UkmBuilder::kFilled_BeforeSecurity_QualitativeName, kPartialFill},
           {UkmBuilder::kFilled_AfterSecurity_QualitativeName, kPartialFill},

           {UkmBuilder::kFillable_BeforeSecurity_BitmaskName,
            kName | kNumber | kExp | kCvc},
           {UkmBuilder::kFillable_AfterSecurity_BitmaskName, kNumber | kCvc},
           {UkmBuilder::kFilled_BeforeSecurity_BitmaskName, kNumber},
           {UkmBuilder::kFilled_AfterSecurity_BitmaskName, kNumber},

           {UkmBuilder::kFillable_BeforeSecurity_Visible_QualitativeName,
            kOptionalCvcMissing},
           {UkmBuilder::kFillable_AfterSecurity_Visible_QualitativeName,
            kPartialFill},
           {UkmBuilder::kFilled_BeforeSecurity_Visible_QualitativeName,
            kPartialFill},
           {UkmBuilder::kFilled_AfterSecurity_Visible_QualitativeName,
            kPartialFill},

           {UkmBuilder::kFillable_BeforeSecurity_Visible_BitmaskName,
            kName | kNumber | kExp},
           {UkmBuilder::kFillable_AfterSecurity_Visible_BitmaskName, kNumber},
           {UkmBuilder::kFilled_BeforeSecurity_Visible_BitmaskName, kNumber},
           {UkmBuilder::kFilled_AfterSecurity_Visible_BitmaskName, kNumber},

           {UkmBuilder::kSharedAutofillName, kSharedAutofillIsIrrelevant},

           {UkmBuilder::kFormSignatureName,
            *Collapse(CalculateFormSignature(form_))},
       }});
}

// Test the field log events at the form submission.
class AutofillMetricsFromLogEventsTest : public AutofillMetricsTest {
 protected:
  AutofillMetricsFromLogEventsTest() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillLogUKMEventsWithSampleRate,
                              features::kAutofillParsingPatternProvider},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Test if we record FieldInfo UKM metrics correctly after we fill and submit an
// address form.
TEST_F(AutofillMetricsFromLogEventsTest, AddressSubmittedFormLogEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  FormData form = CreateForm({CreateField("State", "state", "", "text"),
                              CreateField("Street", "street", "", "text"),
                              CreateField("Number", "", "", "text")});

  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_STREET_ADDRESS, NO_SERVER_DATA};

  autofill_manager().AddSeenForm(form, field_types);

  {
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
    SubmitForm(form);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  auto entries =
      test_ukm_recorder_->GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(0u, entries.size());

  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data. The third field cannot be
    // autofilled because its type cannot be predicated.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields[0], gfx::RectF(), AutoselectFirstSuggestion(false),
        FormElementWasClicked(true));
    FillTestProfile(form);

    // Simulate text input in the first fields.
    SimulateUserChangedTextField(form, form.fields[0]);
    SubmitForm(form);

    // Record Autofill2.FieldInfo UKM event at autofill manager reset.
    autofill_manager().Reset();

    entries =
        test_ukm_recorder_->GetEntriesByName(UkmFieldInfoType::kEntryName);
    ASSERT_EQ(3u, entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
      SCOPED_TRACE(testing::Message() << i);
      using UFIT = UkmFieldInfoType;
      const auto* const entry = entries[i];

      SkipStatus status =
          i == 2 ? SkipStatus::kNoFillableGroup : SkipStatus::kNotSkipped;
      std::map<std::string, int64_t> expected = {
          {UFIT::kFormSessionIdentifierName,
           AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
          {UFIT::kFieldSessionIdentifierName,
           AutofillMetrics::FieldGlobalIdToHash64Bit(
               form.fields[i].global_id())},
          {UFIT::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[i])).value()},
          {UFIT::kWasFocusedName, i == 0},
          {UFIT::kIsFocusableName, true},
          {UFIT::kWasAutofilledName, i != 2},
          {UFIT::kAutofillSkippedStatusName,
           DenseSet<SkipStatus>{status}.to_uint64()},
          {UFIT::kWasRefillName, false},
          {UFIT::kHadValueBeforeFillingName, false},
          {UFIT::kUserTypedIntoFieldName, i == 0},
          {UFIT::kHadTypedOrFilledValueAtSubmissionName, i != 2},
      };
      if (i == 0) {
        expected[UFIT::kSuggestionWasAvailableName] = true;
        expected[UFIT::kSuggestionWasShownName] = true;
        expected[UFIT::kSuggestionWasAcceptedName] = true;
      }
      if (i != 2) {
        expected[UFIT::kFilledValueWasModifiedName] = i == 0;
      }

      EXPECT_EQ(expected.size(), entry->metrics.size());
      for (const auto& [metric, value] : expected) {
        test_ukm_recorder_->ExpectEntryMetric(entry, metric, value);
      }
    }
  }
}

// Test if we have recorded UKM metrics correctly about field types after
// parsing the form by the local heuristic prediction.
TEST_F(AutofillMetricsFromLogEventsTest, AutofillFieldInfoMetricsFieldType) {
  FormData form = CreateForm(
      {// Heuristic value will match with Autocomplete attribute.
       CreateField("Last Name", "lastname", "", "text", "family-name"),
       // Heuristic value will NOT match with Autocomplete attribute.
       CreateField("First Name", "firstname", "", "text", "additional-name"),
       // No autocomplete attribute.
       CreateField("Address", "address", "", "text", ""),
       // Heuristic value will be unknown.
       CreateField("Garbage label", "garbage", "", "text", "postal-code")});

  auto form_structure = std::make_unique<FormStructure>(form);
  FormStructure* form_structure_ptr = form_structure.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  ASSERT_TRUE(
      autofill_manager()
          .mutable_form_structures_for_test()
          ->emplace(form_structure_ptr->global_id(), std::move(form_structure))
          .second);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // The server type of each field predicted from autofill crowdsourced server.
  std::vector<ServerFieldType> server_types{
      // Server response will match with autocomplete.
      NAME_LAST,
      // Server response will NOT match with autocomplete.
      NAME_FIRST,
      // No autocomplete, server predicts a type from majority voting.
      NAME_MIDDLE,
      // Server response will have no data.
      NO_SERVER_DATA};
  // Set suggestions from server for the form.
  for (size_t i = 0; i < server_types.size(); ++i) {
    AddFieldPredictionToForm(form.fields[i], server_types[i], form_suggestion);
  }

  std::string response_string = SerializeAndEncode(response);
  autofill_manager().OnLoadedServerPredictionsForTest(
      response_string, test::GetEncodedSignatures(*form_structure_ptr));

  SubmitForm(form);
  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  autofill_manager().Reset();

  auto entries =
      test_ukm_recorder_->GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(4u, entries.size());
  // The heuristic type of each field. The local heuristic prediction does not
  // predict the type for the fourth field.
  std::vector<ServerFieldType> heuristic_types{
      NAME_LAST, NAME_FIRST, ADDRESS_HOME_LINE1, UNKNOWN_TYPE};
  // Field types as per the autocomplete attribute in the input.
  std::vector<HtmlFieldType> html_field_types{
      HtmlFieldType::kFamilyName, HtmlFieldType::kAdditionalName,
      HtmlFieldType::kUnrecognized, HtmlFieldType::kPostalCode};

  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i];
    FieldPrediction::Source prediction_source =
        server_types[i] != NO_SERVER_DATA
            ? FieldPrediction::SOURCE_AUTOFILL_DEFAULT
            : FieldPrediction::SOURCE_UNSPECIFIED;
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(form.fields[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[i])).value()},
        {UFIT::kServerType1Name, server_types[i]},
        {UFIT::kServerPredictionSource1Name, prediction_source},
        {UFIT::kServerType2Name, NO_SERVER_DATA},
        {UFIT::kServerPredictionSource2Name,
         FieldPrediction::SOURCE_UNSPECIFIED},
        {UFIT::kServerTypeIsOverrideName, false},
        {UFIT::kIsFocusableName, true},
        {UFIT::kRankInFieldSignatureGroupName, 1},
        {UFIT::kWasFocusedName, false},
        {UFIT::kUserTypedIntoFieldName, false},
    };
    if (heuristic_types[i] != UNKNOWN_TYPE) {
      expected.merge(std::map<std::string, int64_t>({
          {UFIT::kHeuristicTypeName, heuristic_types[i]},
          {UFIT::kHeuristicTypeLegacyName, heuristic_types[i]},
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
          {UFIT::kHeuristicTypeDefaultName, heuristic_types[i]},
          {UFIT::kHeuristicTypeExperimentalName, heuristic_types[i]},
          {UFIT::kHeuristicTypeNextGenName, heuristic_types[i]},
#else
          {UFIT::kHeuristicTypeDefaultName, UNKNOWN_TYPE},
          {UFIT::kHeuristicTypeExperimentalName, UNKNOWN_TYPE},
          {UFIT::kHeuristicTypeNextGenName, UNKNOWN_TYPE},
#endif
      }));
    }
    if (html_field_types[i] != HtmlFieldType::kUnrecognized) {
      expected.merge(std::map<std::string, int64_t>({
          {UFIT::kHtmlFieldTypeName, static_cast<int>(html_field_types[i])},
          {UFIT::kHtmlFieldModeName, static_cast<int>(HtmlFieldMode::kNone)},
      }));
    }
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder_->ExpectEntryMetric(entry, metric, value);
    }
  }
}

// Test if we have recorded FieldInfo UKM metrics correctly after typing in
// fields without autofilling first.
TEST_F(AutofillMetricsFromLogEventsTest,
       AutofillFieldInfoMetricsEditedFieldWithoutFill) {
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.role = NAME_FULL,
                  .value = u"Elvis Aaron Presley",
                  .is_autofilled = false},
                 {.role = EMAIL_ADDRESS,
                  .value = u"buddy@gmail.com",
                  .is_autofilled = false},
                 {.role = PHONE_HOME_CITY_AND_NUMBER, .is_autofilled = true}},
      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields[0]);
  SimulateUserChangedTextField(form, form.fields[1]);

  SubmitForm(form);

  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  autofill_manager().Reset();

  auto entries =
      test_ukm_recorder_->GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(2u, entries.size());

  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i];

    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(form.fields[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[i])).value()},
        {UFIT::kWasFocusedName, false},
        {UFIT::kIsFocusableName, true},
        {UFIT::kUserTypedIntoFieldName, true},
        {UFIT::kHadTypedOrFilledValueAtSubmissionName, true},
    };

    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder_->ExpectEntryMetric(entry, metric, value);
    }
  }
}

// TODO(crbug.com/1352826) Delete this after collecting the metrics.
struct LaxLocalHeuristicsTestCase {
  test::FormDescription form;
  std::vector<ServerFieldType> heuristic_types;
  std::vector<ServerFieldType> server_types;
  bool change_form_after_filling = false;
  std::string affected_metric;
  std::vector<Bucket> expected_buckets;
};

// TODO(crbug.com/1352826) Delete this after collecting the metrics.
class AutofillMetricsTestForLaxLocalHeuristics
    : public AutofillMetricsTest,
      public ::testing::WithParamInterface<LaxLocalHeuristicsTestCase> {};

TEST_P(AutofillMetricsTestForLaxLocalHeuristics, TestHistogramReporting) {
  SCOPED_TRACE(GetParam().form.description_for_logging);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  // Set up our form data.
  FormData form = test::GetFormData(GetParam().form);
  bool is_cc_form = AutofillType(GetParam().form.fields[0].role).group() ==
                    FieldTypeGroup::kCreditCard;
  autofill_manager().AddSeenForm(form, GetParam().heuristic_types,
                                 GetParam().server_types);
  // User interacted with form.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  // Simulate seeing a suggestion.
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  // Simulate filling the form.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(),
      is_cc_form ? MakeFrontendId({.credit_card_id = kTestLocalCardId})
                 : MakeFrontendId({.profile_id = kTestProfileId}));

  if (GetParam().change_form_after_filling)
    SimulateUserChangedTextField(form, form.fields[0]);

  base::HistogramTester histogram_tester;
  SubmitForm(form);
  EXPECT_THAT(histogram_tester.GetAllSamples(GetParam().affected_metric),
              BucketsAre(GetParam().expected_buckets));
}

// TODO(crbug.com/1352826) Delete this after collecting the metrics.
INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTestForLaxLocalHeuristics,
    AutofillMetricsTestForLaxLocalHeuristics,
    testing::Values(
        // Because the local heuristic classifies 3 dictinct field types, we
        // don't expect any metrics. This form is not eligible.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Three different field types",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_CITY},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA, NO_SERVER_DATA},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {}},
        // This is a case where we don't have 3 distinct field types and we
        // would expect that the filling acceptance is reported.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Repeated field types",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA, NO_SERVER_DATA},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {Bucket(true, 1)}},
        // Here we would not expect any reports because the server overrides
        // would mean that a change to the heuristics would have no effect.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "All overridden by server",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {}},
        // Here we would expect metrics because the first field is only
        // classified by the heuristic, the server does not know it.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Server misses field",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, ADDRESS_HOME_LINE1,
                             ADDRESS_HOME_LINE2},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {Bucket(true, 1)}},
        // This is a special case of the previous because email addresses are
        // always classified by the local heuristic. They don't require the
        // minimum number of classified fields. Therefore, a more stricter
        // heuristic would not make any difference and we expect no metrics.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Email address exception",
                     .fields = {{.role = EMAIL_ADDRESS},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {EMAIL_ADDRESS, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, ADDRESS_HOME_LINE1,
                             ADDRESS_HOME_LINE2},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {}},
        // This is a special case of the previous because promo codes are
        // always classified by the local heuristic. They don't require the
        // minimum number of classified fields. Therefore, a more stricter
        // heuristic would not make any difference and we expect no metrics.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Promo code exception",
                     .fields = {{.role = MERCHANT_PROMO_CODE},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {MERCHANT_PROMO_CODE, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, ADDRESS_HOME_LINE1,
                             ADDRESS_HOME_LINE2},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.Address",
            .expected_buckets = {}},
        // Check that this also works for credit card fields.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Credit card",
                     .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                {.role = CREDIT_CARD_NUMBER},
                                {.role = CREDIT_CARD_VERIFICATION_CODE}}},
            .heuristic_types = {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                CREDIT_CARD_NUMBER},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA, NO_SERVER_DATA},
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingAcceptance.CreditCard",
            .expected_buckets = {Bucket(true, 1)}},
        // Ensure that the correctness is properly reported if the user edits
        // a field.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Correctness of edited form",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA, NO_SERVER_DATA},
            .change_form_after_filling = true,
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingCorrectness.Address",
            .expected_buckets = {Bucket(false, 1)}},
        // Ensure that the correctness is properly reported if the user does
        // not edit a field.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Correctness of edited form",
                     .fields = {{.role = NAME_FULL},
                                {.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA, NO_SERVER_DATA},
            .change_form_after_filling = false,
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingCorrectness.Address",
            .expected_buckets = {Bucket(true, 1)}},
        // Ensure that forms with two fields don't emit metrics.
        LaxLocalHeuristicsTestCase{
            .form = {.description_for_logging = "Two field form",
                     .fields = {{.role = ADDRESS_HOME_LINE1},
                                {.role = ADDRESS_HOME_CITY}}},
            .heuristic_types = {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE1},
            .server_types = {NO_SERVER_DATA, NO_SERVER_DATA},
            .change_form_after_filling = false,
            .affected_metric = "Autofill.FormAffectedByLaxLocalHeuristicRule."
                               "FillingCorrectness.Address",
            .expected_buckets = {},
        }));

}  // namespace autofill::metrics
