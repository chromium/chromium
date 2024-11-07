// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"

#include <vector>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace autofill::autofill_metrics {
namespace {

using ::autofill::mojom::SubmissionSource;
using ::autofill::test::AddFieldPredictionToForm;
using ::autofill::test::CreateTestFormField;
using AutofillStatus = FormInteractionsUkmLogger::AutofillStatus;

using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmLogHiddenRepresentationalFieldSkipDecisionType =
    ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmEditedAutofilledFieldAtSubmission =
    ukm::builders::Autofill_EditedAutofilledFieldAtSubmission;
using UkmFieldInfoType = ukm::builders::Autofill2_FieldInfo;
using UkmFieldInfoAfterSubmissionType =
    ukm::builders::Autofill2_FieldInfoAfterSubmission;
using UkmFormSummaryType = ukm::builders::Autofill2_FormSummary;
using UkmFocusedComplexFormType = ukm::builders::Autofill2_FocusedComplexForm;
using UkmSubmittedFormWithExperimentalFieldsType =
    ukm::builders::Autofill2_SubmittedFormWithExperimentalFields;
using ExpectedUkmMetricsRecord = std::vector<ExpectedUkmMetricsPair>;
using ExpectedUkmMetrics = std::vector<ExpectedUkmMetricsRecord>;

FormSignature Collapse(FormSignature sig) {
  return FormSignature(sig.value() % 1021);
}

FieldSignature Collapse(FieldSignature sig) {
  return FieldSignature(sig.value() % 1021);
}

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  return base::Base64Encode(unencoded_response_string);
}

class FormInteractionsUkmLoggerTest : public AutofillMetricsBaseTest,
                                      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

// Test that we log the skip decisions for hidden/representational fields
// correctly.
TEST_F(FormInteractionsUkmLoggerTest,
       LogHiddenRepresentationalFieldSkipDecision) {
  RecreateProfile();

  FormData form = CreateForm({
      CreateTestFormField("Name", "name", "",
                          FormControlType::kInputText),  // no decision
      CreateTestFormField("Street", "street", "",
                          FormControlType::kInputText),  // skips
      CreateTestFormField("City", "city", "",
                          FormControlType::kInputText),  // skips
      CreateTestFormField("State", "state", "",
                          FormControlType::kSelectOne),  // doesn't skip
      CreateTestFormField("Country", "country", "",
                          FormControlType::kSelectOne)  // doesn't skip
  });

  test_api(form).field(1).set_is_focusable(false);
  test_api(form).field(2).set_role(FormFieldData::RoleAttribute::kPresentation);
  test_api(form).field(3).set_is_focusable(false);
  test_api(form).field(4).set_role(FormFieldData::RoleAttribute::kPresentation);

  std::vector<FieldType> field_types = {NAME_FULL, ADDRESS_HOME_LINE1,
                                        ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                        ADDRESS_HOME_COUNTRY};

  std::vector<FieldSignature> field_signature;
  for (auto it = form.fields().begin() + 1; it != form.fields().end(); ++it) {
    field_signature.push_back(Collapse(CalculateFieldSignatureForField(*it)));
  }

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate filling form.
  {
    base::UserActionTester user_action_tester;
    FillTestProfile(form);
  }

  VerifyUkm(
      &test_ukm_recorder(), form,
      UkmLogHiddenRepresentationalFieldSkipDecisionType::kEntryName,
      {{{UkmLogHiddenRepresentationalFieldSkipDecisionType::kFormSignatureName,
         form_signature.value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldSignatureName,
         field_signature[2].value()},
        {UkmLogHiddenRepresentationalFieldSkipDecisionType::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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

// Verify that when submitting an autofillable form, the proper type of
// the edited fields is correctly logged to UKM.
TEST_F(FormInteractionsUkmLoggerTest, TypeOfEditedAutofilledFieldsUkmLogging) {
  FormData form = CreateForm(
      {CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                           FormControlType::kInputText),
       CreateTestFormField("Autofill Failed", "autofillfailed",
                           "buddy@gmail.com", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "2345678901",
                           FormControlType::kInputTelephone)});
  test_api(form).field(0).set_is_autofilled(true);
  test_api(form).field(1).set_is_autofilled(true);
  test_api(form).field(2).set_is_autofilled(true);

  std::vector<FieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS,
                                            PHONE_HOME_CITY_AND_NUMBER};

  std::vector<FieldType> server_types = {NAME_FULL, EMAIL_ADDRESS,
                                         PHONE_HOME_CITY_AND_NUMBER};

  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  VerifyUkm(
      &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
      {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
        {UkmFormEventType::kFormTypesName,
         AutofillMetrics::FormTypesToBitVector(
             {FormTypeNameForLogging::kAddressForm})},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields()[0]);

  SubmitForm(form);
  ExpectedUkmMetricsRecord name_field_ukm_record{
      {UkmEditedAutofilledFieldAtSubmission::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields()[0])).value()},
      {UkmEditedAutofilledFieldAtSubmission::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UkmEditedAutofilledFieldAtSubmission::kOverallTypeName,
       static_cast<int64_t>(NAME_FULL)}};

  VerifyUkm(&test_ukm_recorder(), form,
            UkmEditedAutofilledFieldAtSubmission::kEntryName,
            {name_field_ukm_record});
}

// Test the ukm recorded when Suggestion is shown.
TEST_F(FormInteractionsUkmLoggerTest, AutofillSuggestionsShownTest) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText)});

  FormFieldData field;
  std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH};
  autofill_manager().AddSeenForm(form, field_types);

  // Simulate and Autofill query on credit card name field.
  DidShowAutofillSuggestions(form);
  VerifyUkm(
      &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionsShownType::kMillisecondsSinceFormParsedName, 0},
        {UkmSuggestionsShownType::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmSuggestionsShownType::kServerTypeName, CREDIT_CARD_NAME_FULL},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
}

