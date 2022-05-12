// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

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
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/form_interactions_counter.h"
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
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/test_sync_service.h"
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

using ::autofill::metrics::kTestGuid;
using ::base::ASCIIToUTF16;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::BucketsInclude;
using ::base::TimeTicks;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::UnorderedPointwise;

namespace autofill {

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

using ExpectedUkmMetricsRecord = std::vector<std::pair<const char*, int64_t>>;
using ExpectedUkmMetrics = std::vector<ExpectedUkmMetricsRecord>;

using AddressImportRequirements =
    AutofillMetrics::AddressProfileImportRequirementMetric;

const int kDefaultPageID = 137;

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
               form_interaction_counts.autofill_fills},
              {UkmFormSubmittedType::kAutocompleteFillsName,
               form_interaction_counts.autocomplete_fills}}});
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

void AddAutoCompleteFieldToForm(const std::string& type, FormData& form) {
  FormFieldData field;
  test::CreateTestFormField("", "", "", "", &field);
  field.autocomplete_attribute = type;
  form.fields.push_back(field);
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

// This is defined in the autofill_metrics.cc implementation file.
int GetFieldTypeGroupPredictionQualityMetric(
    ServerFieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric);

class AutofillMetricsTest : public metrics::AutofillMetricsBaseTest {
 public:
  AutofillMetricsTest() = default;
  ~AutofillMetricsTest() override = default;
};

// Test parameter indicates if the metrics are being logged for a form in an
// iframe or the main frame. True means the form is in the main frame.
class AutofillMetricsIFrameTest : public testing::WithParamInterface<bool>,
                                  public metrics::AutofillMetricsBaseTest {
 public:
  AutofillMetricsIFrameTest()
      : metrics::AutofillMetricsBaseTest(
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

// Test that we log the right number of autofilled fields at submission time.
TEST_F(AutofillMetricsTest, NumberOfAutofilledFieldsAtSubmission) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "NumberOfAutofilledFields",
                         .fields = {{.label = u"Autofilled",
                                     .name = u"autofilled",
                                     .value = u"Elvis Aaron Presley",
                                     .is_autofilled = true},
                                    {.label = u"Autofilled but corrected",
                                     .name = u"autofillfailed",
                                     .value = u"buddy@gmail.com",
                                     .is_autofilled = true},
                                    {.label = u"Empty",
                                     .name = u"empty",
                                     .value = u"",
                                     .is_autofilled = false},
                                    {.label = u"Unknown",
                                     .name = u"unknown",
                                     .value = u"garbage",
                                     .is_autofilled = false},
                                    {.label = u"Select",
                                     .name = u"select",
                                     .value = u"USA",
                                     .form_control_type = "select-one",
                                     .is_autofilled = false},
                                    {.role = ServerFieldType::PHONE_HOME_NUMBER,
                                     .value = u"2345678901",
                                     .form_control_type = "tel",
                                     .is_autofilled = true}},
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,         PHONE_HOME_NUMBER, NAME_FULL,
      PHONE_HOME_NUMBER, UNKNOWN_TYPE,      PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {
      NAME_FIRST,    EMAIL_ADDRESS,  NAME_FIRST,
      EMAIL_ADDRESS, NO_SERVER_DATA, PHONE_HOME_CITY_AND_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate user changing the second field of the form.
  autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());
  form.fields.at(1).is_autofilled = false;

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillFillAndImportFromMoreFields);
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "NumberOfAutofilledFields",
                         .fields = {{.label = u"Autofilled",
                                     .name = u"autofilled",
                                     .value = u"Elvis Aaron Presley",
                                     .autocomplete_attribute = "garbage",
                                     .is_autofilled = true},
                                    {.label = u"Autofilled but corrected",
                                     .name = u"autofillfailed",
                                     .value = u"buddy@gmail.com",
                                     .autocomplete_attribute = "garbage",
                                     .is_autofilled = true},
                                    {.label = u"Empty",
                                     .name = u"empty",
                                     .value = u"",
                                     .is_autofilled = false},
                                    {.label = u"Unknown",
                                     .name = u"unknown",
                                     .value = u"garbage",
                                     .is_autofilled = false},
                                    {.label = u"Select",
                                     .name = u"select",
                                     .value = u"USA",
                                     .form_control_type = "select-one",
                                     .is_autofilled = false},
                                    {.role = ServerFieldType::PHONE_HOME_NUMBER,
                                     .value = u"2345678901",
                                     .form_control_type = "tel",
                                     .is_autofilled = true}},
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,         PHONE_HOME_NUMBER, NAME_FULL,
      PHONE_HOME_NUMBER, UNKNOWN_TYPE,      PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {
      NAME_FIRST,    EMAIL_ADDRESS,  NAME_FIRST,
      EMAIL_ADDRESS, NO_SERVER_DATA, PHONE_HOME_CITY_AND_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate user changing the second and forth field of the form.
  autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());
  form.fields.at(1).is_autofilled = false;

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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

// Test that we log the perfect filling metric correctly for an address form in
// which every field is autofilled.
TEST_F(AutofillMetricsTest, PerfectFillingForAddresses_AllAutofillFilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"Email",
                                  .name = u"email",
                                  .value = u"buddy@gmail.com",
                                  .is_autofilled = true},
                                 {.label = u"City",
                                  .name = u"city",
                                  .value = u"Munich",
                                  .is_autofilled = true},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS,
                                                  ADDRESS_HOME_CITY};
  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS,
                                               ADDRESS_HOME_CITY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for perfect filling for
  // addresses.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));
}

// Test that we log the perfect filling metric correctly for an address form in
// which every field is autofilled or empty.
TEST_F(AutofillMetricsTest,
       PerfectFillingForAddresses_AllAutofillFilledOrEmpty) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"Email",
                                  .name = u"email",
                                  .value = u"buddy@gmail.com",
                                  .is_autofilled = true},
                                 {.label = u"City",
                                  .name = u"city",
                                  .value = u"",
                                  .is_autofilled = false},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS,
                                                  ADDRESS_HOME_CITY};
  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS,
                                               ADDRESS_HOME_CITY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for perfect filling for
  // addresses.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));
}

// Test that we log the perfect filling metric correctly for an address form in
// which a non-empty field is not autofilled.
TEST_F(AutofillMetricsTest, PerfectFillingForAddresses_NotAllAutofilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"Email",
                                  .name = u"email",
                                  .value = u"buddy@gmail.com",
                                  .is_autofilled = true},
                                 {.label = u"City",
                                  .name = u"city",
                                  .value = u"Munich",
                                  .is_autofilled = false},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS,
                                                  ADDRESS_HOME_CITY};
  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS,
                                               ADDRESS_HOME_CITY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for non-perfect filling for
  // addresses.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));
}

// Test that we log the perfect filling metric correctly for a credit card form
// in which every field is autofilled.
TEST_F(AutofillMetricsTest, PerfectFillingForCreditCards_AllAutofilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"CCNumber",
                                  .name = u"ccnumber",
                                  .value = u"01230123012399",
                                  .is_autofilled = true},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {CREDIT_CARD_NAME_FULL,
                                                  CREDIT_CARD_NUMBER};
  std::vector<ServerFieldType> server_types = {CREDIT_CARD_NAME_FULL,
                                               CREDIT_CARD_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for perfect filling for credit
  // cards.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
}

// Test that we log the perfect filling metric correctly for a credit card form
// in which not every field is autofilled or empty.
TEST_F(AutofillMetricsTest, PerfectFillingForCreditCards_NotAllAutofilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"CCNumber",
                                  .name = u"ccnumber",
                                  .value = u"01230123012399",
                                  .is_autofilled = false},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {CREDIT_CARD_NAME_FULL,
                                                  CREDIT_CARD_NUMBER};
  std::vector<ServerFieldType> server_types = {CREDIT_CARD_NAME_FULL,
                                               CREDIT_CARD_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for non-perfect filling for
  // credit cards.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 0), Bucket(true, 0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));
}

// Test that we log the perfect filling metric correctly for a form that
// contains both credit card and address information. Here, the form is fully
// autofilled resulting in a perfect count for both addresses and credit cards.
TEST_F(AutofillMetricsTest, PerfectFillingForMixedForm_AllAutofilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"CCNumber",
                                  .name = u"ccnumber",
                                  .value = u"01230123012399",
                                  .is_autofilled = true},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  CREDIT_CARD_NUMBER};
  std::vector<ServerFieldType> server_types = {NAME_FULL, CREDIT_CARD_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for perfect filling for credit
  // cards and for addresses.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 0), Bucket(true, 1)));
}

