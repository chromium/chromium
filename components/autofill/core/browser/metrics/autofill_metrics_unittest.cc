// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
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
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_canon.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

using ::autofill::test::AddFieldPredictionToForm;
using ::autofill::test::CreateTestFormField;
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
    FieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric);

}  // namespace autofill

namespace autofill::autofill_metrics {
namespace {

using mojom::SubmissionSource;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using PaymentsSigninState = AutofillMetrics::PaymentsSigninState;
using AutofillStatus = AutofillMetrics::AutofillStatus;

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
using UkmFieldTypeValidationType = ukm::builders::Autofill_FieldTypeValidation;
using UkmFieldFillStatusType = ukm::builders::Autofill_FieldFillStatus;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmEditedAutofilledFieldAtSubmission =
    ukm::builders::Autofill_EditedAutofilledFieldAtSubmission;
using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
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

void CreateSimpleForm(const GURL& origin, FormData& form) {
  form.set_host_frame(test::MakeLocalFrameToken());
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"TestForm");
  form.set_url(GURL("http://example.com/form.html"));
  form.set_action(GURL("http://example.com/submit.html"));
  form.set_main_frame_origin(url::Origin::Create(origin));
}

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  return base::Base64Encode(unencoded_response_string);
}

class AutofillMetricsTest : public AutofillMetricsBaseTest,
                            public testing::Test {
 public:
  using AutofillMetricsBaseTest::AutofillMetricsBaseTest;
  ~AutofillMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Params:
// 1. If the metrics are being logged for a form in an iframe or the main frame.
// True means the form is in the main frame.
// 2. Whether the form events logged are logged to all parsed form or just the
// interacted form.
class AutofillMetricsIFrameTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public AutofillMetricsTest {
 public:
  AutofillMetricsIFrameTest()
      : AutofillMetricsTest(
            /*is_in_any_main_frame=*/std::get<0>(GetParam())),
        credit_card_form_events_frame_histogram_(
            std::string("Autofill.FormEvents.CreditCard.") +
            (is_in_any_main_frame_ ? "IsInMainFrame" : "IsInIFrame")) {
    feature_list_.InitWithFeatureState(
        features::kAutofillEnableLogFormEventsToAllParsedFormTypes,
        std::get<1>(GetParam()));
  }

  CreditCard GetVirtualCreditCard(const std::string& guid) {
    CreditCard copy =
        *personal_data().payments_data_manager().GetCreditCardByGUID(guid);
    copy.set_record_type(CreditCard::RecordType::kVirtualCard);
    return copy;
  }

 protected:
  const std::string credit_card_form_events_frame_histogram_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillMetricsTest,
                         AutofillMetricsIFrameTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(AutofillMetricsTest, PerfectFilling_Addresses_CreditCards) {
  FormData address_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL,
                   .value = u"Elvis Aaron Presley",
                   .is_autofilled = true},
                  {.role = ADDRESS_HOME_CITY, .value = u"Munich"}}});
  FormData payments_form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FULL,
                   .value = u"Elvis Aaron Presley",
                   .is_autofilled = true},
                  {.role = CREDIT_CARD_NUMBER, .value = u"01230123012399"}}});
  FormData autocompleted_form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NUMBER,
                                     .value = u"01230123012399",
                                     .is_autofilled = true},
                                    {.role = ADDRESS_HOME_CITY,
                                     .value = u"Munich",
                                     .is_autofilled = true}}});
  test_api(payments_form).field(-1).set_is_user_edited(true);
  autofill_manager().AddSeenForm(address_form, {NAME_FULL, ADDRESS_HOME_LINE1});
  autofill_manager().AddSeenForm(payments_form,
                                 {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER});
  autofill_manager()
      .GetAutofillField(address_form, address_form.fields().front())
      ->set_filling_product(FillingProduct::kAddress);
  autofill_manager()
      .GetAutofillField(payments_form, payments_form.fields().front())
      ->set_filling_product(FillingProduct::kCreditCard);

  base::HistogramTester histogram_tester;
  // Upon submitting the address form, we expect logging a perfect address
  // filling.
  SubmitForm(address_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectTotalCount("Autofill.PerfectFilling.CreditCards", 0);
  // Upon submitting the payments form, we expect logging a perfect address
  // filling, since one of the fields was user edited.
  SubmitForm(payments_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.CreditCards", 0,
                                      1);
  // Upon submitting the autocompleted form, we expect not logging anything for
  // both metrics, since the product of filling the form is neither addresses
  // nor credit cards.
  SubmitForm(autocompleted_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.CreditCards", 0,
                                      1);
}