// Test the field log events at the form submission.
class FieldLogUkmMetricTest : public FormInteractionsUkmLoggerTest {
 protected:
  FieldLogUkmMetricTest() {
    scoped_features_.InitAndEnableFeatureWithParameters(
        features::kAutofillLogUKMEventsWithSamplingOnSession,
        {{features::kAutofillLogUKMEventsWithSamplingOnSessionRate.name,
          "100"}});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Test if we record FieldInfo UKM event correctly after we click the field and
// show autofill suggestions.
TEST_F(FieldLogUkmMetricTest, TestShowSuggestionAutofillStatus) {
  RecreateProfile();
  FormData form = test::GetFormData({.fields = {
                                         {.label = u"State", .name = u"state"},
                                         {.label = u"Street"},
                                         {.label = u"Number"},
                                     }});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, NO_SERVER_DATA,
                                        NO_SERVER_DATA};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Show autofill suggestions.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields()[0].global_id(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);

    task_environment_.FastForwardBy(base::Milliseconds(9));
    base::HistogramTester histogram_tester;
    SubmitForm(form);

    // Record Autofill2.FieldInfo UKM event at autofill manager reset.
    test_api(autofill_manager()).Reset();

    // Verify FieldInfo UKM event for every field.
    auto field_entries =
        test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
    ASSERT_EQ(1u, field_entries.size());
    for (size_t i = 0; i < field_entries.size(); ++i) {
      SCOPED_TRACE(testing::Message() << i);
      using UFIT = UkmFieldInfoType;
      const auto* const entry = field_entries[i].get();

      DenseSet<AutofillStatus> autofill_status_vector = {
          AutofillStatus::kIsFocusable, AutofillStatus::kWasFocusedByTapOrClick,
          AutofillStatus::kSuggestionWasAvailable,
          AutofillStatus::kSuggestionWasShown, AutofillStatus::kWasFocused};
      std::map<std::string, int64_t> expected = {
          {UFIT::kFormSessionIdentifierName,
           AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
          {UFIT::kFieldSessionIdentifierName,
           AutofillMetrics::FieldGlobalIdToHash64Bit(
               form.fields()[i].global_id())},
          {UFIT::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
          {UFIT::kFormControlType2Name,
           base::to_underlying(FormControlType::kInputText)},
          {UFIT::kAutocompleteStateName,
           base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
          {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
          {UFIT::kFieldLogEventCountName, 1},
      };

      EXPECT_EQ(expected.size(), entry->metrics.size());
      for (const auto& [metric, value] : expected) {
        test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
      }
    }
  }
}

// Test if we record FieldInfo UKM metrics correctly after we fill and submit an
// address form.
TEST_F(FieldLogUkmMetricTest, AddressSubmittedFormLogEvents) {
  RecreateProfile();
  FormData form = test::GetFormData({.fields = {
                                         {.label = u"State", .name = u"state"},
                                         {.label = u"Street"},
                                         {.label = u"Number"},
                                     }});

  std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_STREET_ADDRESS, NO_SERVER_DATA};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data. The third field cannot be
    // autofilled because its type cannot be predicted.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields()[0].global_id(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);
    FillTestProfile(form);

    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    // Simulate text input in the first fields.
    SimulateUserChangedTextFieldTo(form, form.fields()[0], u"United States",
                                   parse_time + base::Milliseconds(3));
    task_environment_.FastForwardBy(base::Milliseconds(1200));
    base::HistogramTester histogram_tester;
    SubmitForm(form);

    // Record Autofill2.FieldInfo UKM event at autofill manager reset.
    test_api(autofill_manager()).Reset();

    // Verify FieldInfo UKM event for every field.
    auto field_entries =
        test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
    ASSERT_EQ(3u, field_entries.size());
    for (size_t i = 0; i < field_entries.size(); ++i) {
      SCOPED_TRACE(testing::Message() << i);
      using UFIT = UkmFieldInfoType;
      const auto* const entry = field_entries[i].get();

      FieldFillingSkipReason status =
          i == 2 ? FieldFillingSkipReason::kNoFillableGroup
                 : FieldFillingSkipReason::kNotSkipped;
      DenseSet<AutofillStatus> autofill_status_vector;
      int field_log_events_count = 0;
      if (i == 0) {
        autofill_status_vector = {
            AutofillStatus::kIsFocusable,
            AutofillStatus::kWasFocusedByTapOrClick,
            AutofillStatus::kWasAutofillTriggeredAnywhereOnForm,
            AutofillStatus::kShouldBeAutofilledBeforeSecurityPolicy,
            AutofillStatus::kSuggestionWasAvailable,
            AutofillStatus::kSuggestionWasShown,
            AutofillStatus::kWasAutofillTriggeredOnField,
            AutofillStatus::kUserTypedIntoField,
            AutofillStatus::kFilledValueWasModified,
            AutofillStatus::kHadTypedOrFilledValueAtSubmission,
            AutofillStatus::kWasAutofilledAfterSecurityPolicy,
            AutofillStatus::kWasFocused};
        field_log_events_count = 4;
      } else if (i == 1) {
        autofill_status_vector = {
            AutofillStatus::kIsFocusable,
            AutofillStatus::kWasAutofillTriggeredAnywhereOnForm,
            AutofillStatus::kShouldBeAutofilledBeforeSecurityPolicy,
            AutofillStatus::kHadTypedOrFilledValueAtSubmission,
            AutofillStatus::kWasAutofilledAfterSecurityPolicy};
        field_log_events_count = 1;
      } else if (i == 2) {
        autofill_status_vector = {
            AutofillStatus::kIsFocusable,
            AutofillStatus::kWasAutofillTriggeredAnywhereOnForm};
        field_log_events_count = 1;
      }
      std::map<std::string, int64_t> expected = {
          {UFIT::kFormSessionIdentifierName,
           AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
          {UFIT::kFieldSessionIdentifierName,
           AutofillMetrics::FieldGlobalIdToHash64Bit(
               form.fields()[i].global_id())},
          {UFIT::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
          {UFIT::kAutofillSkippedStatusName,
           DenseSet<FieldFillingSkipReason>{status}.data()[0]},
          {UFIT::kFormControlType2Name,
           base::to_underlying(FormControlType::kInputText)},
          {UFIT::kAutocompleteStateName,
           base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
          {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
          {UFIT::kFieldLogEventCountName, field_log_events_count},
      };
      EXPECT_EQ(expected.size(), entry->metrics.size());
      for (const auto& [metric, value] : expected) {
        test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
      }
    }

    // Verify FieldInfoAfterSubmission UKM event for each field in the form.
    auto submission_entries = test_ukm_recorder().GetEntriesByName(
        UkmFieldInfoAfterSubmissionType::kEntryName);
    // Form submission and user interaction trigger uploading votes twice.
    ASSERT_EQ(6u, submission_entries.size());
    for (size_t i = 0; i < submission_entries.size(); ++i) {
      SCOPED_TRACE(testing::Message() << i);
      using UFIAST = UkmFieldInfoAfterSubmissionType;
      const auto* const entry = submission_entries[i].get();
      FieldType submitted_type1 =
          i % 3 == 0 ? ADDRESS_HOME_COUNTRY : EMPTY_TYPE;

      // TODO(crbug.com/40225658): Check that the second vote submission (with
      // SubmissionSource::NONE) is always identical with the first one (it's
      // possible that only the SubmissionSource::NONE exists). If we always
      // get the same values, we should modify
      // BrowserAutofillManager::OnFormSubmittedImpl to only send one vote
      // submission.
      SubmissionSource submission_source =
          i < 3 ? SubmissionSource::FORM_SUBMISSION : SubmissionSource::NONE;
      std::map<std::string, int64_t> expected = {
          {UFIAST::kFormSessionIdentifierName,
           AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
          {UFIAST::kFieldSessionIdentifierName,
           AutofillMetrics::FieldGlobalIdToHash64Bit(
               form.fields()[i % 3].global_id())},
          {UFIAST::kSubmittedType1Name, submitted_type1},
          {UFIAST::kSubmissionSourceName, static_cast<int>(submission_source)},
          {UFIAST::kMillisecondsFromFormParsedUntilSubmissionName, 1000},
      };
      EXPECT_EQ(expected.size(), entry->metrics.size());
      for (const auto& [metric, value] : expected) {
        test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
      }
    }

    // Verify FormSummary UKM event for the form.
    auto form_entries =
        test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
    ASSERT_EQ(1u, form_entries.size());
    using UFST = UkmFormSummaryType;
    const auto* const entry = form_entries[0].get();
    FormInteractionsUkmLogger::FormEventSet form_events = {
        FORM_EVENT_DID_PARSE_FORM,
        FORM_EVENT_INTERACTED_ONCE,
        FORM_EVENT_LOCAL_SUGGESTION_FILLED,
        FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
        FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE,
        FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE};
    std::map<std::string, int64_t> expected = {
        {UFST::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFST::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()},
        {UFST::kAutofillFormEventsName, form_events.data()[0]},
        {UFST::kAutofillFormEvents2Name, form_events.data()[1]},
        {UFST::kSampleRateName, 1},
        {UFST::kWasSubmittedName, true},
        {UFST::kMillisecondsFromFirstInteratctionUntilSubmissionName, 1000},
        {UFST::kMillisecondsFromFormParsedUntilSubmissionName, 1000},
    };
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }

    // Verify LogEvent count UMA events of each type.
    histogram_tester.ExpectBucketCount(
        "Autofill.LogEvent.AskForValuesToFillEvent", 1, 1);
    histogram_tester.ExpectBucketCount("Autofill.LogEvent.TriggerFillEvent", 1,
                                       1);
    histogram_tester.ExpectBucketCount("Autofill.LogEvent.FillEvent", 3, 1);
    histogram_tester.ExpectBucketCount("Autofill.LogEvent.TypingEvent", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.LogEvent.HeuristicPredictionEvent", 0, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.LogEvent.AutocompleteAttributeEvent", 0, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.LogEvent.ServerPredictionEvent", 0, 1);
    histogram_tester.ExpectBucketCount("Autofill.LogEvent.RationalizationEvent",
                                       0, 1);
    histogram_tester.ExpectBucketCount("Autofill.LogEvent.All", 6, 1);
  }
}

// Test if we have recorded UKM metrics correctly about field types after
// parsing the form by the local heuristic prediction.
TEST_F(FieldLogUkmMetricTest, AutofillFieldInfoMetricsFieldType) {
  FormData form = test::GetFormData(
      {.fields = {
           // Heuristic value will match with Autocomplete attribute.
           {.label = u"Last Name",
            .name = u"lastname",
            .autocomplete_attribute = "family-name"},
           // Heuristic value will NOT match with Autocomplete attribute.
           {.label = u"First Name",
            .name = u"firstname",
            .autocomplete_attribute = "additional-name"},
           // No autocomplete attribute.
           {.label = u"Address",
            .name = u"address",
            .autocomplete_attribute = "off"},
           // Heuristic value will be unknown.
           {.label = u"Garbage label",
            .name = u"garbage",
            .autocomplete_attribute = "postal-code"},
           {.label = u"Email",
            .name = u"email",
            .autocomplete_attribute = "garbage"},
           {.label = u"Password",
            .name = u"password",
            .autocomplete_attribute = "new-password"},
       }});

  auto form_structure = std::make_unique<FormStructure>(form);
  FormStructure* form_structure_ptr = form_structure.get();
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  ASSERT_TRUE(
      test_api(autofill_manager())
          .mutable_form_structures()
          ->emplace(form_structure_ptr->global_id(), std::move(form_structure))
          .second);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // The server type of each field predicted from autofill crowdsourced server.
  std::vector<FieldType> server_types{
      // Server response will match with autocomplete.
      NAME_LAST,
      // Server response will NOT match with autocomplete.
      NAME_FIRST,
      // No autocomplete, server predicts a type from majority voting.
      NAME_MIDDLE,
      // Server response will have no data.
      NO_SERVER_DATA, EMAIL_ADDRESS, NO_SERVER_DATA};
  // Set suggestions from server for the form.
  for (size_t i = 0; i < server_types.size(); ++i) {
    AddFieldPredictionToForm(form.fields()[i], server_types[i],
                             form_suggestion);
  }

  std::string response_string = SerializeAndEncode(response);
  test_api(autofill_manager())
      .OnLoadedServerPredictions(
          response_string, test::GetEncodedSignatures(*form_structure_ptr));

  task_environment_.FastForwardBy(base::Milliseconds(37000));
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  test_api(autofill_manager()).Reset();

  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(6u, entries.size());
  // The heuristic type of each field. The local heuristic prediction does not
  // predict the type for the fourth field.
  std::vector<FieldType> heuristic_types{NAME_LAST,          NAME_FIRST,
                                         ADDRESS_HOME_LINE1, UNKNOWN_TYPE,
                                         EMAIL_ADDRESS,      UNKNOWN_TYPE};
  // Field types as per the autocomplete attribute in the input.
  std::vector<HtmlFieldType> html_field_types{
      HtmlFieldType::kFamilyName,   HtmlFieldType::kAdditionalName,
      HtmlFieldType::kUnrecognized, HtmlFieldType::kPostalCode,
      HtmlFieldType::kUnrecognized, HtmlFieldType::kUnrecognized};
  std::vector<FieldType> overall_types{NAME_LAST,     NAME_MIDDLE,
                                       NAME_MIDDLE,   ADDRESS_HOME_ZIP,
                                       EMAIL_ADDRESS, UNKNOWN_TYPE};
  std::vector<AutofillMetrics::AutocompleteState> autocomplete_states{
      AutofillMetrics::AutocompleteState::kValid,
      AutofillMetrics::AutocompleteState::kValid,
      AutofillMetrics::AutocompleteState::kOff,
      AutofillMetrics::AutocompleteState::kValid,
      AutofillMetrics::AutocompleteState::kGarbage,
      AutofillMetrics::AutocompleteState::kPassword};
  int field_log_events_count = 0;
  // Verify FieldInfo UKM event for every field.
  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i].get();
    FieldPrediction::Source prediction_source =
        server_types[i] != NO_SERVER_DATA
            ? FieldPrediction::SOURCE_AUTOFILL_DEFAULT
            : FieldPrediction::SOURCE_UNSPECIFIED;
    DenseSet<AutofillStatus> autofill_status_vector = {
        AutofillStatus::kIsFocusable};
    field_log_events_count = 2;
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
        {UFIT::kServerType1Name, server_types[i]},
        {UFIT::kServerPredictionSource1Name, prediction_source},
        {UFIT::kServerType2Name, /*SERVER_RESPONSE_PENDING*/ 161},
        {UFIT::kServerPredictionSource2Name,
         FieldPrediction::SOURCE_UNSPECIFIED},
        {UFIT::kServerTypeIsOverrideName, false},
        {UFIT::kOverallTypeName, overall_types[i]},
        {UFIT::kSectionIdName, 1},
        {UFIT::kTypeChangedByRationalizationName, false},
        {UFIT::kRankInFieldSignatureGroupName, 1},
        {UFIT::kFormControlType2Name,
         base::to_underlying(FormControlType::kInputText)},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(autocomplete_states[i])},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
    };
    if (heuristic_types[i] != UNKNOWN_TYPE) {
      field_log_events_count += 2;
      expected.merge(std::map<std::string, int64_t>({
          {UFIT::kHeuristicTypeName, heuristic_types[i]},
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
          {UFIT::kHeuristicTypeLegacyName, UNKNOWN_TYPE},
          {UFIT::kHeuristicTypeDefaultName, heuristic_types[i]},
          {UFIT::kHeuristicTypeExperimentalName, UNKNOWN_TYPE},
      }));
#else
          {UFIT::kHeuristicTypeLegacyName, heuristic_types[i]},
          {UFIT::kHeuristicTypeDefaultName, UNKNOWN_TYPE},
          {UFIT::kHeuristicTypeExperimentalName, UNKNOWN_TYPE},
      }));
#endif
    } else {
      ++field_log_events_count;
    }
    if (autocomplete_states[i] != AutofillMetrics::AutocompleteState::kOff) {
      expected.merge(std::map<std::string, int64_t>({
          {UFIT::kHtmlFieldTypeName, base::to_underlying(html_field_types[i])},
          {UFIT::kHtmlFieldModeName, base::to_underlying(HtmlFieldMode::kNone)},
      }));
      ++field_log_events_count;
    }
    expected.merge(std::map<std::string, int64_t>({
        {UFIT::kFieldLogEventCountName, field_log_events_count},
    }));
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }
  }

  // Verify FieldInfoAfterSubmission UKM event for each field in the form.
  auto submission_entries = test_ukm_recorder().GetEntriesByName(
      UkmFieldInfoAfterSubmissionType::kEntryName);
  // Form submission triggers uploading votes once.
  ASSERT_EQ(6u, submission_entries.size());
  for (size_t i = 0; i < submission_entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIAST = UkmFieldInfoAfterSubmissionType;
    const auto* const entry = submission_entries[i].get();
    std::map<std::string, int64_t> expected = {
        {UFIAST::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIAST::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i].global_id())},
        {UFIAST::kSubmittedType1Name, EMPTY_TYPE},
        {UFIAST::kSubmissionSourceName,
         static_cast<int>(SubmissionSource::FORM_SUBMISSION)},
        {UFIAST::kMillisecondsFromFormParsedUntilSubmissionName, 35000},
    };
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      if (metric == UFIAST::kMillisecondsFromFormParsedUntilSubmissionName) {
        test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
      }
    }
  }

  // Verify FormSummary UKM event for the form.
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  ASSERT_EQ(1u, form_entries.size());
  using UFST = UkmFormSummaryType;
  const auto* const entry = form_entries[0].get();
  FormInteractionsUkmLogger::FormEventSet form_events = {};
  std::map<std::string, int64_t> expected = {
      {UFST::kFormSessionIdentifierName,
       AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
      {UFST::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UFST::kAutofillFormEventsName, form_events.data()[0]},
      {UFST::kAutofillFormEvents2Name, form_events.data()[1]},
      {UFST::kSampleRateName, 1},
      {UFST::kWasSubmittedName, true},
      {UFST::kMillisecondsFromFormParsedUntilSubmissionName, 35000},
  };
  EXPECT_EQ(expected.size(), entry->metrics.size());
  for (const auto& [metric, value] : expected) {
    test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
  }

  // Verify LogEvent count UMA events of each type.
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AskForValuesToFillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TriggerFillEvent", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.FillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TypingEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AutocompleteAttributeEvent", 5, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.ServerPredictionEvent",
                                     6, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.RationalizationEvent",
                                     12, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.HeuristicPredictionEvent", 4, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.All", 27, 1);
}