// Test that we log the perfect filling metric correctly for a form that
// contains both credit card and address information.  Here, the form is not
// fully autofilled resulting in a non-perfect count for both addresses and
// credit cards
TEST_F(AutofillMetricsTest, PerfectFillingForMixedForm_NotAllAutofilled) {
  // Set up our form data with two autofilled fields.
  FormData form =
      test::GetFormData({.description_for_logging = "PerectFilling",
                         .fields =
                             {
                                 {.label = u"Name",
                                  .name = u"name",
                                  .value = u"Elvis Aaron Presley",
                                  .is_autofilled = true},
                                 {.label = u"CCNumber",
                                  .name = u"ccnumber",
                                  .value = u"01230123012399",
                                  .is_autofilled = false},
                             },
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  CREDIT_CARD_NUMBER};
  std::vector<ServerFieldType> server_types = {NAME_FULL, CREDIT_CARD_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // Here, it is expected that there is a count for non-perfect filling for
  // credit cards and for addresses.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.Addresses"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.PerfectFilling.CreditCards"),
      BucketsAre(Bucket(false, 1), Bucket(true, 0)));
}

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
  // Set up our form data.
  FormData form =
      test::GetFormData({.description_for_logging = "QualityMetrics",
                         .fields = {{.label = u"Autofilled",
                                     .name = u"autofilled",
                                     .value = u"Elvis Aaron Presley",
                                     .is_autofilled = true},
                                    {.label = u"Autofill Failed",
                                     .name = u"autofillfailed",
                                     .value = u"buddy@gmail.com",
                                     .is_autofilled = false},
                                    {.label = u"Empty",
                                     .name = u"empty",
                                     .value = u"",
                                     .is_autofilled = false},
                                    {.label = u"Unknown",
                                     .name = u"unknown",
                                     .value = u"garbage",
                                     .is_autofilled = false},
                                    {.label = u"Select",
                                     .name = u"select",
                                     .value = u"USA",
                                     .form_control_type = "select-one",
                                     .is_autofilled = false},
                                    {.role = ServerFieldType::PHONE_HOME_NUMBER,
                                     .value = u"2345678901",
                                     .form_control_type = "tel",
                                     .is_autofilled = true}},
                         .unique_renderer_id = test::MakeFormRendererId(),
                         .main_frame_origin = url::Origin::Create(
                             autofill_client_->form_origin())});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,         PHONE_HOME_NUMBER, NAME_FULL,
      PHONE_HOME_NUMBER, UNKNOWN_TYPE,      PHONE_HOME_CITY_AND_NUMBER};
  std::vector<ServerFieldType> server_types = {
      NAME_FIRST,    EMAIL_ADDRESS,  NAME_FIRST,
      EMAIL_ADDRESS, NO_SERVER_DATA, PHONE_HOME_CITY_AND_NUMBER};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportStatus_NoImport",
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE,
            .value = u"Invalid State"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP,
            .value = u"00000000000000000"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY,
            .value = u"NoACountry"}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportStatus_RegularImport",
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportStatus_UnionImport",
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_CITY,
            .value = u"New York",
            .autocomplete_attribute = "section-billing locality"},
           // Add the last field of the form into a new section.
           {.role = ServerFieldType::ADDRESS_HOME_STATE,
            .value = u"CA",
            .autocomplete_attribute = "section-shipping address-level1"}}});

  // Set the heuristic types.
  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_ZIP,
                                                  ADDRESS_HOME_COUNTRY,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  ADDRESS_HOME_CITY,
                                                  ADDRESS_HOME_STATE};

  // Set the server types.
  std::vector<ServerFieldType> server_types = {NAME_FULL,
                                               ADDRESS_HOME_LINE1,
                                               ADDRESS_HOME_ZIP,
                                               ADDRESS_HOME_COUNTRY,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               ADDRESS_HOME_CITY,
                                               ADDRESS_HOME_STATE};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  base::HistogramTester histogram_tester;

  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportRequirements_AllFulfilled",
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1, .value = u""},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u""},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY,
            .value = u"Germany"}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_FilledButInvalidZipEmailAndState",
       .fields = {
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE,
            .value = u"DefNotAState"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"1234567890"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = ServerFieldType::EMAIL_ADDRESS,
            .value = u"test_noat_test.io"}}});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_CITY,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  ADDRESS_HOME_STATE,
                                                  ADDRESS_HOME_ZIP,
                                                  ADDRESS_HOME_COUNTRY,
                                                  EMAIL_ADDRESS};
  std::vector<ServerFieldType> server_types = {NAME_FULL,
                                               ADDRESS_HOME_LINE1,
                                               ADDRESS_HOME_CITY,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               ADDRESS_HOME_STATE,
                                               ADDRESS_HOME_ZIP,
                                               ADDRESS_HOME_COUNTRY,
                                               EMAIL_ADDRESS};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = ServerFieldType::EMAIL_ADDRESS,
            .value = u"test_noat_test.io"},
           {.label = u"Email1",
            .name = u".email1",
            .value = u"not_test@test.io"}}});

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL,
                                                  ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_CITY,
                                                  PHONE_HOME_CITY_AND_NUMBER,
                                                  ADDRESS_HOME_STATE,
                                                  ADDRESS_HOME_ZIP,
                                                  ADDRESS_HOME_COUNTRY,
                                                  EMAIL_ADDRESS,
                                                  EMAIL_ADDRESS};
  std::vector<ServerFieldType> server_types = {NAME_FULL,
                                               ADDRESS_HOME_LINE1,
                                               ADDRESS_HOME_CITY,
                                               PHONE_HOME_CITY_AND_NUMBER,
                                               ADDRESS_HOME_STATE,
                                               ADDRESS_HOME_ZIP,
                                               ADDRESS_HOME_COUNTRY,
                                               EMAIL_ADDRESS,
                                               EMAIL_ADDRESS};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
           {.role = ServerFieldType::NAME_FULL,
            .value = u"Elvis Aaron Presley"},
           {.role = ServerFieldType::ADDRESS_HOME_LINE1,
            .value = u"3734 Elvis Presley Blvd."},
           {.role = ServerFieldType::ADDRESS_HOME_CITY, .value = u""},
           {.role = ServerFieldType::PHONE_HOME_NUMBER, .value = u""},
           {.role = ServerFieldType::ADDRESS_HOME_STATE, .value = u""},
           {.role = ServerFieldType::ADDRESS_HOME_ZIP, .value = u""},
           {.role = ServerFieldType::ADDRESS_HOME_COUNTRY, .value = u""}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::STATE_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::EMAIL_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::ZIP_VALID_REQUIREMENT_VIOLATED, false},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_FULFILLED, true},
      {AddressImportRequirements::PHONE_VALID_REQUIREMENT_VIOLATED, false},
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
      {AddressImportRequirements::ZIP_OR_STATE_REQUIREMENT_VIOLATED, false},
  };

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Name", "name", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Address", "address", "3734 Elvis Presley Blvd.",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  test::CreateTestFormField("Phone", "phone", "2345678901", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  field.is_autofilled = false;

  // Below are fields with only_fill_when_focused set to true.
  // RATIONALIZATION_OK because it's ambiguous value.
  test::CreateTestFormField("Phone1", "phone1", "nonsense value", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  // RATIONALIZATION_OK because it's same type but different
  // to what is in the profile.
  test::CreateTestFormField("Phone2", "phone2", "2345678902", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  // RATIONALIZATION_OK because it's a type mismatch.
  test::CreateTestFormField("Phone3", "phone3", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  base::UserActionTester user_action_tester;
  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  // Trigger phone number rationalization at filling time.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Name", "name", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Address", "address", "3734 Elvis Presley Blvd.",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  test::CreateTestFormField("Phone", "phone", "2345678901", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  field.is_autofilled = false;

  // Below are fields with only_fill_when_focused set to true.
  // RATIONALIZATION_GOOD because it's empty.
  test::CreateTestFormField("Phone1", "phone1", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  base::UserActionTester user_action_tester;
  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  // Trigger phone number rationalization at filling time.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  FieldSignature field_signature[4];

  // no decision
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);

  // skips
  test::CreateTestFormField("Street", "street", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_LINE1);
  field_signature[0] = Collapse(CalculateFieldSignatureForField(field));

  // skips
  test::CreateTestFormField("City", "city", "", "text", &field);
  field.role = FormFieldData::RoleAttribute::kPresentation;
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  field_signature[1] = Collapse(CalculateFieldSignatureForField(field));

  // doesn't skip
  test::CreateTestFormField("State", "state", "", "select-one", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  field_signature[2] = Collapse(CalculateFieldSignatureForField(field));

  // doesn't skip
  test::CreateTestFormField("Country", "country", "", "select-one", &field);
  field.role = FormFieldData::RoleAttribute::kPresentation;
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_COUNTRY);
  field_signature[3] = Collapse(CalculateFieldSignatureForField(field));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate filling form.
  {
    base::UserActionTester user_action_tester;
    std::string guid(kTestGuid);  // local profile.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HTML_MODE_NONE},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HTML_MODE_NONE},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HTML_MODE_NONE},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kHtmlFieldModeName,
         HTML_MODE_NONE},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kIsSkippedName,
         false}}});
}

namespace {
void AddFieldSuggestionToForm(
    AutofillQueryResponse_FormSuggestion* form_suggestion,
    FormFieldData field_data,
    ServerFieldType field_type) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  field_suggestion->add_predictions()->set_type(field_type);
}
}  // namespace

// Test that we log the address line fields whose server types are rationalized
TEST_F(AutofillMetricsTest, LogRepeatedAddressTypeRationalized) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

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
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::vector<ServerFieldType> field_types;
  for (size_t i = 0; i < forms[0]->field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms),
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HTML_MODE_NONE},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HTML_MODE_NONE},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_LINE2},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_STREET_ADDRESS}}});
}