// Test that we log the skip decisions for hidden/representational fields
// correctly.
TEST_F(AutofillMetricsTest, LogHiddenRepresentationalFieldSkipDecision) {
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

// Test that we log the address line fields whose server types are rationalized
TEST_F(AutofillMetricsTest, LogRepeatedAddressTypeRationalized) {
  FormData form = CreateEmptyForm();

  FieldSignature field_signature[2];

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"fullname");
  field.set_name(u"fullname");
  test_api(form).Append(field);

  field.set_label(u"Street 1");
  field.set_name(u"street1");
  test_api(form).Append(field);
  field_signature[0] = Collapse(CalculateFieldSignatureForField(field));

  field.set_label(u"Street 2");
  field.set_name(u"street2");
  test_api(form).Append(field);
  field_signature[1] = Collapse(CalculateFieldSignatureForField(field));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  FormStructure form_structure(form);

  std::vector<FieldType> field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  autofill_manager().AddSeenForm(form, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], ADDRESS_HOME_STREET_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[2], ADDRESS_HOME_STREET_ADDRESS,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  ParseServerPredictionsQueryResponse(
      response_string, {&form_structure},
      test::GetEncodedSignatures({&form_structure}),
      autofill_manager().form_interactions_ukm_logger(), nullptr);

  ASSERT_EQ(test_ukm_recorder()
                .GetEntriesByName(
                    UkmLogRepeatedServerTypePredictionRationalized::kEntryName)
                .size(),
            (size_t)2);

  VerifyUkm(
      &test_ukm_recorder(), form,
      UkmLogRepeatedServerTypePredictionRationalized::kEntryName,
      {{{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         form_signature.value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[0].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Country");
  field.set_name(u"country");
  test_api(form).Append(field);
  field_signature[0] = Collapse(CalculateFieldSignatureForField(field));

  field.set_label(u"fullname");
  field.set_name(u"fullname");
  test_api(form).Append(field);

  field.set_label(u"State");
  field.set_name(u"state");
  test_api(form).Append(field);
  field_signature[2] = Collapse(CalculateFieldSignatureForField(field));

  field.set_label(u"State");
  field.set_name(u"state");
  field.set_is_focusable(false);
  field.set_form_control_type(FormControlType::kSelectOne);
  test_api(form).Append(field);
  // Regardless of the order of appearance, hidden fields are rationalized
  // before their corresponding visible one.
  field_signature[1] = Collapse(CalculateFieldSignatureForField(field));

  FormSignature form_signature = Collapse(CalculateFormSignature(form));

  FormStructure form_structure(form);

  std::vector<FieldType> field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    field_types.push_back(UNKNOWN_TYPE);

  autofill_manager().AddSeenForm(form, field_types);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], ADDRESS_HOME_COUNTRY,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(form.fields()[2], ADDRESS_HOME_COUNTRY,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[3], ADDRESS_HOME_COUNTRY,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  ParseServerPredictionsQueryResponse(
      response_string, {&form_structure},
      test::GetEncodedSignatures({&form_structure}),
      autofill_manager().form_interactions_ukm_logger(), nullptr);

  ASSERT_EQ(test_ukm_recorder()
                .GetEntriesByName(
                    UkmLogRepeatedServerTypePredictionRationalized::kEntryName)
                .size(),
            (size_t)3);

  VerifyUkm(
      &test_ukm_recorder(), form,
      UkmLogRepeatedServerTypePredictionRationalized::kEntryName,
      {{{UkmLogRepeatedServerTypePredictionRationalized::kFormSignatureName,
         *form_signature},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldSignatureName,
         field_signature[0].value()},
        {UkmLogRepeatedServerTypePredictionRationalized::kFieldTypeGroupName,
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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
         static_cast<int64_t>(FieldTypeGroup::kAddress)},
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

// Ensures that metrics that measure timing some important Autofill functions
// actually are recorded and retrieved.
TEST_F(AutofillMetricsTest, TimingMetrics) {
  base::HistogramTester histogram_tester;
  FormData form = CreateForm(
      {CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                           FormControlType::kInputText),
       CreateTestFormField("Autofill Failed", "autofillfailed",
                           "buddy@gmail.com", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "2345678901",
                           FormControlType::kInputTelephone)});
  test_api(form).field(0).set_is_autofilled(true);
  test_api(form).field(1).set_is_autofilled(false);
  test_api(form).field(2).set_is_autofilled(false);

  SeeForm(form);

  // Because these metrics are related to timing, it is not possible to know in
  // advance which bucket the sample will fall into, so we just need to make
  // sure we have valid samples.
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.DetermineHeuristicTypes")
          .empty());
  EXPECT_FALSE(histogram_tester.GetAllSamples("Autofill.Timing.ParseFormsAsync")
                   .empty());
  EXPECT_FALSE(
      histogram_tester
          .GetAllSamples("Autofill.Timing.ParseFormsAsync.RunHeuristics")
          .empty());
  EXPECT_FALSE(histogram_tester
                   .GetAllSamples("Autofill.Timing.ParseFormsAsync.UpdateCache")
                   .empty());
}

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(AutofillMetricsTest, SaneMetricsWithCacheMismatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillFixValueSemantics,
       features::kAutofillFixInitialValueOfSelect,
       features::kAutofillFixCurrentValueInImport,
       // Enable model predictions but don't make it the active source.
       features::kAutofillModelPredictions},
      {});
  FormData form = CreateForm(
      {CreateTestFormField("Both match", "match", "Elvis Aaron Presley",
                           FormControlType::kInputText),
       CreateTestFormField("Both mismatch", "mismatch", "buddy@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Only heuristics match", "mixed", "Memphis",
                           FormControlType::kInputText),
       CreateTestFormField("Unknown", "unknown", "garbage",
                           FormControlType::kInputText)});
  test_api(form).field(0).set_is_autofilled(true);

  std::vector<FieldType> heuristic_types = {NAME_FULL, PHONE_HOME_NUMBER,
                                            ADDRESS_HOME_CITY, UNKNOWN_TYPE};
  std::vector<FieldType> server_types = {NAME_FULL, PHONE_HOME_NUMBER,
                                         PHONE_HOME_NUMBER, UNKNOWN_TYPE};
  std::vector<FieldType> ml_types = server_types;

  std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
      all_heuristic_types;
  ASSERT_EQ(heuristic_types.size(), ml_types.size());
  for (size_t i = 0; i < heuristic_types.size(); ++i) {
    all_heuristic_types.push_back(
        {{GetActiveHeuristicSource(), heuristic_types[i]},
         {HeuristicSource::kMachineLearning, ml_types[i]}});
  }

  autofill_manager().AddSeenForm(test::WithoutValues(form), all_heuristic_types,
                                 server_types);

  // Add a field and re-arrange the remaining form fields before submitting. The
  // five submitted fields are filled with
  // - EMPTY_TYPE (Tennessee) - While this is an ADDRESS_HOME_STATE in theory,
  //     this field is added at runtime. As the value "Tennessee" is seen
  //     for the first time when the form is submitted, the field's initial
  //     value equates the current value. Therefore, the field is considered as
  //     not-typed and therefore empty. Also no ML heuristics are executed on
  //     the field because it just appears at form submission time.
  // - ADDRESS_HOME_CITY (Memphis)
  // - EMAIL_ADDRESS (buddy@gmail.com)
  // - garbage
  // - NAME_FULL (Elvis Aaron Presley)
  std::vector<FormFieldData> cached_fields = form.fields();
  form.set_fields({CreateTestFormField("New field", "new field", "Tennessee",
                                       FormControlType::kInputText),
                   cached_fields[2], cached_fields[1], cached_fields[3],
                   cached_fields[0]});
  std::vector<FieldType> actual_types = {NAME_FULL, EMAIL_ADDRESS,
                                         ADDRESS_HOME_CITY, UNKNOWN_TYPE};

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<std::string> sources = {"Heuristic", "Server", "Overall"};
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Quality metrics for ".ML" are only recorded if the ML predictions are
  // computed but not the active heuristic source.
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      GetActiveHeuristicSource() != HeuristicSource::kMachineLearning) {
    sources.push_back("ML");
  }
#endif

  for (const std::string& source : sources) {
    SCOPED_TRACE(testing::Message() << source);
    using FieldTypeQualityMetric = AutofillMetrics::FieldTypeQualityMetric;
    using enum FieldTypeQualityMetric;

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FieldPredictionQuality.Aggregate." + source),
        BucketsAre(
            Bucket(TRUE_NEGATIVE_UNKNOWN, 1), Bucket(TRUE_NEGATIVE_EMPTY, 1),
            Bucket(TRUE_POSITIVE, source == "Heuristic" ? 2 : 1),
            Bucket(FALSE_NEGATIVE_MISMATCH, source == "Heuristic" ? 1 : 2)));

    auto b = [](FieldType type, FieldTypeQualityMetric metric,
                size_t count = 1) {
      return Bucket(GetFieldTypeGroupPredictionQualityMetric(type, metric),
                    count);
    };
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.FieldPredictionQuality.ByFieldType." + source),
                BucketsAre(b(ADDRESS_HOME_CITY, source == "Heuristic"
                                                    ? TRUE_POSITIVE
                                                    : FALSE_NEGATIVE_MISMATCH),
                           b(PHONE_HOME_NUMBER, FALSE_POSITIVE_MISMATCH,
                             source != "Heuristic" ? 2 : 1),
                           b(EMAIL_ADDRESS, FALSE_NEGATIVE_MISMATCH),
                           b(NAME_FULL, TRUE_POSITIVE)));

    std::vector<FieldType>& predicted_type = [&]() -> std::vector<FieldType>& {
      if (source == "Heuristic") {
        return heuristic_types;
      } else if (source == "Server") {
        return server_types;
      } else if (source == "Overall") {
        return server_types;
      } else if (source == "ML") {
        return ml_types;
      }
      NOTREACHED_NORETURN();
    }();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FieldPrediction." + source),
        // The first field has the ML prediction type NO_SERVER_DATA because the
        // ML predictions were never executed and NO_SERVER_DATA is used to
        // indicate that a specific heuristic type is unset.
        BucketsAre(source == "Server" || source == "ML"
                       ? Bucket((NO_SERVER_DATA << 16) | EMPTY_TYPE, 1)
                       : Bucket((UNKNOWN_TYPE << 16) | EMPTY_TYPE, 1),
                   Bucket((predicted_type[0] << 16) | actual_types[0], 1),
                   Bucket((predicted_type[1] << 16) | actual_types[1], 1),
                   Bucket((predicted_type[2] << 16) | actual_types[2], 1),
                   Bucket((predicted_type[3] << 16) | actual_types[3], 1)));
  }
}