// Test if we have recorded FieldInfo UKM metrics correctly after typing in
// fields without autofilling first.
TEST_F(FieldLogUkmMetricTest, AutofillFieldInfoMetricsEditedFieldWithoutFill) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FULL},
                 {.role = EMAIL_ADDRESS},
                 {.role = PHONE_HOME_CITY_AND_NUMBER}}};

  FormData form = GetAndAddSeenForm(form_description);

  base::TimeTicks parse_time = autofill_manager()
                                   .form_structures()
                                   .begin()
                                   ->second->form_parsed_timestamp();
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextFieldTo(form, form.fields()[0], u"Elvis Aaron Presley",
                                 parse_time + base::Milliseconds(3));
  SimulateUserChangedTextFieldTo(form, form.fields()[1], u"buddy@gmail.com",
                                 parse_time + base::Milliseconds(3));
  task_environment_.FastForwardBy(base::Milliseconds(1200));
  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  test_api(autofill_manager()).Reset();

  // Verify FieldInfo UKM event for every field.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(2u, entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    DenseSet<AutofillStatus> autofill_status_vector = {
        AutofillStatus::kIsFocusable, AutofillStatus::kUserTypedIntoField,
        AutofillStatus::kHadTypedOrFilledValueAtSubmission};
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i].get();
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
        {UFIT::kFormControlType2Name,
         base::to_underlying(FormControlType::kInputText)},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
        {UFIT::kFieldLogEventCountName, 1},
    };

    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }
  }

  // Verify FieldInfoAfterSubmission UKM event for each field in the form.
  std::vector<FieldType> submitted_types{NAME_FULL, EMAIL_ADDRESS, EMPTY_TYPE};
  auto submission_entries = test_ukm_recorder().GetEntriesByName(
      UkmFieldInfoAfterSubmissionType::kEntryName);
  // Form submission and user interaction trigger uploading votes twice.
  ASSERT_EQ(6u, submission_entries.size());
  for (size_t i = 0; i < submission_entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIAST = UkmFieldInfoAfterSubmissionType;
    const auto* const entry = submission_entries[i].get();
    SubmissionSource submission_source =
        i < 3 ? SubmissionSource::FORM_SUBMISSION : SubmissionSource::NONE;
    std::map<std::string, int64_t> expected = {
        {UFIAST::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIAST::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i % 3].global_id())},
        {UFIAST::kSubmittedType1Name, submitted_types[i % 3]},
        {UFIAST::kSubmissionSourceName, static_cast<int>(submission_source)},
        {UFIAST::kMillisecondsFromFormParsedUntilSubmissionName, 1000},
    };
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      if (metric == UFIAST::kMillisecondsFromFormParsedUntilSubmissionName) {
        test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
      }
    }
  }

  // Verify FormSummary UKM event for the form.
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  ASSERT_EQ(1u, form_entries.size());
  using UFST = UkmFormSummaryType;
  const auto* const entry = form_entries[0].get();
  FormInteractionsUkmLogger::FormEventSet form_events = {
      FORM_EVENT_DID_PARSE_FORM};
  std::map<std::string, int64_t> expected = {
      {UFST::kFormSessionIdentifierName,
       AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
      {UFST::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UFST::kAutofillFormEventsName, form_events.data()[0]},
      {UFST::kAutofillFormEvents2Name, form_events.data()[1]},
      {UFST::kSampleRateName, 1},
      {UFST::kWasSubmittedName, true},
      {UFST::kMillisecondsFromFirstInteratctionUntilSubmissionName, 1000},
      {UFST::kMillisecondsFromFormParsedUntilSubmissionName, 1000},
  };
  EXPECT_EQ(expected.size(), entry->metrics.size());
  for (const auto& [metric, value] : expected) {
    test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
  }

  // Verify LogEvent count UMA events of each type.
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AskForValuesToFillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TriggerFillEvent", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.FillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TypingEvent", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.HeuristicPredictionEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AutocompleteAttributeEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.ServerPredictionEvent",
                                     0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.RationalizationEvent",
                                     0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.All", 2, 1);
}