// Test that we log the state/country fields whose server types are rationalized
TEST_F(AutofillMetricsTest, LogRepeatedStateCountryTypeRationalized) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

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
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::vector<ServerFieldType> field_types;
  for (size_t i = 0; i < forms[0]->field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           ADDRESS_HOME_COUNTRY);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms),
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
         form_signature.value()},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HTML_MODE_NONE},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_COUNTRY},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_COUNTRY}},
       {{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         form_signature.value()},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HTML_MODE_NONE},
        {UkmLogRepeatedServerTypePredictionRationalized::
             kFieldNewOverallTypeName,
         ADDRESS_HOME_STATE},
        {UkmLogRepeatedServerTypePredictionRationalized::kServerTypeName,
         ADDRESS_HOME_COUNTRY}},
       {{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         form_signature.value()},
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
         HTML_TYPE_UNSPECIFIED},
        {UkmLogRepeatedServerTypePredictionRationalized::kHtmlFieldModeName,
         HTML_MODE_NONE},
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Name", "name", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Address", "address", "3734 Elvis Presley Blvd.",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  test::CreateTestFormField("Phone", "phone", "2345678901", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  field.is_autofilled = false;

  // Below are fields with only_fill_when_focused set to true.
  // RATIONALIZATION_BAD because it's filled with same
  // value as filled previously.
  test::CreateTestFormField("Phone1", "phone1", "2345678901", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  base::UserActionTester user_action_tester;
  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  // Trigger phone number rationalization at filling time.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  // TRUE_POSITIVE + no rationalization logging
  test::CreateTestFormField("Name", "name", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  // TRUE_POSITIVE + no rationalization logging
  test::CreateTestFormField("Address", "address", "3734 Elvis Presley Blvd.",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  // TRUE_POSITIVE + no rationalization logging
  test::CreateTestFormField("Phone", "phone", "2345678901", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  field.is_autofilled = false;

  // Below are fields with only_fill_when_focused set to true.
  // TRUE_NEGATIVE_EMPTY + RATIONALIZATION_GOOD
  test::CreateTestFormField("Phone1", "phone1", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  // TRUE_POSITIVE + RATIONALIZATION_BAD
  test::CreateTestFormField("Phone2", "phone2", "2345678901", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  // FALSE_NEGATIVE_MISMATCH + RATIONALIZATION_OK
  test::CreateTestFormField("Phone3", "phone3", "Elvis Aaron Presley", "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  base::UserActionTester user_action_tester;
  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);
  std::string guid(kTestGuid);
  // Trigger phone number rationalization at filling time.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Autofill_FilledProfileSuggestion"));

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  std::vector<ServerFieldType> heuristic_types, server_types, actual_types;
  AutofillField field;

  // Add a first name field, that is predicted correctly.
  test::CreateTestFormField("first", "first", ValueForType(NAME_FIRST), "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FIRST);
  server_types.push_back(NAME_FIRST);
  actual_types.push_back(NAME_FIRST);

  // Add a last name field, that is predicted correctly.
  test::CreateTestFormField("last", "last", ValueForType(NAME_LAST), "test",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_LAST);
  server_types.push_back(NAME_LAST);
  actual_types.push_back(NAME_LAST);

  // Add an empty or unknown field, that is predicted as per the test params.
  test::CreateTestFormField("Unknown", "Unknown",
                            ValueForType(actual_field_type), "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(predicted_type == NO_SERVER_DATA ? UNKNOWN_TYPE
                                                             : predicted_type);
  server_types.push_back(predicted_type);

  // Resolve any field type ambiguity.
  if (actual_field_type == AMBIGUOUS_TYPE) {
    if (predicted_type == COMPANY_NAME || predicted_type == NAME_MIDDLE)
      actual_field_type = predicted_type;
  }
  actual_types.push_back(actual_field_type);

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Run the form submission code while tracking the histograms.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
    histogram_tester.ExpectBucketCount(
        crossed_histogram, FieldTypeCross(NAME_FIRST, NAME_FIRST), 1);
    histogram_tester.ExpectBucketCount(crossed_histogram,
                                       FieldTypeCross(NAME_LAST, NAME_LAST), 1);
    histogram_tester.ExpectBucketCount(
        crossed_histogram,
        FieldTypeCross((predicted_type == NO_SERVER_DATA && source != "Server"
                            ? UNKNOWN_TYPE
                            : predicted_type),
                       actual_field_type),
        1);
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

  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);

  // Simulate a OnFormsSeen() call that should trigger the recording.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});

  // Because these metrics are related to timing, it is not possible to know in
  // advance which bucket the sample will fall into, so we just need to make
  // sure we have valid samples.
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.DetermineHeuristicTypes")
          .empty());
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.ParseForm").empty());
}

// Test that we log quality metrics appropriately when an upload is triggered
// but no submission event is sent.
TEST_F(AutofillMetricsTest, QualityMetrics_NoSubmission) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate text input on one of the fields.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // Trigger a form upload and metrics by Resetting the manager.
  base::HistogramTester histogram_tester;

  autofill_manager().Reset();

  // Heuristic predictions.
  {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate.Heuristic.NoSubmission";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType.Heuristic.NoSubmission";
    // False Negative:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FULL, AutofillMetrics::TRUE_POSITIVE),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            EMAIL_ADDRESS, AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);
    // False Positives:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FULL, AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            PHONE_HOME_NUMBER, AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }

  // Server predictions override heuristics, so server and overall will be the
  // same.
  for (const std::string source : {"Server", "Overall"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source + ".NoSubmission";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source +
        ".NoSubmission";

    // Unknown.
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            ADDRESS_HOME_COUNTRY, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            EMAIL_ADDRESS, AutofillMetrics::TRUE_POSITIVE),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            PHONE_HOME_WHOLE_NUMBER, AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FULL, AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);

    // False Positives:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            NAME_FIRST, AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            EMAIL_ADDRESS, AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }
}

// Test that we log quality metrics for heuristics and server predictions based
// on autocomplete attributes present on the fields.
TEST_F(AutofillMetricsTest, QualityMetrics_BasedOnAutocomplete) {
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  FormFieldData field;
  // Heuristic value will match with Autocomplete attribute.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "additional-name";
  form.fields.push_back(field);

  // Heuristic value will be unknown.
  test::CreateTestFormField("Garbage label", "garbage", "", "text", &field);
  field.autocomplete_attribute = "postal-code";
  form.fields.push_back(field);

  // No autocomplete attribute. No metric logged.
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  std::unique_ptr<TestFormStructure> form_structure =
      std::make_unique<TestFormStructure>(form);
  TestFormStructure* form_structure_ptr = form_structure.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  ASSERT_TRUE(
      autofill_manager()
          .mutable_form_structures_for_test()
          ->emplace(form_structure_ptr->global_id(), std::move(form_structure))
          .second);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // Server response will match with autocomplete.
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_LAST);
  // Server response will NOT match with autocomplete.
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NAME_FIRST);
  // Server response will have no data.
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], NO_SERVER_DATA);
  // Not logged.
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NAME_MIDDLE);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  autofill_manager().OnLoadedServerPredictionsForTest(
      response_string, test::GetEncodedSignatures(*form_structure_ptr));

  // Verify that FormStructure::ParseApiQueryResponse was called (here and
  // below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  // Heuristic value will match with Autocomplete attribute.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_LAST);
  server_types.push_back(NAME_LAST);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FIRST);
  server_types.push_back(NAME_FIRST);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("Payment Address", "payment_address", "user@upi",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  std::vector<ServerFieldType> heuristic_types, server_types;

  FormFieldData field;
  test::CreateTestFormField("Both match", "match", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);
  test::CreateTestFormField("Both mismatch", "mismatch", "buddy@gmail.com",
                            "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField("Only heuristics match", "mixed", "Memphis", "text",
                            &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_CITY);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Add a field and re-arrange the remaining form fields before submitting.
  std::vector<FormFieldData> cached_fields = form.fields;
  form.fields.clear();
  test::CreateTestFormField("New field", "new field", "Tennessee", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(cached_fields[2]);
  form.fields.push_back(cached_fields[1]);
  form.fields.push_back(cached_fields[3]);
  form.fields.push_back(cached_fields[0]);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Construct a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // An autofillable form was submitted, and the number of stored profiles is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 2, 1);
}

// Verify that when submitting a non-autofillable form, the stored profile
// metric is not logged.
TEST_F(AutofillMetricsTest, StoredProfileCountNonAutofillableFormSubmission) {
  // Construct a non-fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  // Two fields is not enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Verify that when submitting an autofillable form, the proper tppe of
// the edited fields is correctly logged to UKM.
TEST_F(AutofillMetricsTest, TypeOfEditedAutofilledFieldsUkmLogging) {
  // Construct a fillable form.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("https://example.test/form.html");
  form.action = GURL("https://example.test/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://example.test/form.html"));

  std::vector<ServerFieldType> heuristic_types, server_types;

  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Verify that there are no counts before form submission.

  EXPECT_EQ(0U, test_ukm_recorder_->entries_count());

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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

// Verify that when submitting an autofillable form, the proper tppe of
// the edited fields is correctly logged to UMA.
TEST_F(AutofillMetricsTest, TypeOfEditedAutofilledFieldsUmaLogging) {
  // Construct a fillable form.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  std::vector<ServerFieldType> heuristic_types, server_types;

  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());
  autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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
  // Construct a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  std::vector<ServerFieldType> heuristic_types, server_types;

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());
  autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // An autofillable form was submitted, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission", 2, 1);
}

// Verify that when resetting the autofill manager (such as during a
// navigation), the proper number of edited fields is logged.
TEST_F(AutofillMetricsTest, NumberOfEditedAutofilledFields_NoSubmission) {
  // Construct a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  std::vector<ServerFieldType> heuristic_types, server_types;

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first field.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // We expect metrics to be logged when the manager is reset.
  autofill_manager().Reset();

  // An autofillable form was uploaded, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission", 1, 1);
}

// Verify that we correctly log metrics regarding developer engagement.
TEST_F(AutofillMetricsTest, DeveloperEngagement) {
  // Start with a non-fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Ensure no metrics are logged when small form support is disabled (min
  // number of fields enforced).
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();
    histogram_tester.ExpectTotalCount("Autofill.DeveloperEngagement", 0);
  }

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Expect the "form parsed without hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
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
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "email";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  // Expect the "form parsed with field type hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);

    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 0);
  }

  // Add a field with an author-specified UPI-VPA field type in the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "upi-vpa";
  forms.back().fields.push_back(field);

  // Expect the "form parsed with type hints" metric, and the
  // "author-specified upi-vpa type" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 1);
  }
}

// Verify that we correctly log UKM for form parsed without type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithoutTypeHints) {
  // Start with a non-fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Ensure no entries are logged when loading a non-fillable form.
  {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();

    EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
  }

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, forms.back(), /*is_for_credit_card=*/false,
        {FormType::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithTypeHints) {
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "email";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().Reset();

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, forms.back(), /*is_for_credit_card=*/false,
        {FormType::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest, UkmDeveloperEngagement_LogUpiVpaTypeHint) {
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Payment", "payment", "", "text", &field);
  field.autocomplete_attribute = "upi-vpa";
  form.fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  {
    SCOPED_TRACE("VPA and other autocomplete hint present");
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, forms.back(), /*is_for_credit_card=*/false,
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

TEST_F(AutofillMetricsTest, LogStoredOfferMetrics) {
  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  AutofillOfferData offer1 = test::GetCardLinkedOfferData1();
  AutofillOfferData offer2 = test::GetCardLinkedOfferData2();
  offer2.eligible_instrument_id.emplace_back(999999);
  offer2.eligible_instrument_id.emplace_back(888888);
  offer2.merchant_origins.emplace_back("https://www.example3.com/");
  offers.push_back(std::make_unique<AutofillOfferData>(offer1));
  offers.push_back(std::make_unique<AutofillOfferData>(offer2));

  base::HistogramTester histogram_tester;
  auto SamplesOf = [&histogram_tester](base::StringPiece metric) {
    return histogram_tester.GetAllSamples(metric);
  };
  AutofillMetrics::LogStoredOfferMetrics(offers);

  // Validate the count metrics.
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferCount"),
              BucketsAre(Bucket(2, 1)));
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferRelatedMerchantCount"),
              BucketsAre(Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferRelatedCardCount"),
              BucketsAre(Bucket(1, 1), Bucket(3, 1)));
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  field_types.push_back(EMAIL_ADDRESS);
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  form.fields.push_back(field);
  field_types.push_back(PHONE_HOME_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }

  {
    // Simulate activating the autofill popup for the email field after typing.
    // No new metric should be logged, since we're still on the same page.
    test::CreateTestFormField("Email", "email", "b", "email", &field);
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 1,
                                        1);
  }

  // Reset the autofill manager state again.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after a fill.
    form.fields[0].is_autofilled = true;
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 1);
  }
}

// Test that we log the correct number of Company Name Autofill suggestions when
// filling a form.
TEST_F(AutofillMetricsTest, CompanyNameSuggestions) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  field_types.push_back(EMAIL_ADDRESS);
  test::CreateTestFormField("Company", "company", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(COMPANY_NAME);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);

    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }
}