// Verify that when submitting an autofillable form, the stored profile metric
// is logged.
TEST_F(AutofillMetricsTest, StoredProfileCountAutofillableFormSubmission) {
  // Three fields is enough to make it an autofillable form.
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "", FormControlType::kInputText)});

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
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  SubmitForm(form);

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Verify that when submitting an autofillable form, the proper type of
// the edited fields is correctly logged to UKM.
TEST_F(AutofillMetricsTest, TypeOfEditedAutofilledFieldsUkmLogging) {
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
      .renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  SimulateUserChangedTextField(form, form.fields()[0]);
  SimulateUserChangedTextField(form, form.fields()[1]);

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

// Verify that we correctly log metrics regarding developer engagement.
TEST_F(AutofillMetricsTest, DeveloperEngagement) {
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText)});

  // Ensure no metrics are logged when small form support is disabled (min
  // number of fields enforced).
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    test_api(autofill_manager()).Reset();
    histogram_tester.ExpectTotalCount("Autofill.DeveloperEngagement", 0);
  }

  // Add another field to the form, so that it becomes fillable.
  test_api(form).Append(
      CreateTestFormField("Phone", "phone", "", FormControlType::kInputText));

  // Expect the "form parsed without hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    test_api(autofill_manager()).Reset();
    histogram_tester.ExpectUniqueSample(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS, 1);
  }

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  test_api(form).Append(CreateTestFormField(
      "", "", "", FormControlType::kInputText, "given-name"));
  test_api(form).Append(
      CreateTestFormField("", "", "", FormControlType::kInputText, "email"));
  test_api(form).Append(CreateTestFormField(
      "", "", "", FormControlType::kInputText, "address-line1"));

  // Expect the "form parsed with field type hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    test_api(autofill_manager()).Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);
  }
}

// Verify that we correctly log UKM for form parsed without type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithoutTypeHints) {
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText)});

  // Ensure no entries are logged when loading a non-fillable form.
  {
    SeeForm(form);
    test_api(autofill_manager()).Reset();

    EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
  }

  // Add another field to the form, so that it becomes fillable.
  test_api(form).Append(
      CreateTestFormField("Phone", "phone", "", FormControlType::kInputText));

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    SeeForm(form);
    test_api(autofill_manager()).Reset();

    VerifyDeveloperEngagementUkm(
        &test_ukm_recorder(), form, /*is_for_credit_card=*/false,
        {FormTypeNameForLogging::kAddressForm},
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
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "", FormControlType::kInputText),
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "given-name"),
       CreateTestFormField("", "", "", FormControlType::kInputText, "email"),
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "address-line1")});

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    SeeForm(form);
    test_api(autofill_manager()).Reset();

    VerifyDeveloperEngagementUkm(
        &test_ukm_recorder(), form, /*is_for_credit_card=*/false,
        {FormTypeNameForLogging::kAddressForm},
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS});
  }
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardMetrics) {
  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded one_month_ago_exploded;
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type: 1 of each for local
  // and 2 of each for masked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
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
      auto& repo = (record_type == CreditCard::RecordType::kLocalCard)
                       ? local_cards
                       : server_cards;
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
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);

  // Validate the disused count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardDisusedCount", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardDisusedCount", 3,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 2, 1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 6);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 2);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 4);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 30, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 200, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 30, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 200, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 200, 2);
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
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
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
      auto& repo = (record_type == CreditCard::RecordType::kLocalCard)
                       ? local_cards
                       : server_cards;
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
      "Autofill.StoredCreditCardCount.Server.WithNickname", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Local.WithNickname", 1, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithNickname", 2, 1);
}

// Tests that local cards with invalid card numbers are correctly logged.
TEST_F(AutofillMetricsTest, LogStoredCreditCardWithInvalidCardNumberMetrics) {
  // Only local cards can have invalid card numbers.
  CreditCard card_with_valid_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_valid_card_number.SetNumber(u"4444333322221111");
  CreditCard card_with_invalid_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_invalid_card_number.SetNumber(u"4444333322221115");
  CreditCard card_with_non_digit_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_non_digit_card_number.SetNumber(u"invalid_number");

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_valid_card_number));
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_non_digit_card_number));
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_invalid_card_number));
  std::vector<std::unique_ptr<CreditCard>> server_cards;

  // Log the stored credit card metrics for the cards configured above.
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/0,
      base::Days(180));

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredCreditCardCount.Local.WithInvalidCardNumber", 2, 1);
}

// Test that the credit card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, CreditCardCheckoutFlowUserActions) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Disable mandatory reauth as it is not part of this test and will
  // interfere with the card retrieval flow.
  personal_data()
      .payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(false);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_NUMBER,
                                        CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Name on card" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),

        AutofillSuggestionTriggerSource::kFormControlElementClicked);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestLocalCardId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field along with a "Clear form" footer suggestion.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

#if !BUILDFLAG(IS_IOS)
  // Simulate selecting an "Undo autofill" suggestion.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);

    external_delegate().DidAcceptSuggestion(
        Suggestion(SuggestionType::kUndoOrClear),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(
        1, user_action_tester.GetActionCount("Autofill_UndoPaymentsAutofill"));
  }
#endif

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field, this time to submit the form.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestLocalCardId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a credit card suggestion.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledCreditCardSuggestion"));
  }

  // Simulate submitting the credit card form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
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
       Collapse(CalculateFieldSignatureForField(form.fields()[0])).value()},
      {UkmSuggestionsShownType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  ExpectedUkmMetricsRecord number_field_record{
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
      {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
       HtmlFieldType::kUnspecified},
      {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
      {UkmSuggestionsShownType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields()[1])).value()},
      {UkmSuggestionsShownType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  VerifyUkm(&test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
            {name_field_record, number_field_record, number_field_record,
             number_field_record});

  // Expect 3 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate().DidAcceptSuggestion|. Second and third, from
  // ExpectedUkmMetrics |autofill_manager().AuthenticateThenFillCreditCardForm|.
  ExpectedUkmMetricsRecord from_did_accept_suggestion{
      {UkmSuggestionFilledType::kRecordTypeName,
       base::to_underlying(CreditCard::RecordType::kLocalCard)},
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmSuggestionFilledType::kIsForCreditCardName, true},
      {UkmSuggestionFilledType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields().front()))
           .value()},
      {UkmSuggestionFilledType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  ExpectedUkmMetricsRecord from_fill_or_preview_form{
      {UkmSuggestionFilledType::kRecordTypeName,
       base::to_underlying(CreditCard::RecordType::kLocalCard)},
      {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
      {UkmSuggestionFilledType::kIsForCreditCardName, true},
      {UkmSuggestionFilledType::kFieldSignatureName,
       Collapse(CalculateFieldSignatureForField(form.fields().front()))
           .value()},
      {UkmSuggestionFilledType::kFormSignatureName,
       Collapse(CalculateFormSignature(form)).value()}};
  VerifyUkm(&test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
            {from_did_accept_suggestion, from_fill_or_preview_form,
             from_fill_or_preview_form});
}

// Test that the profile checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, ProfileCheckoutFlowUserActions) {
  RecreateProfile();

  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a profile field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "State" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "City" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate selecting a profile suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestProfileId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

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
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
  }

  VerifyUkm(
      &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_STATE},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_STATE},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[0])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_CITY},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
         HtmlFieldType::kUnspecified},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_CITY},
        {UkmSuggestionsShownType::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[1])).value()},
        {UkmSuggestionsShownType::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}}});
  // Expect 2 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate().DidAcceptSuggestion|. Second, from call to
  // |autofill_manager().FillOrPreviewProfileForm|.
  VerifyUkm(&test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
            {{{UkmSuggestionFilledType::kIsForCreditCardName, false},
              {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
              {UkmSuggestionFilledType::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields().front()))
                   .value()},
              {UkmSuggestionFilledType::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}},
             {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
              {UkmSuggestionFilledType::kIsForCreditCardName, false},
              {UkmSuggestionsShownType::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields().front()))
                   .value()},
              {UkmSuggestionsShownType::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}}});
}