// Test that we do not record FieldInfo/FormSummary UKM metrics for forms
// whose action is a search URL. We do this to reduce the number of useless UKM
// events.
TEST_F(FieldLogUkmMetricTest,
       AutofillFieldInfoMetricsNotRecordOnSearchURLForm) {
  FormData form = CreateForm(
      {CreateTestFormField("input", "", "", FormControlType::kInputText)});
  // Form whose action is a search URL should not be parsed.
  form.set_action(GURL("http://google.com/search?q=hello"));

  SeeForm(form);
  SubmitForm(form);

  test_api(autofill_manager()).Reset();

  // This form is not parsed in |AutofillManager::OnFormsSeen|.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  EXPECT_EQ(0u, entries.size());
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  EXPECT_EQ(0u, form_entries.size());
}

// Test that we do not record FieldInfo/FormSummary UKM metrics for forms
// that have one search box. We do this to reduce the number of useless UKM
// events.
TEST_F(FieldLogUkmMetricTest, AutofillFieldInfoMetricsNotRecordOnSearchBox) {
  FormData form = CreateForm({CreateTestFormField(
      "Search", "Search", "", FormControlType::kInputText)});
  // The form only has one field which has a placeholder of 'Search'.
  test_api(form).field(0).set_placeholder(u"Search");

  SeeForm(form);
  SubmitForm(form);

  test_api(autofill_manager()).Reset();

  // The form that only has a search box is not recorded into any UKM events.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  EXPECT_EQ(0u, entries.size());
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  EXPECT_EQ(0u, form_entries.size());
}