// Test that the credit card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, CreditCardCheckoutFlowUserActions) {
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Name on card" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    external_delegate_->OnQuery(0, form, form.fields.front(), gfx::RectF());
    external_delegate_->DidAcceptSuggestion(
        u"Test", autofill_manager().MakeFrontendIDForTest(guid, std::string()),
        guid, 0);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field along with a "Clear form" footer suggestion.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a "Clear form" suggestion.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    external_delegate_->OnQuery(0, form, form.fields.front(), gfx::RectF());
    external_delegate_->DidAcceptSuggestion(
        std::u16string(), POPUP_ITEM_ID_CLEAR_FORM, std::string(), 0);
    EXPECT_EQ(1, user_action_tester.GetActionCount("Autofill_ClearedForm"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field, this time to submit the form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    external_delegate_->OnQuery(0, form, form.fields.front(), gfx::RectF());
    external_delegate_->DidAcceptSuggestion(
        u"Test", autofill_manager().MakeFrontendIDForTest(guid, std::string()),
        guid, 0);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a credit card suggestion.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledCreditCardSuggestion"));
  }

  // Simulate submitting the credit card form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
      {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
      {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NAME_FULL},
      {UkmSuggestionsShownType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
      {UkmSuggestionsShownType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  ExpectedUkmMetricsRecord number_field_record{
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
      {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
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
                      /*is_for_credit_card=*/true, /* has_upi_vpa_field=*/false,
                      {FormType::kCreditCardForm}, {0, 3, 0});
}

// Test that the UPI Checkout flow form submit is correctly logged
TEST_F(AutofillMetricsTest, UpiVpaUkmTest) {
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());
  FormFieldData field;
  test::CreateTestFormField("Enter VPA", "upi-vpa", "unique_id@upi", "text",
                            &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});

    VerifySubmitFormUkm(test_ukm_recorder_, forms.back(),
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /* has_upi_vpa_field */ true,
                        /* UPI VPA has Unknown form type.*/
                        {FormType::kAddressForm, FormType::kUnknownFormType});
    PurgeUKM();
  }
}

// Test that the profile checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, ProfileCheckoutFlowUserActions) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a profile field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "State" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "City" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate selecting a profile suggestions.
  {
    base::UserActionTester user_action_tester;
    std::string guid(kTestGuid);  // local profile.
    external_delegate_->OnQuery(0, form, form.fields.front(), gfx::RectF());
    external_delegate_->DidAcceptSuggestion(
        u"Test", autofill_manager().MakeFrontendIDForTest(std::string(), guid),
        guid, 0);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a profile suggestion.
  {
    base::UserActionTester user_action_tester;
    std::string guid(kTestGuid);  // local profile.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledProfileSuggestion"));
  }

  // Simulate submitting the profile form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));
  }

  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_STATE},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_STATE},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_CITY},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
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
  VerifySubmitFormUkm(
      test_ukm_recorder_, form, AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
      /*is_for_credit_card=*/false,
      /* has_upi_vpa_field=*/false, {FormType::kAddressForm}, {0, 2, 0});
}

// Tests that the Autofill_PolledCreditCardSuggestions user action is only
// logged once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledCreditCardSuggestions_DebounceLogs) {
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up the form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a second query on the same field. There should still only be one
  // logged poll.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[1], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));
}

// Tests that the Autofill.QueriedCreditCardFormIsSecure histogram is logged
// properly.
TEST_F(AutofillMetricsTest, QueriedCreditCardFormIsSecure) {
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up the form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

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
    autofill_manager().AddSeenForm(form, field_types, field_types);

    // Simulate an Autofill query on a credit card field (HTTP, non-secure
    // form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, form.fields[1], gfx::RectF(),
        /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", false, 1);
    // Reset the main frame origin to secure for other tests
    autofill_client_->set_form_origin(client_form_origin);
  }

  {
    // Simulate having seen this secure form on page load.
    autofill_manager().Reset();
    form.host_frame = test::MakeLocalFrameToken();
    form.unique_renderer_id = test::MakeFormRendererId();
    form.url = GURL("https://example.com/form.html");
    form.action = GURL("https://example.com/submit.html");
    form.main_frame_origin =
        url::Origin::Create(autofill_client_->form_origin());
    autofill_manager().AddSeenForm(form, field_types, field_types);

    // Simulate an Autofill query on a credit card field (HTTPS form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, form.fields[1], gfx::RectF(),
        /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", true, 1);
  }
}

// Tests that the Autofill_PolledProfileSuggestions user action is only logged
// once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledProfileSuggestions_DebounceLogs) {
  RecreateProfile(/*is_server=*/false);

  // Set up the form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a profile field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a second query on the same field. There should still only be poll
  // logged.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[1], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));
}

// Test that we log parsed form event for credit card forms.
TEST_P(AutofillMetricsIFrameTest, CreditCardParsedFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Card Number", "card_number", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Expiration", "cc_exp", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Verification", "verification", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_VERIFICATION_CODE);

  // Simulate seeing and parsing the form.
  std::vector<FormData> forms;
  forms.push_back(form);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that we log interacted form event for credit cards related.
TEST_P(AutofillMetricsIFrameTest, CreditCardInteractedFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnAskForValuesToFill(
        1, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardPopupSuppressedFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating popup being suppressed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_POPUP_SUPPRESSED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_POPUP_SUPPRESSED, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating popup being suppressed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, field);
    autofill_manager().DidSuppressPopup(form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_POPUP_SUPPRESSED, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_POPUP_SUPPRESSED, 2);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardShownFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
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
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableMerchantBoundVirtualCards);

  // Set up our form data.  https:// is required.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_VERIFICATION_CODE);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Creating cards, including a virtual card.
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      true /* include_virtual_credit_card */);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 2);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0);
  }

  // Recreate cards, this time *without* a virtual card.
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* include_virtual_credit_card */);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load. Suggestions shown should be
    // logged, but suggestions shown with virtual card should not.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0);
  }
}

// Test that we log selected form event for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardSelectedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields[2],
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a masked server card multiple times.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields[2],
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields[2],
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a virtual server suggestion by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000002");  // masked card
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a virtual card multiple times.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000002");  // masked card
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields[2]);
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1);
  }
}

// Test that we log filled form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardFilledFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a virtual card suggestion by selecting the option
    // based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000002");  // masked card
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a full card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
                                       1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
  }
}

// Test that we log preflight calls for credit card unmasking.
TEST_F(AutofillMetricsTest, CreditCardUnmaskingPreflightCall) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillCreditCardAuthentication);
  std::string preflight_call_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightCalled";
  std::string preflight_latency_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";

  // Set up our form data.
  FormData form;
  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Create local cards and set user as eligible for FIDO authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(true /* include_local_credit_card */,
                        false /* include_masked_server_credit_card */,
                        false /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    // If no masked server cards are available, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create masked server cards and set user as ineligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(false /* include_local_credit_card */,
                        true /* include_masked_server_credit_card */,
                        false /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);
    SetFidoEligibility(false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    // If user is not verifiable, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create full server cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(false /* include_local_credit_card */,
                        false /* include_masked_server_credit_card */,
                        true /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);
    SetFidoEligibility(false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    // If no masked server cards are available, then no preflight call is made.
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
  }

  {
    // Create masked server cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(false /* include_local_credit_card */,
                        true /* include_masked_server_credit_card */,
                        false /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    // Preflight call is made only if a masked server card is available and the
    // user is eligible for FIDO authentication (except iOS).
#if BUILDFLAG(IS_IOS)
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
#else
    histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 1);
#endif
  }

  {
    // Create all types of cards and set user as eligible for FIDO
    // authentication.
    base::HistogramTester histogram_tester;
    RecreateCreditCards(true /* include_local_credit_card */,
                        true /* include_masked_server_credit_card */,
                        true /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);
    SetFidoEligibility(true);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    // Preflight call is made only if a masked server card is available and the
    // user is eligible for FIDO authentication (except iOS).
#if BUILDFLAG(IS_IOS)
    histogram_tester.ExpectTotalCount(preflight_call_metric, 0);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 0);
#else
    histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
    histogram_tester.ExpectTotalCount(preflight_latency_metric, 1);
#endif
  }
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_ServerCard) {
  // Creating masked card
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Success", 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  // Creating masked card
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
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
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);
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

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 1);
}

TEST_P(AutofillMetricsIFrameTest,
       CreditCardSubmittedWithoutSelectingSuggestionsWrongSizeCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "411111111", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "4444444444444444", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "5105105105105100", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "4111111111111111", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "4111111111111111", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown and selected.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  std::string guid("10000000-0000-0000-0000-000000000001");
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
      autofill_manager().MakeFrontendIDForTest(guid, std::string()));

  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD, 0);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0);
  histogram_tester.ExpectBucketCount(
      credit_card_form_events_frame_histogram_,
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0);
}

TEST_F(AutofillMetricsTest, ShouldNotLogFormEventNoCardForAddressForm) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulating submission with no filled data.
  base::HistogramTester histogram_tester;
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0);
}

// Test that we log submitted form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardSubmittedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager().Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);

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
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {0, 1, 0});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid("10000000-0000-0000-0000-000000000002");  // masked card
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1);

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
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {0, 1, 0});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);

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
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {0, 1, 0});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);

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
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm}, {0, 1, 0});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});

    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
          {UkmFormSubmittedType::kAutofillFillsName, 0},
          {UkmFormSubmittedType::kAutocompleteFillsName, 0}},
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
          {UkmFormSubmittedType::kAutofillFillsName, 0},
          {UkmFormSubmittedType::kAutocompleteFillsName, 0}}});

    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       0);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);

    VerifyUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/true,
                        /* has_upi_vpa_field=*/false,
                        {FormType::kCreditCardForm});
  }
}

// Test that we log "will submit" and "submitted" form events for credit
// cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardWillSubmitFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid("10000000-0000-0000-0000-000000000002");  // masked card
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, guid, kDefaultPageID, form,
        form.fields.front());
    OnCreditCardFetchingSuccessful(u"6011000990139424",
                                   /*is_virtual_card=*/true);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    // Full server card.
    std::string guid("10000000-0000-0000-0000-000000000003");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnCreditCardFetchingSuccessful(u"6011000990139424");
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      true /* masked_card_is_enrolled_for_virtual_card */);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       0);
    histogram_tester.ExpectBucketCount(credit_card_form_events_frame_histogram_,
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        credit_card_form_events_frame_histogram_,
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 0);
  }
}

// Test that we log form events for masked server card with offers.
TEST_F(AutofillMetricsTest, LogServerOfferFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Creating all kinds of cards. None of them have offers.
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
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
  std::string guid("12340000-0000-0000-0000-000000000001");
  AddMaskedServerCreditCardWithOffer(guid, "$4",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x4fff);
  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        IsSupersetOf(
            {Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
             Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                    1)}));

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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    // Select another card, and still log to offer
    // sub-histogram because user has another masked server card with offer.
    guid = "10000000-0000-0000-0000-000000000002";
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 1);

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
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);
  guid = "12340000-0000-0000-0000-000000000002";
  AddMaskedServerCreditCardWithOffer(guid, "$4",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x3fff, /*offer_expired=*/true);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating activating the autofill popup for the credit card field,
    // new popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    // Select the card with linked offer, though metrics should not record it
    // since the offer is expired.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    // Histograms without ".WithOffer" should be recorded.
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 1);

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
  RecreateCreditCards(true /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);
  guid = "12340000-0000-0000-0000-000000000003";
  AddMaskedServerCreditCardWithOffer(guid, "$5",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x5fff);

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, showing the
    // suggestions again, and then submitting the form with previously filled
    // card. Verify that all related form events are correctly logged to offer
    // sub-histogram. Making suggestions reappear tests confirmation of a fix
    // for crbug/1198751.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");

    // Simulate user showing suggestions but then submitting form with
    // previously filled card info.
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 1);

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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // failing the CVC check and submitting the form anyways. Verify that all
    // related form events are correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    // Select the masked server card with the linked offer, but fail the CVC
    // check.
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kPermanentFailure,
                    std::string());

    // Submitting the form without the filled suggestion.
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOffer",
        FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0);

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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // selecting a local card instead. Verify that all related form events are
    // correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;

    // Show suggestions and select the card with offer.
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                    "6011000990139424");

    // Show suggestions again, and select a local card instead.
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    guid = "10000000-0000-0000-0000-000000000001";
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.back(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        IsSupersetOf(
            {Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
             Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
             Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                    1)}));

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