// Tests that the Autofill_PolledCreditCardSuggestions user action is only
// logged once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledCreditCardSuggestions_DebounceLogs) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_NUMBER,
                                        CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a second query on the same field. There should still only be one
  // logged poll.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[1].global_id());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));
}

// Tests that the Autofill.QueriedCreditCardFormIsSecure histogram is logged
// properly.
TEST_F(AutofillMetricsTest, QueriedCreditCardFormIsSecure) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  {
    // Simulate having seen this insecure form on page load.
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("http://example.com/form.html"));
    form.set_action(GURL("http://example.com/submit.html"));
    // In order to test that the QueriedCreditCardFormIsSecure is logged as
    // false, we need to set the main frame origin, otherwise this fill is
    // skipped due to the form being detected as mixed content.
    GURL client_form_origin = autofill_client_->form_origin();
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpScheme);
    autofill_client_->set_form_origin(
        client_form_origin.ReplaceComponents(replacements));
    form.set_main_frame_origin(
        url::Origin::Create(autofill_client_->form_origin()));
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTP, non-secure
    // form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", false, 1);
    // Reset the main frame origin to secure for other tests
    autofill_client_->set_form_origin(client_form_origin);
  }

  {
    test_api(autofill_manager()).Reset();
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("https://example.com/form.html"));
    form.set_action(GURL("https://example.com/submit.html"));
    form.set_main_frame_origin(
        url::Origin::Create(autofill_client_->form_origin()));
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTPS form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", true, 1);
  }
}

// Tests that the Autofill_PolledProfileSuggestions user action is only logged
// once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledProfileSuggestions_DebounceLogs) {
  RecreateProfile();

  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a profile field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a second query on the same field. There should still only be poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[1].global_id());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));
}

// Test that we log parsed form event for credit card forms.
TEST_P(AutofillMetricsIFrameTest, CreditCardParsedFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Card Number", "card_number", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration", "cc_exp", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Verification", "verification", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_EXP_MONTH,
                                        CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that events of standalone CVC forms are only logged to
// Autofill.FormEvents.StandaloneCvc and not to Autofill.FormEvents.CreditCard.
TEST_P(AutofillMetricsIFrameTest, StandaloneCvcParsedFormEvents) {
  FormData form = CreateForm({CreateTestFormField(
      "Standalone Cvc", "CVC", "", FormControlType::kInputText)});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types);

  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.StandaloneCvc",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_DID_PARSE_FORM, 0);
}

// Test that we log interacted form event for credit cards related.
TEST_P(AutofillMetricsIFrameTest, CreditCardInteractedFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    histogram_tester.ExpectUniqueSample(
        credit_card_form_events_frame_histogram_, FORM_EVENT_INTERACTED_ONCE,
        1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardShownFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kAutocompleteEntry);
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
  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_VERIFICATION_CODE, CREDIT_CARD_NUMBER};

  // Creating cards, including a virtual card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card*/ true);

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form,
                               /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kAutocompleteEntry);
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
                      /*masked_card_is_enrolled_for_virtual_card*/ false);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load. Suggestions shown should be
    // logged, but suggestions shown with virtual card should not.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
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
// TODO(crbug.com/362889813): Refactor the nested test cases into separate
// tests.
TEST_P(AutofillMetricsIFrameTest, CreditCardSelectedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a local card suggestion multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    credit_card_form_events_frame_histogram_),
                BucketsInclude(
                    Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED, 2),
                    Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }
  {
    // Simulating selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a masked server card multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual server suggestion by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2], virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields()[2],
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual card multiple times.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2], virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields()[2],
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
                                   /*is_virtual_card=*/true);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[2], virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields()[2],
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
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

// Test 1 of 2 for crbug/1513307, to ensure legacy deprecated metrics are not
// logged if card number is not filled and an unmask request is not sent, but
// that the bugfix's new metric buckets are indeed logged.
TEST_P(AutofillMetricsIFrameTest,
       CreditCardSelectedLegacyFormEvents_NotLoggedIfNoCardNumberFieldExists) {
  // We only care about masked server card for this test, so only create that.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  // Create a form *without* a card number, to avoid triggering an unmask
  // request to Payments. The deprecated metrics should not be logged.
  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_EXP_MONTH,
                                        CREDIT_CARD_EXP_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[0],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   0),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   0),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                0)));

    // Simulate accepting the suggestion again to test ONCE vs. non-ONCE
    // metrics.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[0],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   0),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   0),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                0)));
  }
}

// Test 2 of 2 for crbug/1513307, to ensure legacy deprecated metrics are logged
// if card number is filled and an unmask request is sent, along with the
// bugfix's new metric buckets.
TEST_P(AutofillMetricsIFrameTest,
       CreditCardSelectedLegacyFormEvents_LoggedIfCardNumberFieldExists) {
  // We only care about masked server card for this test, so only create that.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  // Second, create a form *with* a card number, and ensure the deprecated
  // metrics are also logged.
  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[0],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   1),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   1),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                1)));

    // Simulate accepting the suggestion again to test ONCE vs. non-ONCE
    // metrics.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields()[0],
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   2),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                   2),
            Bucket(
                DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
                1)));
  }
}

// Test that we log filled form events for credit cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardFilledFormEvents) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Disable mandatory reauth as it is not part of this test and will
  // interfere with the card retrieval flow.
  personal_data()
      .payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(false);
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a virtual card suggestion by selecting the option
    // based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(), virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().front(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().front(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424");
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
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
// server and local cards exist.
TEST_P(
    AutofillMetricsIFrameTest,
    CreditCardFilledFormEventsUsingUniqueLocalCardWhenOtherDuplicateServerCardsPresent) {
  // Clearing all the existing cards and creating a local credit card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestLocalCardId;

  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a unique local card suggestion.
  base::HistogramTester histogram_tester;
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form, form.fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      {.trigger_source = AutofillTriggerSource::kPopup});

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

// Test to log when a server card is autofilled and a local card with the same
// number exists.
TEST_P(AutofillMetricsIFrameTest,
       CreditCardFilledFormEvents_UsingServerCard_WithLocalDuplicate) {
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestDuplicateMaskedCardId;
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a server card suggestion with a duplicate local card.
  base::HistogramTester histogram_tester;
  // Server card with a duplicate local card present at index 0.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form, form.fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields().back().global_id());
  DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "5454545454545454");
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(credit_card_form_events_frame_histogram_),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1)));
}

// Test to log when a unique server card is autofilled and a different server
// card suggestion has the same number as a local card. That is, for local card
// A and server card B with the same number, this fills unrelated server card C.
TEST_P(AutofillMetricsIFrameTest,
       CreditCardFilledFormEvents_UsingServerCard_WithoutLocalDuplicate) {
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestMaskedCardId;
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a server card suggestion with a duplicate local card.
  base::HistogramTester histogram_tester;
  // Server card with a duplicate local card present at index 0.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form, form.fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields().back().global_id());
  DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 0),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(credit_card_form_events_frame_histogram_),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 0),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0)));
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_ServerCard) {
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Success", 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kPermanentFailure, std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Failure", 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kClientSideTimeout, std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.ClientSideTimeout",
        1);
  }
}

// Test that a malformed or non-HTTP_OK response doesn't cause problems, per
// crbug/1267105.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_BadServerResponse) {
  // Creating masked card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  // Set up our form data.
  FormData form = test::CreateTestCreditCardFormData(
      /*is_https=*/true,
      /*use_month_type=*/true,
      /*split_names=*/false);
  std::vector<FieldType> field_types{CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                     CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     CREDIT_CARD_VERIFICATION_CODE};
  ASSERT_EQ(form.fields().size(), field_types.size());

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
        PaymentsRpcResult::kTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

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
        PaymentsRpcResult::kPermanentFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

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
        PaymentsRpcResult::kClientSideTimeout,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kSuccess,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount("Autofill.UnmaskPrompt.GetRealPanResult",
                                       AutofillMetrics::PAYMENTS_RESULT_SUCCESS,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
  }
}