// Tests that the forms with only <input type="checkbox"> fields are not
// recorded in FieldInfo metrics. We do this to reduce bandwidth.
TEST_F(FieldLogUkmMetricTest, AutofillFieldInfoMetricsNotRecordOnAllCheckBox) {
  // Two checkable checkboxes.
  FormData form = test::GetFormData(
      {.fields = {
           {.label = u"Option 1",
            .name = u"Option 1",
            .form_control_type = FormControlType::kInputCheckbox},
           {.label = u"Option 2",
            .name = u"Option 2",
            .form_control_type = FormControlType::kInputCheckbox},
       }});

  SeeForm(form);
  SubmitForm(form);
  test_api(autofill_manager()).Reset();

  // The form with two checkboxes is not recorded into any UKM events.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  EXPECT_EQ(0u, entries.size());
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  EXPECT_EQ(0u, form_entries.size());
}

// Tests that the forms with <input type="checkbox"> fields and a text field
// which does not get a type from heuristics or the server are not recorded in
// UkmFieldInfo metrics.
TEST_F(
    FieldLogUkmMetricTest,
    AutofillFieldInfoMetricsNotRecordOnCheckBoxWithTextFieldWithUnknownType) {
  FormData form = test::GetFormData(
      {.fields = {
           // Start with a username field.
           {.label = u"username", .name = u"username"},
           // Two checkable radio buttons.
           {.label = u"female",
            .name = u"female",
            .form_control_type = FormControlType::kInputRadio},
           {.label = u"male",
            .name = u"male",
            .form_control_type = FormControlType::kInputRadio},
           // One checkable checkbox.
           {.label = u"save",
            .name = u"save",
            .form_control_type = FormControlType::kInputCheckbox},
       }});

  SeeForm(form);
  SubmitForm(form);
  test_api(autofill_manager()).Reset();

  // This form only has one non-checkable field, so the local heuristics are
  // not executed.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  EXPECT_EQ(0u, entries.size());
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  EXPECT_EQ(0u, form_entries.size());
}

// Tests that the forms with <input type="checkbox"> fields and two text field
// which have predicted types are recorded in FieldInfo metrics.
TEST_F(FieldLogUkmMetricTest,
       AutofillFieldInfoMetricsRecordOnCheckBoxWithTextField) {
  FormData form = test::GetFormData(
      {.fields = {
           // Start with two input text fields.
           {.label = u"First Name", .name = u"firstname"},
           {.label = u"Last Name", .name = u"lastname"},
           // Two checkable radio buttons.
           {.label = u"female",
            .name = u"female",
            .form_control_type = FormControlType::kInputRadio},
           {.label = u"male",
            .name = u"male",
            .form_control_type = FormControlType::kInputRadio},
       }});

  // The two text fields have predicted types.
  std::vector<FieldType> field_types = {NAME_FIRST, NAME_LAST, UNKNOWN_TYPE,
                                        UNKNOWN_TYPE};
  autofill_manager().AddSeenForm(form, field_types);
  SeeForm(form);
  task_environment_.FastForwardBy(base::Milliseconds(3500));
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  test_api(autofill_manager()).Reset();

  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(4u, entries.size());
  std::vector<FormControlType> form_control_types = {
      FormControlType::kInputText, FormControlType::kInputText,
      FormControlType::kInputRadio, FormControlType::kInputRadio};
  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    DenseSet<AutofillStatus> autofill_status_vector = {
        AutofillStatus::kIsFocusable};
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i].get();
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
        {UFIT::kOverallTypeName, field_types[i]},
        {UFIT::kSectionIdName, 1},
        {UFIT::kTypeChangedByRationalizationName, false},
        {UFIT::kFormControlType2Name,
         base::to_underlying(form_control_types[i])},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
        {UFIT::kFieldLogEventCountName, 1},
    };

    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }
  }

  // Verify FormSummary UKM event for the form.
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  ASSERT_EQ(1u, form_entries.size());
  using UFST = UkmFormSummaryType;
  const auto* const form_entry = form_entries[0].get();
  FormInteractionsUkmLogger::FormEventSet form_events = {
      FORM_EVENT_DID_PARSE_FORM};
  std::map<std::string, int64_t> expected = {
      {UFST::kFormSessionIdentifierName,
       AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
      {UFST::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UFST::kAutofillFormEventsName, form_events.data()[0]},
      {UFST::kAutofillFormEvents2Name, form_events.data()[1]},
      {UFST::kSampleRateName, 1},
      {UFST::kWasSubmittedName, true},
      {UFST::kMillisecondsFromFormParsedUntilSubmissionName, 3000},
  };
  EXPECT_EQ(expected.size(), form_entry->metrics.size());
  for (const auto& [metric, value] : expected) {
    test_ukm_recorder().ExpectEntryMetric(form_entry, metric, value);
  }

  // Verify LogEvent count UMA events of each type.
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AskForValuesToFillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TriggerFillEvent", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.FillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TypingEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.HeuristicPredictionEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AutocompleteAttributeEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.ServerPredictionEvent",
                                     0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.RationalizationEvent",
                                     4, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.All", 4, 1);
}