// Test that we log parsed form events for address and cards in the same form.
TEST_F(AutofillMetricsTest, MixedParsedFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);
  test::CreateTestFormField("Card Number", "card_number", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Expiration", "cc_exp", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Verification", "verification", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_VERIFICATION_CODE);

  // Simulate seeing and parsing the form.
  std::vector<FormData> forms;
  forms.push_back(form);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address.WithNoData",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that we log parsed form events for address.
TEST_F(AutofillMetricsTest, AddressParsedFormEvents) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate seeing and parsing the form.
  std::vector<FormData> forms;
  forms.push_back(form);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnAskForValuesToFill(
        1, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_POPUP_SUPPRESSED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);

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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidSuppressPopup(form, field);
    autofill_manager().DidSuppressPopup(form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_POPUP_SUPPRESSED, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_POPUP_SUPPRESSED_ONCE, 1);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(kTestGuid);  // local profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion more than once.
    base::HistogramTester histogram_tester;
    std::string guid(kTestGuid);  // local profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
                                       1);
  }

  // Create a server profile and reset the autofill manager state.
  RecreateProfile(/*is_server=*/true);
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate selecting/filling a server profile suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(kTestGuid);  // server profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
                                       1);
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulate selecting/filling a server profile suggestion more than once.
    base::HistogramTester histogram_tester;
    std::string guid(kTestGuid);  // server profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
                                       1);
  }
}

// Test that we log submitted form events for address.
TEST_F(AutofillMetricsTest, AddressSubmittedFormEvents) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /* has_upi_vpa_field=*/false, {FormType::kAddressForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager().Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
                        /*is_for_credit_card=*/false,
                        /* has_upi_vpa_field=*/false, {FormType::kAddressForm});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid(kTestGuid);  // local profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion show but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                       FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
                                       1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    std::string guid(kTestGuid);  // local profile
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
  autofill_manager().AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  form.fields.push_back(field);
  field_types.push_back(PHONE_HOME_NUMBER);
  autofill_manager().AddSeenForm(form, field_types, field_types);

  base::HistogramTester histogram_tester;
  autofill_manager().OnAskForValuesToFill(
      0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_INTERACTED_ONCE, 1);
}

// Test that we log interacted form event for credit cards only once.
TEST_F(AutofillMetricsTest, CreditCardFormEventsAreSegmented) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateCreditCards(false /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateCreditCards(false /* include_local_credit_card */,
                      true /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateCreditCards(false /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      true /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log interacted form event for address only once.
TEST_F(AutofillMetricsTest, AddressFormEventsAreSegmented) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);
  personal_data().ClearProfiles();

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);
  RecreateProfile(/*is_server=*/false);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
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
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"First Last",
      structured_address::VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"First", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Last", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND, u"Last",
      structured_address::VerificationStatus::kParsed);

  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(profile);

  std::string base_histo =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(
      base_histo + "Full", structured_address::VerificationStatus::kObserved,
      1);
  histogram_tester.ExpectUniqueSample(
      base_histo + "First", structured_address::VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(
      base_histo + "Last", structured_address::VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(
      base_histo + "SecondLast",
      structured_address::VerificationStatus::kParsed, 1);

  histogram_tester.ExpectTotalCount(base_histo + "Middle", 0);
  histogram_tester.ExpectTotalCount(base_histo + "FirstLast", 0);

  histogram_tester.ExpectTotalCount(base_histo + "Any", 4);
  histogram_tester.ExpectBucketCount(
      base_histo + "Any", structured_address::VerificationStatus::kObserved, 1);
  histogram_tester.ExpectBucketCount(
      base_histo + "Any", structured_address::VerificationStatus::kParsed, 3);
}

// Test that we log the verification status of address tokens..
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfAddressTokens) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"123 StreetName",
      structured_address::VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123",
      structured_address::VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"StreetName",
      structured_address::VerificationStatus::kObserved);

  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(profile);

  std::string base_histo =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(
      base_histo + "StreetAddress",
      structured_address::VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectUniqueSample(
      base_histo + "StreetName",
      structured_address::VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(
      base_histo + "HouseNumber",
      structured_address::VerificationStatus::kObserved, 1);

  histogram_tester.ExpectTotalCount(base_histo + "FloorNumber", 0);
  histogram_tester.ExpectTotalCount(base_histo + "ApartmentNumber", 0);
  histogram_tester.ExpectTotalCount(base_histo + "Premise", 0);
  histogram_tester.ExpectTotalCount(base_histo + "SubPremise", 0);

  histogram_tester.ExpectTotalCount(base_histo + "Any", 3);
  histogram_tester.ExpectBucketCount(
      base_histo + "Any", structured_address::VerificationStatus::kFormatted,
      1);
  histogram_tester.ExpectBucketCount(
      base_histo + "Any", structured_address::VerificationStatus::kObserved, 2);
}

// Verify that we correctly log the submitted form's state.
TEST_F(AutofillMetricsTest, AutofillFormSubmittedState) {
  // Start with a form with insufficiently many fields.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Unknown", "unknown", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);

  // Expect no notifications when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
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
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Non fillable form.
  form.fields[0].value = u"Unknown Person";
  form.fields[1].value = u"unknown.person@gmail.com";
  forms.front() = form;

  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
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
  forms.front() = form;

  // Autofilled none with no suggestions shown.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});

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
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
           HTML_TYPE_UNSPECIFIED},
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Mark one of the fields as autofilled.
  form.fields[1].is_autofilled = true;
  forms.front() = form;

  // Autofilled some of the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Mark all of the fillable fields as autofilled.
  form.fields[0].is_autofilled = true;
  form.fields[2].is_autofilled = true;
  forms.front() = form;

  // Autofilled all the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
    VerifyUkm(test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
              expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyUkm(test_ukm_recorder_, form, UkmFieldFillStatusType::kEntryName,
              expected_field_fill_status_ukm_metrics);
  }

  // Clear out the third field's value.
  form.fields[2].value = std::u16string();
  forms.front() = form;
}

// Verify that we correctly log the submitted form's state with fields
// having |only_fill_when_focused|=true.
TEST_F(
    AutofillMetricsTest,
    AutofillFormSubmittedState_DontCountUnfilledFieldsWithOnlyFillWhenFocused) {
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Billing Phone", "billing_phone", "", "text",
                            &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Verify if the form is otherwise filled with a field having
  // |only_fill_when_focused|=true, we consider the form is all filled.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
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

    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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
         {UkmFormSubmittedType::kAutofillFillsName, 0},
         {UkmFormSubmittedType::kAutocompleteFillsName, 0}});
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
  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_CreditCardForm) {
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  // Construct a valid credit card form.
  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Card Number", "card_number", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Expiration", "cc_exp", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Verification", "verification", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_VERIFICATION_CODE);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    SCOPED_TRACE("First seen");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    SCOPED_TRACE("Initial typing");
    base::HistogramTester histogram_tester;
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate suggestions shown twice with separate popups.
  {
    SCOPED_TRACE("Separate pop-ups");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, field);
    autofill_manager().DidShowSuggestions(true, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  autofill_manager().Reset();
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    SCOPED_TRACE("Multiple keystrokes");
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, field);
    autofill_manager().DidShowSuggestions(false, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
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
    autofill_manager().OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::USER_DID_AUTOFILL_ONCE,
                                       1);
  }

  // Simulate editing an autofilled field.
  {
    SCOPED_TRACE("Edit autofilled field");
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(guid, std::string()));
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
  }

  // Simulate invoking autofill again.
  {
    SCOPED_TRACE("Invoke autofill again");
    base::HistogramTester histogram_tester;
    autofill_manager().OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    SCOPED_TRACE("Edit another autofilled field");
    base::HistogramTester histogram_tester;
    autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                            TimeTicks());
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
  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  // Simulate suggestions shown twice with separate popups.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, field);
    autofill_manager().DidShowSuggestions(true, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  autofill_manager().Reset();
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, field);
    autofill_manager().DidShowSuggestions(false, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  // Simulate suggestions shown for a different field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  // Simulate invoking autofill.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::USER_DID_AUTOFILL_ONCE,
                                       1);
  }

  // Simulate editing an autofilled field.
  {
    base::HistogramTester histogram_tester;
    std::string guid(kTestGuid);
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), guid));
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
  }

  // Simulate invoking autofill again.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                            TimeTicks());
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
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[2])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
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
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
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
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmTextFieldDidChangeType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmTextFieldDidChangeType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kEmail)},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[1])).value()},
        {UkmTextFieldDidChangeType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
}