TEST_F(AutofillMetricsTest, CreditCardGetRealPanResult_VirtualCard) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kClientSideTimeout,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kSuccess,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "411111111",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4444444444444444",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "5105105105105100",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4111111111111111",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4111111111111111",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown and selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form, form.fields().back(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(
          kTestLocalCardId),
      {.trigger_source = AutofillTriggerSource::kPopup});

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
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with no filled data.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
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
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
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

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
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
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown. Form is submitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    test_api(autofill_manager()).Reset();
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
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kLocalCard)},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(), virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().front(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
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
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kVirtualCard)},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().back(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424");
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

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kMaskedServerCard)},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().back()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
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
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }
}

// Test that we log "will submit" and "submitted" form events for credit
// cards.
TEST_P(AutofillMetricsIFrameTest, CreditCardWillSubmitFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(), virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().front(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424",
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnCreditCardFetchingSuccessful(form, form.fields().back(),
                                   AutofillTriggerSource::kPopup,
                                   u"6011000990139424");
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
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            credit_card_form_events_frame_histogram_),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
  }
}

// Parametrized test class to test
// kAutofillEnableLogFormEventsToAllParsedFormTypes and ensure form event
// logging still works in the appropriate histograms when logging to parsed form
// types on a webpage.
class AutofillMetricsTestWithParsedFormLogging
    : public AutofillMetricsTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillMetricsTestWithParsedFormLogging() = default;
  ~AutofillMetricsTestWithParsedFormLogging() override = default;

  void SetUp() override {
    AutofillMetricsTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableLogFormEventsToAllParsedFormTypes, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillMetricsTestWithParsedFormLogging,
                         testing::Bool());

// Test that we log form events for masked server card with offers.
TEST_P(AutofillMetricsTestWithParsedFormLogging, LogServerOfferFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  // Creating all kinds of cards. None of them have offers.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
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
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().front(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[0]),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
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
                                        /*sample=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/true, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select another card, and still log to offer
    // sub-histogram because user has another masked server card with offer.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
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
                                        /*sample=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/false, 1);
  }

  // Recreate cards and add card that is linked to an expired offer.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[1], "$4",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x3fff, /*offer_expired=*/true);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating activating the autofill popup for the credit card field,
    // new popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the card with linked offer, though metrics should not record it
    // since the offer is expired.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[1]),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
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
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[2], "$5",
                                     autofill_client_->form_origin(),
                                     /*id=*/0x5fff);

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
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
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");

    // Simulate user showing suggestions but then submitting form with
    // previously filled card info.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
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
                                       /*sample=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/true, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // failing the CVC check and submitting the form anyways. Verify that all
    // related form events are correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer, but fail the CVC
    // check.
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kPermanentFailure, std::string());

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
                                        /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SubmittedCardHasOffer",
                                       /*sample=*/true, 0);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // selecting a local card instead. Verify that all related form events are
    // correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;

    // Show suggestions and select the card with offer.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        {.trigger_source = AutofillTriggerSource::kPopup});
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");

    // Show suggestions again, and select a local card instead.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().AuthenticateThenFillCreditCardForm(
        form, form.fields().back(),
        *personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        {.trigger_source = AutofillTriggerSource::kPopup});
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
                                       /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*sample=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/false, 1);
  }
}

// Test that we log parsed form events for address and cards in the same form.
TEST_P(AutofillMetricsTestWithParsedFormLogging, MixedParsedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "", FormControlType::kInputText),
       CreateTestFormField("Card Number", "card_number", "",
                           FormControlType::kInputText),
       CreateTestFormField("Expiration", "cc_exp", "",
                           FormControlType::kInputText),
       CreateTestFormField("Verification", "verification", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE,          ADDRESS_HOME_CITY,
      ADDRESS_HOME_STREET_ADDRESS, CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_EXP_MONTH,       CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that we log parsed form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressParsedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                      FORM_EVENT_DID_PARSE_FORM, 1);

  // Check if FormEvent UKM is logged properly
  auto entries =
      test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  VerifyUkm(
      &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
      {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
        {UkmFormEventType::kFormTypesName,
         AutofillMetrics::FormTypesToBitVector(
             {FormTypeNameForLogging::kAddressForm,
              FormTypeNameForLogging::kPostalAddressForm})},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
}

// Test that we log interacted form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressInteractedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_INTERACTED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(2u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_INTERACTED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }
}

// Test that we log suggestion shown form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressShownFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    DidShowAutofillSuggestions(form);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(4u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_SUGGESTIONS_SHOWN},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kAutocompleteEntry);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(1u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }
}

// Test that we log filled form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressFilledFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

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
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
    VerifyUkm(
        &test_ukm_recorder(), form, UkmFormEventType::kEntryName,
        {{{UkmFormEventType::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_LOCAL_SUGGESTION_FILLED},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormEventType::kAutofillFormEventName,
           FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE},
          {UkmFormEventType::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm})},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
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
}

// Test that we log submitted form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressSubmittedFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data. Form is submitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    test_api(autofill_manager()).Reset();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion show but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
  }
}

// Test that we log "will submit" and "submitted" form events for address.
TEST_P(AutofillMetricsTestWithParsedFormLogging, AddressWillSubmitFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(4u, entries.size());
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
  }
}

// A site can have two different <form> elements, one for an address and one
// for a credit card. It's common that only one of these forms receives a
// submit event, while the website actually submitted both. Test that
// the submit events are recorded for both of Autofill.FormEvents.{Address,
// CreditCard} after a submit event on the credit card form.
TEST_P(AutofillMetricsTestWithParsedFormLogging,
       SeparateCreditCardAndAddressForm_CreditCardSubmitted) {
  base::HistogramTester histogram_tester;
  FormData address_form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  FormData credit_card_form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  SeeForm(address_form);
  SeeForm(credit_card_form);
  // Show suggestions first as a prerequisite for
  // FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE gets logged.
  DidShowAutofillSuggestions(address_form, /*field_index=*/0,
                             SuggestionType::kAddressEntry);
  autofill_manager().OnAskForValuesToFillTest(
      address_form, address_form.fields().back().global_id());
  DidShowAutofillSuggestions(credit_card_form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(
      credit_card_form, credit_card_form.fields().back().global_id());
  SubmitForm(credit_card_form);

  size_t expected_address_count =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLogFormEventsToAllParsedFormTypes)
          ? 1
          : 0;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE,
                            expected_address_count),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
                            expected_address_count)));
}

// A site can have two different <form> elements, one for an address and one
// for a credit card. It's common that only one of these forms receives a
// submit event, while the website actually submitted both. Test that
// the submit events are recorded for both of Autofill.FormEvents.{Address,
// CreditCard} after a submit event on the Address form.
TEST_P(AutofillMetricsTestWithParsedFormLogging,
       SeparateCreditCardAndAddressForm_AddressSubmitted) {
  base::HistogramTester histogram_tester;
  FormData address_form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  FormData credit_card_form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  SeeForm(address_form);
  SeeForm(credit_card_form);
  DidShowAutofillSuggestions(address_form, /*field_index=*/0,
                             SuggestionType::kAddressEntry);
  autofill_manager().OnAskForValuesToFillTest(
      address_form, address_form.fields().back().global_id());
  DidShowAutofillSuggestions(credit_card_form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(
      credit_card_form, credit_card_form.fields().back().global_id());
  SubmitForm(address_form);

  size_t expected_credit_card_count =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLogFormEventsToAllParsedFormTypes)
          ? 1
          : 0;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE,
                            expected_credit_card_count),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
                            expected_credit_card_count)));
}