// Tests that the field which is in a different frame than its form is recorded
// as AutofillStatus::kIsInSubFrame.
TEST_F(FieldLogUkmMetricTest, AutofillFieldInfoMetricsRecordOnDifferentFrames) {
  // The form has three input text fields, the second field is in a sub frame.
  FormData form = test::GetFormData(
      {.fields =
           {
               {.label = u"First Name", .name = u"firstname"},
               {.host_frame = test::MakeFormGlobalId().frame_token,
                .label = u"Last Name",
                .name = u"lastname"},
               {.label = u"Email", .name = u"email"},
           },
       .host_frame = test::MakeFormGlobalId().frame_token});

  std::vector<FieldType> field_types = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS};
  autofill_manager().AddSeenForm(form, field_types);
  SeeForm(form);
  task_environment_.FastForwardBy(base::Milliseconds(1980000));  // 33m
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  test_api(autofill_manager()).Reset();

  // Verify FieldInfo UKM event for each field.
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(3u, entries.size());
  std::vector<FormControlType> form_control_types = {
      FormControlType::kInputText, FormControlType::kInputText,
      FormControlType::kInputText};
  for (size_t i = 0; i < entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);

    DenseSet<AutofillStatus> autofill_status_vector;
    if (i == 1) {
      autofill_status_vector = {AutofillStatus::kIsFocusable,
                                AutofillStatus::kIsInSubFrame};
    } else {
      autofill_status_vector = {AutofillStatus::kIsFocusable};
    }
    using UFIT = UkmFieldInfoType;
    const auto* const entry = entries[i].get();
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[i])).value()},
        {UFIT::kOverallTypeName, field_types[i]},
        {UFIT::kSectionIdName, 1},
        {UFIT::kTypeChangedByRationalizationName, false},
        {UFIT::kFormControlType2Name,
         base::to_underlying(form_control_types[i])},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
        {UFIT::kHeuristicTypeName, field_types[i]},
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
        {UFIT::kHeuristicTypeLegacyName, UNKNOWN_TYPE},
        {UFIT::kHeuristicTypeDefaultName, field_types[i]},
        {UFIT::kHeuristicTypeExperimentalName, UNKNOWN_TYPE},
#else
        {UFIT::kHeuristicTypeLegacyName, field_types[i]},
        {UFIT::kHeuristicTypeDefaultName, UNKNOWN_TYPE},
        {UFIT::kHeuristicTypeExperimentalName, UNKNOWN_TYPE},
#endif
        {UFIT::kFieldLogEventCountName, 2},
        {UFIT::kRankInFieldSignatureGroupName, 1},
    };

    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }
  }

  // Verify FormSummary UKM event for the form.
  auto form_entries =
      test_ukm_recorder().GetEntriesByName(UkmFormSummaryType::kEntryName);
  ASSERT_EQ(1u, form_entries.size());
  using UFST = UkmFormSummaryType;
  const auto* const form_entry = form_entries[0].get();
  FormInteractionsUkmLogger::FormEventSet form_events = {
      FORM_EVENT_DID_PARSE_FORM};
  std::map<std::string, int64_t> expected = {
      {UFST::kFormSessionIdentifierName,
       AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id())},
      {UFST::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()},
      {UFST::kAutofillFormEventsName, form_events.data()[0]},
      {UFST::kAutofillFormEvents2Name, form_events.data()[1]},
      {UFST::kSampleRateName, 1},
      {UFST::kWasSubmittedName, true},
      {UFST::kMillisecondsFromFormParsedUntilSubmissionName, 1800000},  // 30m
  };
  EXPECT_EQ(expected.size(), form_entry->metrics.size());
  for (const auto& [metric, value] : expected) {
    test_ukm_recorder().ExpectEntryMetric(form_entry, metric, value);
  }

  // Verify LogEvent count UMA events of each type.
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AskForValuesToFillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TriggerFillEvent", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.FillEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.TypingEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.AutocompleteAttributeEvent", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.ServerPredictionEvent",
                                     0, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.RationalizationEvent",
                                     3, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LogEvent.HeuristicPredictionEvent", 3, 1);
  histogram_tester.ExpectBucketCount("Autofill.LogEvent.All", 6, 1);
}

// The following code tests that Autofill2.FocusedComplexForm events are emitted
// correctly. These metrics are reported for forms that are classified as
// FormTypeNameForLogging::kPostalAddress and
// FormTypeNameForLogging::kCreditCard.
struct LogFocusedComplexFormAtFormRemoveTestCase {
  std::string test_name;
  test::FormDescription form;
  bool enable_ablation_study_for_addresses = false;
  bool ablation_study_is_dry_run = false;
  // Simulate focus by tab key, focus on page load, or focus by click/tap.
  // For click/tap, also `step_1_click` should be true. The first field
  // of the form is focused.
  bool step_0_focus = false;
  // Simulate clicking on field, which triggers a query for autofill. This
  // should always come after `step_0_focus`. The first field of the form is
  // clicked.
  bool step_1_click = false;
  // Simulate that the user modifies the content of the first field.
  bool step_2_typing = false;
  // Simulate triggering autofill on the first field.
  bool step_3_autofill = false;
  // Simulate that the user edited the first field.
  bool step_4_edit_after_autofill = false;
  // Simlate that the form was submitted.
  bool step_5_submit = false;

  // Key: UKM Metric Name, Value: recorded metric.
  // If this is empty, we assume that no UKM metrics are recorded.
  std::map<std::string, int64_t> expected_metrics;
};

class LogFocusedComplexFormAtFormRemoveTest
    : public AutofillMetricsBaseTest,
      public testing::TestWithParam<LogFocusedComplexFormAtFormRemoveTestCase> {
 public:
  LogFocusedComplexFormAtFormRemoveTest() {
    scoped_features_.InitAndEnableFeatureWithParameters(
        features::kAutofillLogUKMEventsWithSamplingOnSession,
        {{features::kAutofillLogUKMEventsWithSamplingOnSessionRate.name,
          "100"}});
  }
  ~LogFocusedComplexFormAtFormRemoveTest() override = default;

  void SetUp() override {
    SetUpHelper();
    // Enforce a timezone to create deterministic behavior w.r.t.
    // UkmFocusedComplexFormType::kDayInAblationWindowName.
    previousZone = base::WrapUnique(icu::TimeZone::createDefault());
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("GMT"));
  }
  void TearDown() override {
    TearDownHelper();
    icu::TimeZone::adoptDefault(previousZone.release());
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<icu::TimeZone> previousZone;
};

// These are just constants to make the code below easier to understand.
constexpr int kFormType_Address =
    1 << base::to_underlying(FormType::kAddressForm);
constexpr int kFormType_CreditCard =
    1 << base::to_underlying(FormType::kCreditCardForm);
constexpr int kFormTypeNameForLogging_AddressForm_or_PostalAddressForm =
    (1 << base::to_underlying(FormTypeNameForLogging::kAddressForm)) |
    (1 << base::to_underlying(FormTypeNameForLogging::kPostalAddressForm));
constexpr int kFormTypeNameForLogging_CreditCardForm =
    1 << base::to_underlying(FormTypeNameForLogging::kCreditCardForm);