// Verify that we correctly log metrics tracking the duration of form fill.
// TODO(crbug.com/1009364) Test is flake on many builders.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  const std::vector<FormData> forms(1, form);

  // Fill additional form.
  FormData second_form = form;
  second_form.host_frame = test::MakeLocalFrameToken();
  second_form.unique_renderer_id = test::MakeFormRendererId();
  test::CreateTestFormField("Second Phone", "second_phone", "", "text", &field);
  second_form.fields.push_back(field);

  std::vector<FormData> second_forms(1, second_form);

  // Fill the field values for form submission.
  form.fields[0].value = u"Elvis Aaron Presley";
  form.fields[1].value = u"theking@gmail.com";
  form.fields[2].value = u"12345678901";

  // Fill the field values for form submission.
  second_form.fields[0].value = u"Elvis Aaron Presley";
  second_form.fields[1].value = u"theking@gmail.com";
  second_form.fields[2].value = u"12345678901";
  second_form.fields[3].value = u"51512345678";

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    SCOPED_TRACE("Test 1");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
    SCOPED_TRACE("Test 2");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
  form.fields[0].is_autofilled = true;
  {
    SCOPED_TRACE("Test 3");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    autofill_manager().OnDidFillAutofillFormData(
        form, parse_time + base::Microseconds(5));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
    SCOPED_TRACE("Test 4");
    base::HistogramTester histogram_tester;

    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    autofill_manager().OnDidFillAutofillFormData(
        form, parse_time + base::Microseconds(5));

    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
    SCOPED_TRACE("Test 5");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    autofill_manager().OnFormsSeen(/*updated_forms=*/second_forms,
                                   /*removed_forms=*/{});
    autofill_manager().OnDidFillAutofillFormData(
        form, parse_time + base::Microseconds(5));
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
    SCOPED_TRACE("Test 6");
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    autofill_manager().OnFormsSeen(/*updated_forms=*/second_forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time{};
    for (const auto& kv : autofill_manager().form_structures()) {
      if (kv.second->form_parsed_timestamp() > parse_time)
        parse_time = kv.second->form_parsed_timestamp();
    }
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(second_form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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
        {FormType::kCreditCardForm}, true /* used_autofill */,
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
        {FormType::kCreditCardForm}, false /* used_autofill */,
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
        {FormType::kUnknownFormType}, false /* used_autofill */,
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
        {FormType::kAddressForm}, true /* used_autofill */,
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
        {FormType::kAddressForm}, false /* used_autofill */,
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
        {FormType::kUnknownFormType}, false /* used_autofill */,
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
        {FormType::kPasswordForm}, true /* used_autofill */,
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
        {FormType::kPasswordForm}, false /* used_autofill */,
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
        {FormType::kUnknownFormType}, false /* used_autofill */,
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
        {FormType::kUnknownFormType}, true /* used_autofill */,
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
        {FormType::kUnknownFormType}, false /* used_autofill */,
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
        {FormType::kAddressForm}, false /* used_autofill */,
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
        true /* used_autofill */, base::Milliseconds(2000));
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
        false /* used_autofill */, base::Milliseconds(2000));
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

  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  // Create the form's fields.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip", "zip", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Organization", "organization", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Fill second form.
  FormData second_form = form;
  std::vector<FormData> second_forms(1, second_form);

  // Fill a third form.
  FormData third_form = form;
  std::vector<FormData> third_forms(1, third_form);

  // Fill a fourth form.
  FormData fourth_form = form;
  std::vector<FormData> fourth_forms(1, fourth_form);

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
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 0),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log EXISTING_PROFILE_USED for the metric since the same profile
  // is submitted.
  autofill_manager().OnFormsSeen(/*updated_forms=*/second_forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(second_form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log NEW_PROFILE_CREATED for the metric since a new profile is
  // submitted.
  autofill_manager().OnFormsSeen(/*updated_forms=*/third_forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(third_form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileActionOnFormSubmitted"),
      BucketsAre(Bucket(AutofillMetrics::NEW_PROFILE_CREATED, 2),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_USED, 1),
                 Bucket(AutofillMetrics::EXISTING_PROFILE_UPDATED, 0)));

  // Expect to log EXISTING_PROFILE_UPDATED for the metric since the profile was
  // updated.
  autofill_manager().OnFormsSeen(/*updated_forms=*/fourth_forms,
                                 /*removed_forms=*/{});
  autofill_manager().OnFormSubmitted(fourth_form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);
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
  std::vector<std::unique_ptr<FormStructure>> owned_forms_;
  std::vector<FormStructure*> forms_;
};

namespace {
void AddFieldSuggestionToForm(
    AutofillQueryResponse_FormSuggestion* form_suggestion,
    FieldSignature field_signature,
    int field_type) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(field_signature.value());
  field_suggestion->add_predictions()->set_type(field_type);
}
}  // namespace

TEST_F(AutofillMetricsParseQueryResponseTest, ServerHasData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(0)->GetFieldSignature(), 7);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(1)->GetFieldSignature(), 30);
  form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(0)->GetFieldSignature(), 9);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(1)->GetFieldSignature(), 0);

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
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(0)->GetFieldSignature(), 0);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(1)->GetFieldSignature(), 0);
  form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(0)->GetFieldSignature(), 9);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(1)->GetFieldSignature(), 0);
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
      AddFieldSuggestionToForm(
          form_suggestion,
          forms_[form_idx]->field(field_idx)->GetFieldSignature(), 0);
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
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(0)->GetFieldSignature(), 0);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[0]->field(1)->GetFieldSignature(), 10);
  form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(0)->GetFieldSignature(), 0);
  AddFieldSuggestionToForm(form_suggestion,
                           forms_[1]->field(1)->GetFieldSignature(), 11);

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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  GURL frame_origin("http://example_root.com/form.html");
  form.main_frame_origin = url::Origin::Create(frame_origin);
  autofill_client_->set_form_origin(frame_origin);

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.OnNonsecurePage",
        FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
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
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);

  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  // Form action needs to be secure on secure page, otherwise this triggers
  // mixed form warnings and no suggestions are offered.
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("http://example_root.com/form.html"));

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by `autofill_manager()`.
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFill(
        0, form, field, gfx::RectF(), /*autoselect_first_suggestion=*/false);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    // Check that the nonsecure histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(
        0, histograms.GetTotalCountsForPrefix("Autofill.FormEvents.CreditCard")
               ["Autofill.FormEvents.CreditCard.OnNonsecurePage"]);
  }
}

// Tests that logging CardUploadDecision UKM works as expected.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric) {
  GURL url("https://www.google.com");
  int upload_decision = 1;
  autofill_client_->set_form_origin(url);

  AutofillMetrics::LogCardUploadDecisionsUkm(test_ukm_recorder_,
                                             autofill_client_->GetUkmSourceId(),
                                             url, upload_decision);
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
  AutofillMetrics::LogCardUploadDecisionsUkm(test_ukm_recorder_, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder_->sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
}

// Tests that no UKM is logged when the ukm service is null.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_NoUkmService) {
  GURL url("https://www.google.com");
  test_ukm_recorder_->Purge();
  AutofillMetrics::LogCardUploadDecisionsUkm(nullptr, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder_->sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder_->entries_count());
}

// Test the ukm recorded when Suggestion is shown.
//
// Flaky on all platforms. TODO(crbug.com/876897): Fix it.
TEST_F(AutofillMetricsTest, DISABLED_AutofillSuggestionShownTest) {
  RecreateCreditCards(true /* include_local_credit_card */,
                      false /* include_masked_server_credit_card */,
                      false /* include_full_server_credit_card */,
                      false /* masked_card_is_enrolled_for_virtual_card */);
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example_cc.com/form.html");
  form.action = GURL("http://example_cc.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // Simulate and Autofill query on credit card name field.
  autofill_manager().DidShowSuggestions(true /* is_new_popup */, form,
                                        form.fields[0]);
  VerifyUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionsShownType::kMillisecondsSinceFormParsedName, 0},
        {UkmSuggestionsShownType::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmSuggestionsShownType::kServerTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
}

TEST_F(AutofillMetricsTest, DynamicFormMetrics) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate seeing.
  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types, field_types);
  std::string guid(kTestGuid);

  // Simulate checking whether to fill a dynamic form before the form was filled
  // initially.
  FormStructure form_structure(form);
  autofill_manager().ShouldTriggerRefillForTest(form_structure);
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.Address", 0);

  // Simulate filling the form.
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

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
  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate seeing the form.
  {
    base::HistogramTester histogram_tester;
    autofill_client_->set_security_level(
        security_state::SecurityLevel::DANGEROUS);
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address.DANGEROUS",
        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate suggestions shown twice with separate popups.
  {
    base::HistogramTester histogram_tester;
    autofill_client_->set_security_level(
        security_state::SecurityLevel::WARNING);
    autofill_manager().DidShowSuggestions(true, form, field);
    autofill_manager().DidShowSuggestions(true, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address.WARNING",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address.WARNING",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
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
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;

  std::vector<FormData> forms_with_one_time_code(1, form);

  test::CreateTestFormField("", "", "", "password", &field);
  field.autocomplete_attribute = "one-time-code";
  forms_with_one_time_code.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "password", &field);
  forms_with_one_time_code.back().fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_with_one_time_code, /*removed_forms=*/{});
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
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<FormData> forms_without_one_time_code(1, form);

  test::CreateTestFormField("", "", "", "password", &field);
  forms_without_one_time_code.back().fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_without_one_time_code, /*removed_forms=*/{});
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
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;

  std::vector<FormData> forms_with_phone_number(1, form);

  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  forms_with_phone_number.back().fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  forms_with_phone_number.back().fields.push_back(field);
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  forms_with_phone_number.back().fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_with_phone_number, /*removed_forms=*/{});
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
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<FormData> forms_with_single_phone_number_field(1, form);

  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  forms_with_single_phone_number_field.back().fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_with_single_phone_number_field,
      /*removed_forms=*/{});
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
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("phone", form);
  std::vector<FormData> forms_with_phone_number(1, form);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_with_phone_number, /*removed_forms=*/{});
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
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<FormData> forms_without_phone_number(1, form);

  test::CreateTestFormField("", "", "", "password", &field);
  forms_without_phone_number.back().fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/forms_without_phone_number, /*removed_forms=*/{});
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
// Verify that we correctly log PhoneCollectionMetricState::kNone.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateNone) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("password", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(false);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kNone, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kOTC.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateOTC) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("one-time-code", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(false);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kOTC, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kWebOTP.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateWebOTP) {
  // If WebOTP is used, even if there is no form on the page we still need to
  // report it.
  base::HistogramTester histogram_tester;
  autofill_manager().ReportAutofillWebOTPMetrics(true);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kWebOTP, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kWebOTPPlusOTC.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateWebOTPPlusOTC) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("one-time-code", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(true);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kWebOTPPlusOTC,
                                     1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kPhone.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStatePhone) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("tel", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(false);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kPhone, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kPhonePlusOTC.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStatePhonePlusOTC) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("tel", form);
  AddAutoCompleteFieldToForm("one-time-code", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(false);
  histogram_tester.ExpectBucketCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                     PhoneCollectionMetricState::kPhonePlusOTC,
                                     1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log PhoneCollectionMetricState::kPhonePlusWebOTP.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStatePhonePlusWebOTP) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("tel", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(true);
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      PhoneCollectionMetricState::kPhonePlusWebOTP, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that we correctly log
// PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC.
TEST_F(AutofillMetricsTest,
       WebOTPPhoneCollectionMetricsStatePhonePlusWebOTPPlusOTC) {
  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  AddAutoCompleteFieldToForm("tel", form);
  AddAutoCompleteFieldToForm("one-time-code", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().ReportAutofillWebOTPMetrics(true);
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC, 1);
  histogram_tester.ExpectTotalCount("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                    1);
}

// Verify that proper PhoneCollectionMetricsState is logged to UKM.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateLoggedToUKM) {
  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::WebOTPImpact::kEntryName);
  ASSERT_TRUE(entries.empty());

  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  // Document collects phone number
  AddAutoCompleteFieldToForm("tel", form);
  // Document uses OntTimeCode
  AddAutoCompleteFieldToForm("one-time-code", form);

  std::vector<FormData> forms(1, form);
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
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

  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  test::CreateTestFormField("", "", "", "password", &field);
  field.autocomplete_attribute = "one-time-code";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  form.fields[0].value = u"123456";

  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

    histogram_tester.ExpectTotalCount(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 16, 1);
    autofill_manager().Reset();
  }

  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    autofill_manager().OnDidFillAutofillFormData(
        form, parse_time + base::Microseconds(5));
    autofill_manager().OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            parse_time + base::Microseconds(3));
    test_clock.SetNowTicks(parse_time + base::Microseconds(17));
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);

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