// Test that we log the phone field.
TEST_F(AutofillMetricsTest, RecordStandalonePhoneField) {
  FormData form = CreateForm({CreateTestFormField(
      "Phone", "phone", "", FormControlType::kInputTelephone)});

  std::vector<FieldType> field_types = {PHONE_HOME_NUMBER};
  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_INTERACTED_ONCE, 1);
}

// Test that we log interacted form event for credit cards only once.
TEST_F(AutofillMetricsTest, CreditCardFormEventsAreSegmented) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_manager()).Reset();
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }
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
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.set_use_date(AutofillClock::Now() - base::Days(13));
  profile.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.Profile", 13,
                                     1);
}

// Test that we log the verification status of name tokens.
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfNameTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"First Last",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"First",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Last",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Last",
                                           VerificationStatus::kParsed);

  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(profile);

  std::string base_histogram =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histogram + "Full",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "First",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "Last",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "SecondLast",
                                      VerificationStatus::kParsed, 1);

  histogram_tester.ExpectTotalCount(base_histogram + "Middle", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "FirstLast", 0);

  histogram_tester.ExpectTotalCount(base_histogram + "Any", 4);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kObserved, 1);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kParsed, 3);
}

// Test that we log the verification status of address tokens..
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfAddressTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"123 StreetName",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"123",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"StreetName", VerificationStatus::kObserved);

  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(profile);

  std::string base_histogram =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histogram + "StreetAddress",
                                      VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "StreetName",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "HouseNumber",
                                      VerificationStatus::kObserved, 1);

  histogram_tester.ExpectTotalCount(base_histogram + "FloorNumber", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "ApartmentNumber", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "Premise", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "SubPremise", 0);

  histogram_tester.ExpectTotalCount(base_histogram + "Any", 3);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kObserved, 2);
}

// Verify that we correctly log metrics tracking the duration of form fill.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  FormData empty_form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "", FormControlType::kInputText)});

  FormData filled_form = empty_form;
  test_api(filled_form).field(0).set_value(u"Elvis Aaron Presley");
  test_api(filled_form).field(1).set_value(u"theking@gmail.com");
  test_api(filled_form).field(2).set_value(u"12345678901");

  // Fill additional form.
  FormData second_form = empty_form;
  second_form.set_host_frame(test::MakeLocalFrameToken());
  second_form.set_renderer_id(test::MakeFormRendererId());
  test_api(second_form)
      .Append(CreateTestFormField("Second Phone", "second_phone", "",
                                  FormControlType::kInputText));

  // Fill the field values for form submission.
  test_api(second_form).field(0).set_value(u"Elvis Aaron Presley");
  test_api(second_form).field(1).set_value(u"theking@gmail.com");
  test_api(second_form).field(2).set_value(u"12345678901");
  test_api(second_form).field(3).set_value(u"51512345678");

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    SCOPED_TRACE("Test 1 - no interaction, fields are prefilled");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(filled_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    test_api(autofill_manager()).Reset();
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
                                 user_filled_form.fields().front(),
                                 parse_time + base::Microseconds(3));
    task_environment_.FastForwardBy(base::Microseconds(17));
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
    test_api(autofill_manager()).Reset();
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
    task_environment_.FastForwardBy(base::Microseconds(17));
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
    test_api(autofill_manager()).Reset();
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    SCOPED_TRACE(
        "Test 4 - mixed case: some fields are autofilled, some fields are "
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
                                 mixed_filled_form.fields().front(),
                                 parse_time + base::Microseconds(3));

    task_environment_.FastForwardBy(base::Microseconds(17));
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
    test_api(autofill_manager()).Reset();
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
                                 mixed_filled_form.fields().front(),
                                 parse_time + base::Microseconds(3));

    task_environment_.FastForwardBy(base::Microseconds(17));
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
    test_api(autofill_manager()).Reset();
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

    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(second_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    test_api(autofill_manager()).Reset();
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

// Test class that shares setup code for testing ParseQueryResponse.
class AutofillMetricsParseQueryResponseTest : public testing::Test {
 public:
  void SetUp() override {
    FormData form;
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("http://foo.com"));
    form.set_main_frame_origin(
        url::Origin::Create(GURL("http://foo_root.com")));
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);

    field.set_label(u"fullname");
    field.set_name(u"fullname");
    test_api(form).Append(field);

    field.set_label(u"address");
    field.set_name(u"address");
    test_api(form).Append(field);

    // Checkable fields should be ignored in parsing.
    FormFieldData checkable_field;
    checkable_field.set_label(u"radio_button");
    checkable_field.set_form_control_type(FormControlType::kInputRadio);
    checkable_field.set_check_status(
        FormFieldData::CheckStatus::kCheckableButUnchecked);
    test_api(form).Append(checkable_field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());

    field.set_label(u"email");
    field.set_name(u"email");
    test_api(form).Append(field);

    field.set_label(u"password");
    field.set_name(u"password");
    field.set_form_control_type(FormControlType::kInputPassword);
    test_api(form).Append(field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::vector<std::unique_ptr<FormStructure>> owned_forms_;
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms_;
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
  ParseServerPredictionsQueryResponse(response_string, forms_,
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
  ParseServerPredictionsQueryResponse(response_string, forms_,
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
  ParseServerPredictionsQueryResponse(response_string, forms_,
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
  ParseServerPredictionsQueryResponse(response_string, forms_,
                                      test::GetEncodedSignatures(forms_),
                                      nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// Tests that credit card form submissions are logged specially when the form is
// on a non-secure page.
TEST_F(AutofillMetricsTest, NonSecureCreditCardForm) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  // Non-https origin.
  GURL frame_origin("http://example_root.com/form.html");
  form.set_main_frame_origin(url::Origin::Create(frame_origin));
  autofill_client_->set_form_origin(frame_origin);

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
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
       NonSecureCreditCardFormMetricsNotRecordedOnSecurePage) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_NUMBER,
                                        CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
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
      &test_ukm_recorder(), autofill_client_->GetUkmSourceId(), url,
      upload_decision);
  auto entries = test_ukm_recorder().GetEntriesByName(
      UkmCardUploadDecisionType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(entry, url);
    EXPECT_EQ(1u, entry->metrics.size());
    test_ukm_recorder().ExpectEntryMetric(
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
      &test_ukm_recorder(), autofill_client_->GetUkmSourceId(), url, true,
      {FormTypeNameForLogging::kCreditCardForm}, form_structure_metric,
      form_signature);
  auto entries = test_ukm_recorder().GetEntriesByName(
      UkmDeveloperEngagementType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(entry, url);
    EXPECT_EQ(4u, entry->metrics.size());
    test_ukm_recorder().ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
        form_structure_metric);
    test_ukm_recorder().ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kIsForCreditCardName, true);
    test_ukm_recorder().ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormTypesName,
        AutofillMetrics::FormTypesToBitVector(
            {FormTypeNameForLogging::kCreditCardForm}));
    test_ukm_recorder().ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormSignatureName,
        form_signature.value());
  }
}

// Tests that no UKM is logged when the URL is not valid.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_InvalidUrl) {
  GURL url("");
  test_ukm_recorder().Purge();
  autofill_metrics::LogCardUploadDecisionsUkm(&test_ukm_recorder(), -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
}

// Tests that no UKM is logged when the ukm service is null.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_NoUkmService) {
  GURL url("https://www.google.com");
  test_ukm_recorder().Purge();
  autofill_metrics::LogCardUploadDecisionsUkm(nullptr, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
}