INSTANTIATE_TEST_SUITE_P(
    FieldLogUkmMetricTest,
    LogFocusedComplexFormAtFormRemoveTest,
    testing::Values(
        LogFocusedComplexFormAtFormRemoveTestCase{
            // This form is neither a credit card form nor a postal address
            // form. Therefore, no Autofill2.FocusedComplexForm event should be
            // emitted despite plenty of interactions.
            .test_name = "neither_postal_address_nor_credit_card",
            .form = test::FormDescription{.fields = {{.role = NAME_LAST},
                                                     {.role = NAME_FIRST},
                                                     {.role = EMAIL_ADDRESS}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = true,
            .step_5_submit = true,
            // Despite the typing, no metrics are recorded because the form is
            // not eligible for reporting.
            .expected_metrics = {}},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // This form is a postal address form but not focused by the user.
            // Therefore, no Autofill2.FocusedComplexForm event should be
            // emitted.
            .test_name = "not_focused",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_STREET_ADDRESS,
                                .autocomplete_attribute = "street-address"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .expected_metrics = {}},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user focuses a field of a postal address form. An
            // Autofill2.FocusedComplexForm event should be emitted.
            .test_name = "focused_autofillable_field",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName, 0},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName, 0},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     0},
                    {UkmFocusedComplexFormType::kUserModifiedName, 0},
                    // No kMillisecondsFromFirstInteractionUntilSubmissionName
                    // because the user did not edit anything.
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName, 0},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user focuses a field of a postal address form. An
            // Autofill2.FocusedComplexForm event should be emitted.
            // Test that the absence of a submission is correctly reported.
            .test_name = "focused_autofillable_field_no_submission",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_5_submit = false,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName, 0},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName, 0},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     0},
                    {UkmFocusedComplexFormType::kUserModifiedName, 0},
                    // No kMillisecondsFromFirstInteractionUntilSubmissionName
                    // because the user did not edit anything.
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName, 0},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 0},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user types and then submitts this postal address form. An
            // Autofill2.FocusedComplexForm event should be emitted.
            .test_name = "focused_and_typed_into_autofillable_field",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName, 0},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     1000},
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user types into a field that is not part of the
            // address section of the form. In this case don't consider the
            // form interacted for the purpose of this metric.
            .test_name = "typed_into_non-autofillable_field",
            .form =
                test::FormDescription{
                    .fields = {{.role = UNKNOWN_TYPE,
                                .autocomplete_attribute = "unknown-type"},
                               {.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = true,
            .step_5_submit = true},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // This form is an autofilled and then submitted postal address
            // form. An Autofill2.FocusedComplexForm event should be emitted.
            .test_name = "autofilled",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = false,
            .step_3_autofill = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     1000},
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // This form is an autofilled and then submitted postal address
            // form. An Autofill2.FocusedComplexForm event should be emitted.
            .test_name = "autofilled_then_edited",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = false,
            .step_3_autofill = true,
            .step_4_edit_after_autofill = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     2000},
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // This form consists of a credit card section followed by a
            // postal address section. Only the credit card section is filled
            // and edited. The address part is not touched but should still
            // be reported in the form types.
            .test_name = "autofilled_then_edited_cc_from_followed_by_address",
            .form =
                test::FormDescription{
                    .fields = {{.role = CREDIT_CARD_NAME_FULL,
                                .autocomplete_attribute = "cc-name"},
                               {.role = CREDIT_CARD_NUMBER,
                                .autocomplete_attribute = "cc-number"},
                               {.role = CREDIT_CARD_VERIFICATION_CODE,
                                .autocomplete_attribute = "cc-csc"},
                               {.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = false,
            .step_3_autofill = true,
            .step_4_edit_after_autofill = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm |
                         kFormTypeNameForLogging_CreditCardForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     2000},
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName,
                     kFormType_CreditCard},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user focuses and types into an autofillable field. Because
            // the ablation study is rolled to 100%, the respective metrics are
            // reported.
            .test_name = "ablation_study",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .enable_ablation_study_for_addresses = true,
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName, 0},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     1000},
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName, 0},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                    // Ablation study metrics.
                    {UkmFocusedComplexFormType::
                         kIsInAblationGroupOfAblationName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kIsInAblationGroupOfConditionalAblationName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kIsInControlGroupOfAblationName,
                     0},
                    {UkmFocusedComplexFormType::
                         kIsInControlGroupOfConditionalAblationName,
                     0},
                    {UkmFocusedComplexFormType::kDayInAblationWindowName, 10},
                    {UkmFocusedComplexFormType::
                         kIsAblationStudyInDryRunModeName,
                     0},
                }},
        LogFocusedComplexFormAtFormRemoveTestCase{
            // The user focuses and types into an autofillable field. Because
            // the ablation study is rolled to 100%, the respective metrics are
            // reported.
            .test_name = "ablation_study_in_dry_run_mode",
            .form =
                test::FormDescription{
                    .fields = {{.role = ADDRESS_HOME_LINE1,
                                .autocomplete_attribute = "address-line1"},
                               {.role = ADDRESS_HOME_CITY,
                                .autocomplete_attribute = "address-level2"},
                               {.role = ADDRESS_HOME_STATE,
                                .autocomplete_attribute = "address-level1"}}},
            .enable_ablation_study_for_addresses = true,
            .ablation_study_is_dry_run = true,
            .step_0_focus = true,
            .step_1_click = true,
            .step_2_typing = true,
            .step_5_submit = true,
            .expected_metrics =
                {
                    {UkmFocusedComplexFormType::kAutofilledName, 0},
                    {UkmFocusedComplexFormType::kEditedAfterAutofillName, 0},
                    {UkmFocusedComplexFormType::kAutofillDataQueriedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kFormTypesName,
                     kFormTypeNameForLogging_AddressForm_or_PostalAddressForm},
                    {UkmFocusedComplexFormType::
                         kHadNonEmptyValueAtSubmissionName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kUserModifiedName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kMillisecondsFromFirstInteractionUntilSubmissionName,
                     1000},
                    // Due to the dry-run mode, suggestions are available.
                    {UkmFocusedComplexFormType::kSuggestionsAvailableName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kWasSubmittedName, 1},
                    // Ablation study metrics. All of these are the same as
                    // in the non-dry-run mode.
                    {UkmFocusedComplexFormType::
                         kIsInAblationGroupOfAblationName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::
                         kIsInAblationGroupOfConditionalAblationName,
                     kFormType_Address},
                    {UkmFocusedComplexFormType::kIsInControlGroupOfAblationName,
                     0},
                    {UkmFocusedComplexFormType::
                         kIsInControlGroupOfConditionalAblationName,
                     0},
                    {UkmFocusedComplexFormType::kDayInAblationWindowName, 10},
                    // This is true due to dry-run mode.
                    {UkmFocusedComplexFormType::
                         kIsAblationStudyInDryRunModeName,
                     1},
                }}),
    [](const testing::TestParamInfo<
        LogFocusedComplexFormAtFormRemoveTest::ParamType>& info) {
      std::string name = info.param.test_name;
      base::ranges::replace_if(
          name, [](char c) { return !std::isalnum(c); }, '_');
      return name;
    });