TEST_F(AutofillMetricsTest, LogSuggestionAcceptedIndex_CreditCard) {
  const int index = 2;
  const PopupType popup_type = PopupType::kCreditCards;

  base::HistogramTester histogram_tester;
  AutofillMetrics::LogAutofillSuggestionAcceptedIndex(index, popup_type,
                                                      /*off_the_record=*/false);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.CreditCard", index, 1);

  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.SuggestionAcceptedIndex.Other"),
                HasSubstr("Autofill.SuggestionAcceptedIndex.Profile"))));
}

TEST_F(AutofillMetricsTest, LogSuggestionAcceptedIndex_Profile) {
  const int index = 1;
  const PopupType popup_type1 = PopupType::kPersonalInformation;
  const PopupType popup_type2 = PopupType::kAddresses;

  base::HistogramTester histogram_tester;
  AutofillMetrics::LogAutofillSuggestionAcceptedIndex(index, popup_type1,
                                                      /*off_the_record=*/false);
  AutofillMetrics::LogAutofillSuggestionAcceptedIndex(index, popup_type2,
                                                      /*off_the_record=*/false);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Profile", index, 2);

  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.SuggestionAcceptedIndex.CreditCard"),
                HasSubstr("Autofill.SuggestionAcceptedIndex.Other"))));
}

TEST_F(AutofillMetricsTest, LogSuggestionAcceptedIndex_Other) {
  const int index = 0;
  const PopupType popup_type1 = PopupType::kUnspecified;
  const PopupType popup_type2 = PopupType::kPasswords;

  base::HistogramTester histogram_tester;
  AutofillMetrics::LogAutofillSuggestionAcceptedIndex(index, popup_type1,
                                                      /*off_the_record=*/false);
  AutofillMetrics::LogAutofillSuggestionAcceptedIndex(index, popup_type2,
                                                      /*off_the_record=*/false);

  histogram_tester.ExpectUniqueSample("Autofill.SuggestionAcceptedIndex.Other",
                                      index, 2);

  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.SuggestionAcceptedIndex.CreditCard"),
                HasSubstr("Autofill.SuggestionAcceptedIndex.Profile"))));
}

TEST_F(AutofillMetricsTest, OnAutocompleteSuggestionsShown) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::OnAutocompleteSuggestionsShown();
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      /*expected_count=*/1);
}

TEST_F(AutofillMetricsTest, LogNumberOfAutocompleteEntriesCleanedUp) {
  base::HistogramTester histogram_tester;
  const int kNbEntries = 10;
  AutofillMetrics::LogNumberOfAutocompleteEntriesCleanedUp(kNbEntries);
  histogram_tester.ExpectBucketCount("Autocomplete.Cleanup", kNbEntries,
                                     /*expected_count=*/1);
}

// Verify that we correctly log FormEvent metrics with the appropriate sync
// state.
TEST_F(AutofillMetricsTest, FormEventMetrics_BySyncState) {
  FormData form;
  FormStructure form_structure(form);
  std::vector<FormData> forms(1, form);
  autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                 /*removed_forms=*/{});
  autofill_manager().Reset();

  {
    base::HistogramTester histogram_tester;
    AddressFormEventLogger logger(
        /*is_in_main_frame=*/true,
        /*form_interactions_ukm_logger=*/nullptr,
        /*client=*/nullptr);
    logger.OnDidSeeFillableDynamicForm(AutofillSyncSigninState::kSignedOut,
                                       form_structure);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address.WithNoData.SignedOut",
        FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AddressFormEventLogger logger(
        /*is_in_main_frame=*/true,
        /*form_interactions_ukm_logger=*/nullptr,
        /*client=*/nullptr);
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
  ~AutofillMetricsFunnelTest() = default;
};

INSTANTIATE_TEST_SUITE_P(AutofillMetricsTest,
                         AutofillMetricsFunnelTest,
                         testing::Values(0, 1, 2, 3, 4));

TEST_P(AutofillMetricsFunnelTest, LogFunnelMetrics) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  base::HistogramTester histogram_tester;

  // Phase 1: Simulate events according to GetParam().
  const bool user_interacted_with_form = GetParam() >= 1;
  const bool user_saw_suggestion = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  // Simulate that the autofill manager has seen this form on page load.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form},
                                 /*removed_forms=*/{});

  if (!user_saw_suggestion) {
    // Remove the profile to prevent suggestion from being shown.
    personal_data().ClearProfiles();
  }

  // Simulate interacting with the form.
  if (user_interacted_with_form) {
    autofill_manager().OnAskForValuesToFill(
        /*query_id=*/0, form, form.fields[0], gfx::RectF(),
        /*autoselect_first_suggestion=*/false);
  }

  // Simulate seeing a suggestion.
  if (user_saw_suggestion) {
    autofill_manager().DidShowSuggestions(
        /*has_autofill_suggestions=*/true, form, form.fields[0]);
  }

  // Simulate filling the form.
  if (user_accepted_suggestion) {
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, /*query_id=*/0, form,
        form.fields.front(),
        autofill_manager().MakeFrontendIDForTest(std::string(), kTestGuid));
  }

  // Simulate form submission.
  if (user_submitted_form) {
    autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
  }

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
       "1000"},
  };
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableAblationStudy, feature_parameters);

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Load a fillable form.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");
  form.main_frame_origin = url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  base::HistogramTester histogram_tester;

  // Simulate that the autofill manager has seen this form on page load.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form},
                                 /*removed_forms=*/{});

  // Simulate interacting with the form.
  autofill_manager().OnAskForValuesToFill(
      /*query_id=*/0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);

  // Don't simulate a suggestion but simulate the user typing.
  autofill_manager().OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  const char* kMetrics[] = {
      "Autofill.Funnel.ParsedAsType",
      "Autofill.Funnel.InteractionAfterParsedAsType",
      "Autofill.Funnel.SuggestionAfterInteraction",
      "Autofill.Funnel.FillAfterSuggestion",
      "Autofill.Funnel.SubmissionAfterFill",
      "Autofill.KeyMetrics.FillingReadiness",
      "Autofill.KeyMetrics.FillingAcceptance",
      "Autofill.KeyMetrics.FillingCorrectness",
      "Autofill.KeyMetrics.FillingAssistance",
      "Autofill.Autocomplete.NotOff.FillingAcceptance",
      "Autofill.Autocomplete.Off.FillingAcceptance",
  };
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
  form_.unique_renderer_id = test::MakeFormRendererId();
  form_.name = u"TestForm";
  form_.url = GURL("http://example.com/form.html");
  form_.action = GURL("http://example.com/submit.html");
  form_.main_frame_origin =
      url::Origin::Create(autofill_client_->form_origin());

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form_.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form_.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form_.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form_, field_types, field_types);
}

// Validate Autofill.KeyMetrics.* in case the user submits the empty form.
// Empty in the sense that the user did not fill/type into the fields (not that
// it has no fields).
TEST_F(AutofillMetricsKeyMetricsTest, LogEmptyForm) {
  base::HistogramTester histogram_tester;

  // Simulate page load.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                 /*removed_forms=*/{});
  autofill_manager().OnAskForValuesToFill(
      0, form_, form_.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form_, false,
                                     SubmissionSource::FORM_SUBMISSION);

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
}

// Validate Autofill.KeyMetrics.* in case the user has no address profile on
// file, so nothing can be filled.
TEST_F(AutofillMetricsKeyMetricsTest, LogNoProfile) {
  base::HistogramTester histogram_tester;

  // Simulate that no data is available.
  personal_data().ClearProfiles();
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                 /*removed_forms=*/{});
  autofill_manager().OnAskForValuesToFill(
      0, form_, form_.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);

  // Simulate user typing the address.
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[0], gfx::RectF(),
                                          TimeTicks());
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form_, false,
                                     SubmissionSource::FORM_SUBMISSION);

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
}

// Validate Autofill.KeyMetrics.* in case the user does not accept a suggestion.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserDoesNotAcceptSuggestion) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown but user does not accept it.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                 /*removed_forms=*/{});
  autofill_manager().OnAskForValuesToFill(
      0, form_, form_.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);

  // Simulate user typing the address.
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[0], gfx::RectF(),
                                          TimeTicks());
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form_, false,
                                     SubmissionSource::FORM_SUBMISSION);

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
}

// Validate Autofill.KeyMetrics.* in case the user has to fix the filled data.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserFixesFilledData) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                 /*removed_forms=*/{});
  autofill_manager().OnAskForValuesToFill(
      0, form_, form_.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form_, form_.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), kTestGuid));

  // Simulate user fixing the address.
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form_, false,
                                     SubmissionSource::FORM_SUBMISSION);

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
}

// Validate Autofill.KeyMetrics.* in case the user fixes the filled data but
// then does not submit the form.
TEST_F(AutofillMetricsKeyMetricsTest, LogUserFixesFilledDataButDoesNotSubmit) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                 /*removed_forms=*/{});
  autofill_manager().OnAskForValuesToFill(
      0, form_, form_.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form_, form_.fields[0]);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form_, form_.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), kTestGuid));

  // Simulate user fixing the address.
  autofill_manager().OnTextFieldDidChange(form_, form_.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Don't submit form.

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

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

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

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesUsingTranslatedPageLanguage", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ParsedFieldTypesWasPageTranslated", true, 1);
}

// Validate that the source of the autofilled state field is logged on form
// submission.
TEST_F(AutofillMetricsTest, AutofilledStateFieldSource) {
  RecreateProfile(false);
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "AutofilledStateFieldSource",
       .fields = {{.role = ServerFieldType::NAME_FULL},
                  {.role = ServerFieldType::ADDRESS_HOME_LINE1},
                  {.role = ServerFieldType::ADDRESS_HOME_CITY},
                  {.role = ServerFieldType::PHONE_HOME_NUMBER},
                  {.role = ServerFieldType::ADDRESS_HOME_STATE,
                   .value = u"TN",
                   .form_control_type = "select-one",
                   .is_autofilled = true,
                   .select_options = {{u"TN", u"TN"}, {u"CA", u"CA"}}},
                  {.role = ServerFieldType::ADDRESS_HOME_ZIP},
                  {.role = ServerFieldType::ADDRESS_HOME_COUNTRY}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AutofilledFieldAtSubmission.ByStateSelectionField",
      AutofillMetrics::AutofilledSourceMetricForStateSelectionField::
          AUTOFILL_BY_VALUE,
      1);
}