// Test the ukm recorded when Suggestion is shown.
TEST_F(AutofillMetricsTest, AutofillSuggestionsShownTest) {
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

TEST_F(AutofillMetricsTest, DynamicFormMetrics) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STREET_ADDRESS};

  // Simulate seeing.
  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types);

  // Simulate filling the form.
  FillTestProfile(form);

  // Dynamically change the form.
  test_api(form).Remove(-1);

  // Trigger a refill, the refill metric should be updated.
  autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
              BucketsInclude(Bucket(FORM_EVENT_DID_DYNAMIC_REFILL, 1)));
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
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword,
                           "one-time-code"),
       CreateTestFormField("", "", "", FormControlType::kInputPassword)});

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
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword)});

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
      CreateForm({CreateTestFormField("Phone", "phone", "",
                                      FormControlType::kInputTelephone),
                  CreateTestFormField("Last Name", "lastname", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("First Name", "firstname", "",
                                      FormControlType::kInputText)});

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
  FormData form = CreateForm({CreateTestFormField(
      "Phone", "phone", "", FormControlType::kInputTelephone)});

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
  form.set_fields(
      {CreateTestFormField("", "", "", FormControlType::kInputText, "phone")});

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
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword)});

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

// Use <Phone><WebOTP><OTC> as the bit pattern to identify the metrics state.
enum class PhoneCollectionMetricState {
  kNone = 0,    // Site did not collect phone, not use OTC, not use WebOTP
  kOTC = 1,     // Site used OTC only
  kWebOTP = 2,  // Site used WebOTP only
  kWebOTPPlusOTC = 3,  // Site used WebOTP and OTC
  kPhone = 4,          // Site collected phone, not used neither WebOTP nor OTC
  kPhonePlusOTC = 5,   // Site collected phone number and used OTC
  kPhonePlusWebOTP = 6,         // Site collected phone number and used WebOTP
  kPhonePlusWebOTPPlusOTC = 7,  // Site collected phone number and used both
  kMaxValue = kPhonePlusWebOTPPlusOTC,
};

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
    for (const char* autocomplete : test_case.autocomplete_field) {
      test_api(form).Append(CreateTestFormField(
          "", "", "", FormControlType::kInputText, autocomplete));
    }

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
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::WebOTPImpact::kEntryName);
  ASSERT_TRUE(entries.empty());

  FormData form;
  CreateSimpleForm(autofill_client_->form_origin(), form);
  // Document collects phone number
  test_api(form).Append(
      CreateTestFormField("", "", "", FormControlType::kInputTelephone, "tel"));
  // Document uses OntTimeCode
  test_api(form).Append(CreateTestFormField(
      "", "", "", FormControlType::kInputText, "one-time-code"));

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_manager().ReportAutofillWebOTPMetrics(true);

  entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::WebOTPImpact::kEntryName);
  ASSERT_EQ(1u, entries.size());

  const int64_t* metric =
      test_ukm_recorder().GetEntryMetric(entries[0], "PhoneCollection");
  EXPECT_EQ(*metric, static_cast<int>(
                         PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC));
}

TEST_F(AutofillMetricsTest, AutocompleteOneTimeCodeFormFilledDuration) {
  FormData form = CreateForm({CreateTestFormField(
      "", "", "", FormControlType::kInputPassword, "one-time-code")});
  test_api(form).field(0).set_value(u"123456");

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(form);

    histogram_tester.ExpectTotalCount(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 16, 1);
    test_api(autofill_manager()).Reset();
  }

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    base::TimeTicks parse_time = autofill_manager()
                                     .form_structures()
                                     .begin()
                                     ->second->form_parsed_timestamp();
    FillAutofillFormData(form, parse_time + base::Microseconds(5));
    SimulateUserChangedTextField(form, form.fields().front(),
                                 parse_time + base::Microseconds(3));
    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", 14, 1);
    test_api(autofill_manager()).Reset();
  }
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(AutofillMetricsTest, OnAutocompleteSuggestionsShown) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::OnAutocompleteSuggestionsShown();
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events2", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      /*expected_count=*/1);
}

TEST_F(AutofillMetricsTest, LogServerCardLinkClicked) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState::kSignedIn);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       PaymentsSigninState::kSignedIn, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState::kSignedOut);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       PaymentsSigninState::kSignedOut, 1);
  }
}

TEST_F(AutofillMetricsTest, GetFieldTypeUserEditStatusMetric) {
  // The id of ADDRESS_HOME_COUNTRY is 36 = 0b10'0100.
  FieldType server_type = ADDRESS_HOME_COUNTRY;
  // The id of AUTOFILL_FIELD_WAS_NOT_EDITED is 1.
  AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric =
      AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
          AUTOFILLED_FIELD_WAS_NOT_EDITED;

  int expected_result = 0b10'0100'0001;
  int actual_result = GetFieldTypeUserEditStatusMetric(server_type, metric);
  EXPECT_EQ(expected_result, actual_result);
}

// Base class for cross-frame filling metrics, in particular for
// Autofill.CreditCard.SeamlessFills.*.
class AutofillMetricsCrossFrameFormTest : public AutofillMetricsTest {
 public:
  struct CreditCardAndCvc {
    CreditCard credit_card;
    std::u16string cvc;
  };

  AutofillMetricsCrossFrameFormTest() = default;
  ~AutofillMetricsCrossFrameFormTest() override = default;