// Test if we have recorded Autofill2.FocusedComplexForm UKM metrics correctly.
TEST_P(LogFocusedComplexFormAtFormRemoveTest, TestEmittedUKM) {
  base::FieldTrialParams feature_parameters{
      {features::kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {features::kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
       "1000"},
      {features::kAutofillAblationStudyIsDryRun.name,
       GetParam().ablation_study_is_dry_run ? "true" : "false"}};
  base::test::ScopedFeatureList scoped_feature_list;
  if (GetParam().enable_ablation_study_for_addresses) {
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableAblationStudy, feature_parameters);
  }
  constexpr base::Time arbitrary_default_time =
      base::Time::FromSecondsSinceUnixEpoch(25);
  TestAutofillClock test_clock(arbitrary_default_time);

  CreateCreditCards(
      /*include_local_credit_card=*/true,
      /*include_masked_server_credit_card=*/false,
      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = test::GetFormData(GetParam().form);
  const FormFieldData& first_field = form.fields()[0];

  base::HistogramTester histogram_tester;
  task_environment_.FastForwardBy(base::Milliseconds(37000));
  SeeForm(form);

  if (GetParam().step_0_focus) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    autofill_manager().OnFocusOnFormFieldImpl(form, first_field.global_id());
  }
  if (GetParam().step_1_click) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    autofill_manager().OnAskForValuesToFillTest(
        form, first_field.global_id(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);
  }

  if (GetParam().step_2_typing) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    SimulateUserChangedTextField(form, first_field, base::TimeTicks::Now());
  }
  if (GetParam().step_3_autofill) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    ASSERT_TRUE(first_field.parsed_autocomplete());
    HtmlFieldType autocomplete = first_field.parsed_autocomplete()->field_type;
    if (GroupTypeOfHtmlFieldType(autocomplete) == FieldTypeGroup::kAddress) {
      // This simulates the call to the renderer
      // (AutofillManager::FillOrPreviewProfileForm).
      FillTestProfile(form);
    } else if (GroupTypeOfHtmlFieldType(autocomplete) ==
               FieldTypeGroup::kCreditCard) {
      autofill_manager().AuthenticateThenFillCreditCardForm(
          form, first_field.global_id(),
          *personal_data().payments_data_manager().GetCreditCardByGUID(
              "10000000-0000-0000-0000-000000000001"),
          {.trigger_source = AutofillTriggerSource::kPopup});
    } else {
      // Autofill should not be simulated on a field that is not autofillable.
      ASSERT_TRUE(false);
    }
    // This simulates the callback from the renderer
    // (AutofillManager::OnDidFillAutofillFormData).
    FillAutofillFormData(form, base::TimeTicks::Now());
  }
  if (GetParam().step_4_edit_after_autofill) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    SimulateUserChangedTextField(form, first_field, base::TimeTicks::Now());
  }
  if (GetParam().step_5_submit) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
    SubmitForm(form);
  }
  // Record Autofill2.FocusedComplexForm UKM event at autofill manager / reset.
  test_api(autofill_manager()).Reset();

  // Verify UKM event for the form.
  auto interacted_entries = test_ukm_recorder().GetEntriesByName(
      UkmFocusedComplexFormType::kEntryName);
  const std::map<std::string, int64_t>& expected = GetParam().expected_metrics;
  if (expected.empty()) {
    EXPECT_THAT(interacted_entries, testing::IsEmpty());
    return;
  }
  ASSERT_EQ(1u, interacted_entries.size());
  const auto* const entry = interacted_entries[0].get();

  // +2 because the kFormSessionIdentifierName and kFormSignatureName are
  // computed dynamically.
  EXPECT_EQ(expected.size() + 2, entry->metrics.size());
  test_ukm_recorder().ExpectEntryMetric(
      entry, UkmFocusedComplexFormType::kFormSessionIdentifierName,
      AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()));
  test_ukm_recorder().ExpectEntryMetric(
      entry, UkmFocusedComplexFormType::kFormSignatureName,
      Collapse(CalculateFormSignature(form)).value());
  for (const auto& [metric, value] : expected) {
    test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
  }
}

TEST_F(FieldLogUkmMetricTest,
       LogAutofillFormWithExperimentalFieldsCountAtFormRemove) {
  base::FieldTrialParams feature_parameters{
      {features::kAutofillUKMExperimentalFieldsBucket0.name, "label1"},
      {features::kAutofillUKMExperimentalFieldsBucket4.name, "field2"},
  };
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kAutofillUKMExperimentalFields,
                             feature_parameters}},
      /*disabled_features=*/{});
  FormData form = test::GetFormData(test::FormDescription{
      .fields = {
          // This matches bucket 0, has a value, has typing -> gets reported.
          {.label = u"label1", .name_attribute = u"field1", .value = u"foo"},
          // This matches bucket 4, has a value, has typing -> get reported.
          {.label = u"", .name_attribute = u"field2", .value = u"foo"},
          // This matches bucket 4, has a value, has typing -> get reported.
          {.label = u"", .id_attribute = u"field2", .value = u"foo"},
          // This matches bucket 0, HAS NO VALUE, has typing
          // -> does NOT get reported.
          {.label = u"label1", .name_attribute = u"field3"},
          // This MATCHES NO BUCKET, has a value, has typing leading to empty
          // string.
          // -> does NOT get reported.
          {.label = u"label2", .name_attribute = u"field4", .value = u"foo"},
          // This matches bucket 0, has a value, HAS NO TYPING
          // -> does not gets reported.
          {.label = u"label1", .name_attribute = u"field1", .value = u"foo"},
      }});

  FormStructure form_structure(form);
  form_structure.field(0)->AppendLogEventIfNotRepeated(
      TypingFieldLogEvent{.has_value_after_typing = OptionalBoolean::kTrue});
  form_structure.field(1)->AppendLogEventIfNotRepeated(
      TypingFieldLogEvent{.has_value_after_typing = OptionalBoolean::kTrue});
  form_structure.field(2)->AppendLogEventIfNotRepeated(
      TypingFieldLogEvent{.has_value_after_typing = OptionalBoolean::kTrue});
  // Typing leads to empty string:
  form_structure.field(3)->AppendLogEventIfNotRepeated(
      TypingFieldLogEvent{.has_value_after_typing = OptionalBoolean::kFalse});
  form_structure.field(4)->AppendLogEventIfNotRepeated(
      TypingFieldLogEvent{.has_value_after_typing = OptionalBoolean::kTrue});
  // No typing on field 5.

  FormInteractionsUkmLogger logger(autofill_client_.get(),
                                   &test_ukm_recorder());
  logger.LogAutofillFormWithExperimentalFieldsCountAtFormRemove(form_structure);

  auto ukm_entries = test_ukm_recorder().GetEntriesByName(
      UkmSubmittedFormWithExperimentalFieldsType::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());
  const auto* const entry = ukm_entries[0].get();
  test_ukm_recorder().ExpectEntryMetric(
      entry,
      UkmSubmittedFormWithExperimentalFieldsType::kFormSessionIdentifierName,
      AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()));
  test_ukm_recorder().ExpectEntryMetric(
      entry,
      UkmSubmittedFormWithExperimentalFieldsType::
          kNumberOfNonEmptyExperimentalFields0Name,
      1);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry, UkmSubmittedFormWithExperimentalFieldsType::
                 kNumberOfNonEmptyExperimentalFields1Name));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry, UkmSubmittedFormWithExperimentalFieldsType::
                 kNumberOfNonEmptyExperimentalFields2Name));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry, UkmSubmittedFormWithExperimentalFieldsType::
                 kNumberOfNonEmptyExperimentalFields3Name));
  test_ukm_recorder().ExpectEntryMetric(
      entry,
      UkmSubmittedFormWithExperimentalFieldsType::
          kNumberOfNonEmptyExperimentalFields4Name,
      2);
}

}  // namespace
}  // namespace autofill::autofill_metrics