// Tests the following 4 cases when |kAutofillPreventOverridingPrefilledValues|
// is enabled:
// 1. The field is not autofilled since it has a prefilled value but the value
//    is edited before the form submission and is same as the value that was
//    to be autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue|
//    should emit true for this case.
// 2. The field is not autofilled since it has a prefilled value but the value
//    is edited before the form submission and is different than the value that
//    was to be autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue|
//    should emit false for this case.
// 3. The field had a prefilled value that was similar to the value to be
//    autofilled in the field.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue|
//    should not record anything in this case.
// 4. Selection fields are always overridden by Autofill.
//    |Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue|
//    should not record anything in this case.
TEST_F(AutofillMetricsTest,
       IsValueNotAutofilledOverExistingValueSameAsSubmittedValue) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillPreventOverridingPrefilledValues);
  RecreateProfile(false);

  FormData form = test::GetFormData(
      {.description_for_logging = "AutofilledStateFieldSource",
       .fields = {{.role = ServerFieldType::NAME_FULL},
                  {.role = ServerFieldType::ADDRESS_HOME_CITY,
                   .value = u"Sacremento"},  // Case #1
                  {.role = ServerFieldType::ADDRESS_HOME_STATE,
                   .value = u"CA",
                   .form_control_type = "select-one",
                   .select_options = {{u"TN", u"Tennesse"},
                                      {u"CA", u"California"},
                                      {u"WA", u"Washington DC"}}},  // Case #4
                  {.role = ServerFieldType::ADDRESS_HOME_ZIP,
                   .value = u"00000"},  // Case #2
                  {.role = ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                   .value = u"12345678901"},  // Case #3
                  {.role = ServerFieldType::ADDRESS_HOME_COUNTRY}}});

  std::vector<ServerFieldType> heuristic_types = {
      NAME_FULL,        ADDRESS_HOME_CITY,       ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP, PHONE_HOME_WHOLE_NUMBER, ADDRESS_HOME_COUNTRY};
  std::vector<ServerFieldType> server_types = {
      NAME_FULL,        ADDRESS_HOME_CITY,       ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP, PHONE_HOME_WHOLE_NUMBER, ADDRESS_HOME_COUNTRY};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  autofill_manager().OnAskForValuesToFill(
      0, form, form.fields[0], gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
  autofill_manager().DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));

  // Case #1: Change submitted value to expected autofilled value for the field.
  // The histogram should emit true for this.
  form.fields[1].value = u"Memphis";
  autofill_manager().OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Case #2: Change submitted value such that it different than expected
  // autofilled value for the field. The histogram should emit false for this.
  form.fields[3].value = u"00001";
  autofill_manager().OnTextFieldDidChange(form, form.fields[3], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectBucketCount(
      "Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue",
      true, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue",
      false, 1);
}

TEST_F(AutofillMetricsTest, FormInteractionsAreCounted) {
  // GIVEN
  FormData form =
      test::GetFormData({.fields = {{.role = ServerFieldType::NAME_FULL}}});
  CreateSimpleForm(autofill_client_->form_origin(), form);

  std::vector<ServerFieldType> field_types = {NAME_FULL};
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // WHEN
  // Simulate manual text field change.
  const auto field = form.fields[0];
  autofill_manager().OnTextFieldDidChange(form, field, gfx::RectF(),
                                          TimeTicks());
  // Simulate Autocomplete filling twice.
  autofill_manager().OnSingleFieldSuggestionSelected(u"");
  autofill_manager().OnSingleFieldSuggestionSelected(u"");
  // Simulate Autofill filling.
  std::string guid(kTestGuid);
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, 0, form, form.fields.front(),
      autofill_manager().MakeFrontendIDForTest(std::string(), guid));
  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // THEN
  VerifySubmitFormUkm(
      test_ukm_recorder_, form, AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
      /*is_for_credit_card=*/false,
      /* has_upi_vpa_field=*/false, {FormType::kAddressForm},
      {/*form_element_user_modifications=*/1, /*autofill_fills=*/1,
       /*autocomplete_fills=*/2});
}

TEST_F(AutofillMetricsTest, FormInteractionsAreInitiallyZero) {
  // GIVEN
  FormData form =
      test::GetFormData({.fields = {{.role = ServerFieldType::NAME_FULL}}});
  CreateSimpleForm(autofill_client_->form_origin(), form);

  std::vector<ServerFieldType> field_types = {NAME_FULL};
  autofill_manager().AddSeenForm(form, field_types, field_types);

  // WHEN
  // Simulate form submission.
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  // THEN
  VerifySubmitFormUkm(
      test_ukm_recorder_, form, AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA,
      /*is_for_credit_card=*/false,
      /* has_upi_vpa_field=*/false, {FormType::kAddressForm},
      {/*form_element_user_modifications=*/0, /*autofill_fills=*/0,
       /*autocomplete_fills=*/0});
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
        {base::test::ScopedFeatureList::FeatureAndParams(
             features::kAutofillAcrossIframes, {}),
         base::test::ScopedFeatureList::FeatureAndParams(
             features::kAutofillSharedAutofill,
             {{"relax_shared_autofill", "true"}})},
        {});
  }
  ~AutofillMetricsCrossFrameFormTest() override = default;

  void SetUp() override {
    AutofillMetricsTest::SetUp();

    RecreateCreditCards(true /* include_local_credit_card */,
                        false /* include_masked_server_credit_card */,
                        false /* include_full_server_credit_card */,
                        false /* masked_card_is_enrolled_for_virtual_card */);

    credit_card_with_cvc_ = {.credit_card = *autofill_manager()
                                                 .GetCreditCardAccessManager()
                                                 ->GetCreditCardsToSuggest()
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
                 {.label = u"CVC",
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

  void SeeForm() {
    std::vector<ServerFieldType> field_types = {
        CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
        CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE};
    autofill_manager().AddSeenForm(form_, field_types, field_types);
    autofill_manager().OnFormsSeen(/*updated_forms=*/{form_},
                                   /*removed_forms=*/{});
  }

  CreditCardAndCvc& fill_data() { return credit_card_with_cvc_; }

  // Any call to FillForm() should be followed by a SetFormValues() call to
  // mimic its effect on |form_|.
  void FillForm(const FormFieldData& triggering_field) {
    autofill_manager().FillCreditCardForm(
        0, form_, triggering_field, fill_data().credit_card, fill_data().cvc);
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

  void SubmitForm() {
    autofill_manager().OnFormSubmitted(form_, /*known_success=*/false,
                                       SubmissionSource::FORM_SUBMISSION);
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

// Tests that Autofill.CreditCard.SeamlessFills.* is not emitted for manual
// fills.
TEST_F(AutofillMetricsCrossFrameFormTest,
       DoNotLogCreditCardSeamlessFillsMetricIfNotAutofilled) {
  using UkmBuilder = ukm::builders::Autofill_CreditCardFill;
  base::HistogramTester histogram_tester;
  SeeForm();

  // Fake manual fill.
  SetFormValues(
      {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE},
      /*is_autofilled=*/false, /*is_user_typed=*/true);

  // Fakes an Autofill.
  // This fills nothing because all fields have been manually filled.
  FillForm(FormFieldData());
  SubmitForm();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFills.AtFillTimeBeforeSecurityPolicy", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFills.AtFillTimeBeforeSecurityPolicy."
      "Bitmask",
      0);

  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFills.AtFillTimeAfterSecurityPolicy", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFills.AtFillTimeAfterSecurityPolicy.Bitmask",
      0);

  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFillable.AtFillTimeBeforeSecurityPolicy", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFillable.AtFillTimeBeforeSecurityPolicy."
      "Bitmask",
      0);

  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFillable.AtFillTimeAfterSecurityPolicy", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCard.SeamlessFillable.AtFillTimeAfterSecurityPolicy."
      "Bitmask",
      0);

  VerifyUkm(test_ukm_recorder_, form_, UkmBuilder::kEntryName, {});
}

// Tests that Autofill.CreditCard.SeamlessFills.* are emitted.
TEST_F(AutofillMetricsCrossFrameFormTest,
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
  auto SamplesOf = [&histogram_tester](base::StringPiece metric) {
    return histogram_tester.GetAllSamples(metric);
  };

  SeeForm();

  fill_data().cvc = u"";

  // Fakes an Autofill with the following behavior:
  // - before security and assuming a complete profile: kFullFill;
  // - before security and without a CVC:               kOptionalCvcMissing;
  // - after security  and assuming a complete profile: kPartialFill;
  // - after security  and without a CVC:               kPartialFill;
  // because due to the security policy, only NAME and EXP_DATE are filled.
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
  FillForm(form_.fields[1]);
  SetFormValues({CREDIT_CARD_NUMBER},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  SubmitForm();
  ResetDriverToCommitMetrics();

  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFillable."
                        "AtFillTimeBeforeSecurityPolicy"),
              BucketsAre(Bucket(Metric::kFullFill, 2)));
  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFillable."
                        "AtFillTimeBeforeSecurityPolicy"),
              BucketsAre(Bucket(Metric::kFullFill, 2)));
  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFillable."
                        "AtFillTimeBeforeSecurityPolicy.Bitmask"),
              BucketsAre(Bucket(kName | kNumber | kExp | kCvc, 2)));

  EXPECT_THAT(
      SamplesOf(
          "Autofill.CreditCard.SeamlessFillable.AtFillTimeAfterSecurityPolicy"),
      BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFillable."
                        "AtFillTimeAfterSecurityPolicy.Bitmask"),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber | kCvc, 1)));

  EXPECT_THAT(
      SamplesOf(
          "Autofill.CreditCard.SeamlessFills.AtFillTimeBeforeSecurityPolicy"),
      BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1),
                 Bucket(Metric::kPartialFill, 1)));
  EXPECT_THAT(
      SamplesOf("Autofill.CreditCard.SeamlessFills."
                "AtFillTimeBeforeSecurityPolicy.Bitmask"),
      BucketsAre(Bucket(kName | kNumber | kExp, 1), Bucket(kNumber, 1)));

  EXPECT_THAT(
      SamplesOf(
          "Autofill.CreditCard.SeamlessFills.AtFillTimeAfterSecurityPolicy"),
      BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFills."
                        "AtFillTimeAfterSecurityPolicy.Bitmask"),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));

  EXPECT_THAT(SamplesOf("Autofill.CreditCard.SeamlessFills.AtSubmissionTime"),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1)));
  EXPECT_THAT(
      SamplesOf("Autofill.CreditCard.SeamlessFills.AtSubmissionTime.Bitmask"),
      BucketsAre(Bucket(kName | kNumber | kExp, 1)));

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

           {UkmBuilder::kSharedAutofillName, kSharedAutofillIsIrrelevant},

           {UkmBuilder::kFormSignatureName,
            *Collapse(CalculateFormSignature(form_))},
       }});
}

}  // namespace autofill