  void SetUp() override {
    AutofillMetricsTest::SetUp();

    RecreateCreditCards(/*include_local_credit_card=*/true,
                        /*include_masked_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);

    credit_card_with_cvc_ = {
        .credit_card = *autofill_client_->GetPersonalDataManager()
                            ->payments_data_manager()
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
         .renderer_id = test::MakeFormRendererId(),
         .main_frame_origin = main_origin});

    ASSERT_EQ(form_.main_frame_origin(), form_.fields()[0].origin());
    ASSERT_EQ(form_.main_frame_origin(), form_.fields()[2].origin());
    ASSERT_NE(form_.main_frame_origin(), form_.fields()[1].origin());
    ASSERT_NE(form_.main_frame_origin(), form_.fields()[3].origin());
    ASSERT_EQ(form_.fields()[1].origin(), form_.fields()[3].origin());

    // Mock a simplified security model which allows to filter (only) fields
    // from the same origin.
    autofill_driver_->SetFieldTypeMapFilter(base::BindRepeating(
        [](AutofillMetricsCrossFrameFormTest* self,
           const url::Origin& triggered_origin, FieldGlobalId field,
           FieldType) {
          return triggered_origin == self->GetFieldById(field).origin();
        },
        this));
  }

  CreditCardAndCvc& fill_data() { return credit_card_with_cvc_; }

  // Any call to FillForm() should be followed by a SetFormValues() call to
  // mimic its effect on |form_|.
  void FillForm(const FormFieldData& triggering_field) {
    autofill_manager().FillOrPreviewCreditCardForm(
        mojom::ActionPersistence::kFill, form_, triggering_field,
        fill_data().credit_card, fill_data().cvc,
        {.trigger_source = AutofillTriggerSource::kPopup});
  }

  // Sets the field values of |form_| according to the parameters.
  //
  // Since this test suite doesn't use mocks, we can't intercept the autofilled
  // form. Therefore, after each manual fill or autofill, we shall call
  // SetFormValues()
  void SetFormValues(const FieldTypeSet& fill_field_types,
                     bool is_autofilled,
                     bool is_user_typed) {
    auto type_to_index = base::MakeFixedFlatMap<FieldType, size_t>(
        {{CREDIT_CARD_NAME_FULL, 0},
         {CREDIT_CARD_NUMBER, 1},
         {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, 2},
         {CREDIT_CARD_VERIFICATION_CODE, 3}});

    for (FieldType fill_type : fill_field_types) {
      auto index_it = type_to_index.find(fill_type);
      ASSERT_NE(index_it, type_to_index.end());
      FormFieldData& field = test_api(form_).field(index_it->second);
      field.set_value(fill_type != CREDIT_CARD_VERIFICATION_CODE
                          ? fill_data().credit_card.GetRawInfo(fill_type)
                          : fill_data().cvc);
      field.set_is_autofilled(is_autofilled);
      field.set_properties_mask((field.properties_mask() & ~kUserTyped) |
                                (is_user_typed ? kUserTyped : 0));
    }
  }

  FormFieldData& GetFieldById(FieldGlobalId field) {
    auto it = base::ranges::find(test_api(form_).fields(), field,
                                 &FormFieldData::global_id);
    CHECK(it != form_.fields().end());
    return *it;
  }

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

 protected:
  AutofillMetricsSeamlessnessTest() {
    scoped_features_.InitAndEnableFeatureWithParameters(
        features::kAutofillLogUKMEventsWithSamplingOnSession,
        {{features::kAutofillLogUKMEventsWithSamplingOnSessionRate.name,
          "100"}});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
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

  VerifyUkm(&test_ukm_recorder(), form_, UkmBuilder::kEntryName, {});
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
  FillForm(form_.fields()[0]);
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
  FillForm(form_.fields()[1]);
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
      &test_ukm_recorder(), form_, UkmBuilder::kEntryName,
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

// Test if we have correctly recorded the filling status of fields in an unsafe
// iframe.
TEST_F(AutofillMetricsSeamlessnessTest, CreditCardFormRecordOnIFrames) {
  // Create a form with the credit card number and CVC code fields in an
  // iframe with a different origin.
  SeeForm(form_);

  // Triggering autofill from the credit card name field cannot fill the credit
  // card number and CVC code fields, which are in an unsafe iframe.
  FillForm(form_.fields()[0]);
  SetFormValues({CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Triggering autofill from the credit card number field can fill all the
  // credit card fields with values.
  FillForm(form_.fields()[1]);
  SetFormValues({CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                 CREDIT_CARD_VERIFICATION_CODE},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  SubmitForm(form_);
  ResetDriverToCommitMetrics();

  std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE};

  // Verify FieldInfo UKM event for every field.
  auto field_entries =
      test_ukm_recorder().GetEntriesByName(UkmFieldInfoType::kEntryName);
  ASSERT_EQ(4u, field_entries.size());
  for (size_t i = 0; i < field_entries.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    using UFIT = UkmFieldInfoType;
    const auto* const entry = field_entries[i].get();
    DenseSet<FieldFillingSkipReason> skipped_status_vector;
    if (i == 0 || i == 2) {
      skipped_status_vector = {FieldFillingSkipReason::kNotSkipped,
                               FieldFillingSkipReason::kAlreadyAutofilled};
    } else {
      skipped_status_vector = {FieldFillingSkipReason::kNotSkipped};
    }
    DenseSet<AutofillStatus> autofill_status_vector;
    int field_log_events_count = 0;
    if (i == 0 || i == 2) {
      autofill_status_vector = {
          AutofillStatus::kIsFocusable,
          AutofillStatus::kWasAutofillTriggered,
          AutofillStatus::kWasAutofilledBeforeSecurityPolicy,
          AutofillStatus::kWasRefill,
          AutofillStatus::kHadValueBeforeFilling,
          AutofillStatus::kHadTypedOrFilledValueAtSubmission,
          AutofillStatus::kWasAutofilledAfterSecurityPolicy};
      field_log_events_count = i == 0 ? 3 : 2;
    } else {
      autofill_status_vector = {
          AutofillStatus::kIsFocusable,
          AutofillStatus::kWasAutofillTriggered,
          AutofillStatus::kWasAutofilledBeforeSecurityPolicy,
          AutofillStatus::kWasRefill,
          AutofillStatus::kHadTypedOrFilledValueAtSubmission,
          AutofillStatus::kFillingPreventedByIframeSecurityPolicy,
          AutofillStatus::kWasAutofilledAfterSecurityPolicy};
      field_log_events_count = i == 1 ? 3 : 2;
    }
    std::map<std::string, int64_t> expected = {
        {UFIT::kFormSessionIdentifierName,
         AutofillMetrics::FormGlobalIdToHash64Bit(form_.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         AutofillMetrics::FieldGlobalIdToHash64Bit(
             form_.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form_.fields()[i])).value()},
        {UFIT::kAutofillSkippedStatusName, skipped_status_vector.data()[0]},
        {UFIT::kFormControlType2Name,
         base::to_underlying(FormControlType::kInputText)},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
        {UFIT::kOverallTypeName, field_types[i]},
        {UFIT::kSectionIdName, 1},
        {UFIT::kTypeChangedByRationalizationName, false},
        {UFIT::kRankInFieldSignatureGroupName, 1},
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
        {UFIT::kFieldLogEventCountName, field_log_events_count + 2},
    };
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (const auto& [metric, value] : expected) {
      test_ukm_recorder().ExpectEntryMetric(entry, metric, value);
    }
  }
}

// Test the field log events at the form submission.
class AutofillMetricsFromLogEventsTest : public AutofillMetricsTest {
 protected:
  AutofillMetricsFromLogEventsTest() {
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
TEST_F(AutofillMetricsFromLogEventsTest, TestShowSuggestionAutofillStatus) {
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
TEST_F(AutofillMetricsFromLogEventsTest, AddressSubmittedFormLogEvents) {
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
            AutofillStatus::kWasAutofillTriggered,
            AutofillStatus::kWasAutofilledBeforeSecurityPolicy,
            AutofillStatus::kSuggestionWasAvailable,
            AutofillStatus::kSuggestionWasShown,
            AutofillStatus::kSuggestionWasAccepted,
            AutofillStatus::kUserTypedIntoField,
            AutofillStatus::kFilledValueWasModified,
            AutofillStatus::kHadTypedOrFilledValueAtSubmission,
            AutofillStatus::kWasAutofilledAfterSecurityPolicy,
            AutofillStatus::kWasFocused};
        field_log_events_count = 4;
      } else if (i == 1) {
        autofill_status_vector = {
            AutofillStatus::kIsFocusable, AutofillStatus::kWasAutofillTriggered,
            AutofillStatus::kWasAutofilledBeforeSecurityPolicy,
            AutofillStatus::kHadTypedOrFilledValueAtSubmission,
            AutofillStatus::kWasAutofilledAfterSecurityPolicy};
        field_log_events_count = 1;
      } else if (i == 2) {
        autofill_status_vector = {AutofillStatus::kIsFocusable,
                                  AutofillStatus::kWasAutofillTriggered};
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
    AutofillMetrics::FormEventSet form_events = {
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
TEST_F(AutofillMetricsFromLogEventsTest, AutofillFieldInfoMetricsFieldType) {
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
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
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
  AutofillMetrics::FormEventSet form_events = {};
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
TEST_F(AutofillMetricsFromLogEventsTest,
       AutofillFieldInfoMetricsEditedFieldWithoutFill) {
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
  AutofillMetrics::FormEventSet form_events = {FORM_EVENT_DID_PARSE_FORM};
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
TEST_F(AutofillMetricsFromLogEventsTest,
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
TEST_F(AutofillMetricsFromLogEventsTest,
       AutofillFieldInfoMetricsNotRecordOnSearchBox) {
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
TEST_F(AutofillMetricsFromLogEventsTest,
       AutofillFieldInfoMetricsNotRecordOnAllCheckBox) {
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
    AutofillMetricsFromLogEventsTest,
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
TEST_F(AutofillMetricsFromLogEventsTest,
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
  AutofillMetrics::FormEventSet form_events = {FORM_EVENT_DID_PARSE_FORM};
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
TEST_F(AutofillMetricsFromLogEventsTest,
       AutofillFieldInfoMetricsRecordOnDifferentFrames) {
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
  AutofillMetrics::FormEventSet form_events = {FORM_EVENT_DID_PARSE_FORM};
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

namespace {
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
}  // namespace

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsFromLogEventsTest,
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
  autofill::TestAutofillClock test_clock(arbitrary_default_time);

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
          form, first_field,
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

TEST_F(AutofillMetricsFromLogEventsTest,
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

  AutofillMetrics::FormInteractionsUkmLogger logger(autofill_client_.get(),
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
