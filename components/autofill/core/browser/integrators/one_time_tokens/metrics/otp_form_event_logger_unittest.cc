// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/signatures.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
namespace {

using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using test::CreateTestFormField;
using test::VerifySingleSubmissionKeyMetricExpectations;
using ::testing::_;
using ::testing::Each;
using ::testing::Return;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

class OtpFormEventLoggerIntegrationTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 protected:
  OtpFormEventLoggerIntegrationTest() = default;

  void SetUp() override {
    SetUpHelper();
    ResetCrowdsourcingManager();
  }
  void TearDown() override { TearDownHelper(); }

  void InitAutofillClient() override {
    autofill_metrics::AutofillMetricsBaseTest::InitAutofillClient();
    autofill_client().set_sms_otp_service(
        std::make_unique<one_time_tokens::FakeSmsOtpService>());
  }

  one_time_tokens::FakeSmsOtpService& sms_otp_service() {
    return static_cast<one_time_tokens::FakeSmsOtpService&>(
        *autofill_client().GetSmsOtpService());
  }

  void ResetCrowdsourcingManager() {
    auto mock_crowdsourcing_manager =
        std::make_unique<testing::NiceMock<MockAutofillCrowdsourcingManager>>(
            &autofill_client());
    // Default action: always run the callback with a default/empty response
    ON_CALL(*mock_crowdsourcing_manager, StartQueryRequest)
        .WillByDefault(
            [](const std::vector<FormData>&, std::optional<net::IsolationInfo>,
               base::OnceCallback<void(
                   std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                   callback) {
              // Run with a minimal response to ensure the flow completes.
              std::move(callback).Run(
                  AutofillCrowdsourcingManager::QueryResponse("", {}));
              return true;
            });
    autofill_client().set_crowdsourcing_manager(
        std::move(mock_crowdsourcing_manager));
  }

  void SetupMockedServerPredictionResponse(const std::string& response,
                                           FormSignature form_signature) {
    // Create and inject the mock crowdsourcing manager.
    auto mock_crowdsourcing_manager =
        std::make_unique<testing::NiceMock<MockAutofillCrowdsourcingManager>>(
            &autofill_client());
    auto mock_crowdsourcing_manager_ptr = mock_crowdsourcing_manager.get();
    autofill_client().set_crowdsourcing_manager(
        std::move(mock_crowdsourcing_manager));

    EXPECT_CALL(*mock_crowdsourcing_manager_ptr, StartQueryRequest)
        .Times(testing::AtLeast(0))
        .WillRepeatedly(
            [response, form_signature](
                const std::vector<FormData>&, std::optional<net::IsolationInfo>,
                base::OnceCallback<void(
                    std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                    callback) {
              // ALWAYS run the callback to ensure
              // OnAfterLoadedServerPredictions is called.
              std::move(callback).Run(
                  AutofillCrowdsourcingManager::QueryResponse(
                      response, {form_signature}));
              return true;
            });
  }

  FormData CreateOtpForm() {
    return test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
  }

  // Creates an OTP form and lets Autofill see and parse it.
  FormData SeeParsedOtpForm() {
    FormData otp_form = CreateOtpForm();
    SetupMockedServerPredictionResponse(
        CreateMockedServerResponseString(otp_form),
        CalculateFormSignature(otp_form));
    SeeForm(otp_form);
    test_api(autofill_manager()).OnFormsParsed({otp_form});
    return otp_form;
  }

  static std::string CreateMockedServerResponseString(const FormData form) {
    AutofillQueryResponse response;
    auto* form_suggestion = response.add_form_suggestions();
    for (const auto& field : form.fields()) {
      auto* field_suggestion = form_suggestion->add_field_suggestions();
      field_suggestion->set_field_signature(
          CalculateFieldSignatureForField(field).value());
      *field_suggestion->add_predictions() =
          test::CreateFieldPrediction(ONE_TIME_CODE, false);
    }
    return base::Base64Encode(response.SerializeAsString());
  }

  // Simulates the arrival of an SMS OTP if `returns_otp` is true, and a failed
  // SMS OTP retrieval otherwise.
  void SimulateOtpResponse(bool returns_otp) {
    if (returns_otp) {
      // `OtpManager` reports the availability of SMS OTPs only while
      // suggestions are requested. Report it directly to simulate that the OTP
      // was available before the user interacted with the form.
      autofill_manager().GetOtpFormEventLogger().OnOtpAvailable();
    }
    sms_otp_service().SimulateOtpArrived(CreateOtpResult(returns_otp));
  }

  base::expected<one_time_tokens::OneTimeToken,
                 one_time_tokens::OneTimeTokenRetrievalError>
  CreateOtpResult(bool returns_otp) {
    if (returns_otp) {
      return one_time_tokens::OneTimeToken(
          one_time_tokens::OneTimeTokenType::kSmsOtp, "123456",
          base::TimeTicks::Now());
    }
    return base::unexpected(one_time_tokens::OneTimeTokenRetrievalError());
  }

  void VerifyInteractedWithFormUkmMetric(
      const FormData& form,
      size_t expected_local_record_type_count) {
    using Ukm = UkmInteractedWithFormType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kIsForCreditCardName, false},
              {Ukm::kLocalRecordTypeCountName,
               expected_local_record_type_count},
              {Ukm::kServerRecordTypeCountName, 0},
              {Ukm::kFormSignatureName,
               autofill_metrics::Collapse(CalculateFormSignature(form))
                   .value()}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        testing::ElementsAre(form.main_frame_origin().GetURL()));
  }

  void VerifyUkmSuggestionFilledLogged(const FormData& form) {
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(),
                                       UkmSuggestionFilledType::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{UkmSuggestionFilledType::kIsForCreditCardName, false},
              {UkmSuggestionFilledType::kFormSignatureName,
               autofill_metrics::Collapse(CalculateFormSignature(form))
                   .value()},
              {UkmSuggestionFilledType::kFieldSignatureName,
               autofill_metrics::Collapse(
                   CalculateFieldSignatureForField(form.fields().front()))
                   .value()},
              {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName,
               0}}}));
    EXPECT_THAT(autofill_metrics::GetEventUrls(
                    test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
};

TEST_F(OtpFormEventLoggerIntegrationTest, Readiness) {
  autofill_metrics::OtpFormEventLogger logger(&autofill_manager());

  EXPECT_FALSE(logger.HasLoggedDataToFillAvailableForTesting());
  logger.OnOtpAvailable();
  EXPECT_TRUE(logger.HasLoggedDataToFillAvailableForTesting());

  logger.OnDestroyed();
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpReady) {
  base::HistogramTester histogram_tester;
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});
  ASSERT_EQ(sms_otp_service().num_subscribers(), 1u);
  SimulateOtpResponse(/*returns_otp=*/true);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate form submission.
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Verify that the readiness metric was logged.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = true, .assistance = false});
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  VerifyInteractedWithFormUkmMetric(otp_form,
                                    /*expected_local_record_type_count=*/1);
  EXPECT_THAT(autofill_metrics::GetUkmEvents(
                  test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
              autofill_metrics::UkmEventsAre({}));
  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kFillingReadinessName, 1},
              {Ukm::kFillingAssistanceName, 0},
              {Ukm::kAutofillFillsName, 0},
              {Ukm::kFormElementUserModificationsName, 0},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpNotReady) {
  base::HistogramTester histogram_tester;
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});
  ASSERT_EQ(sms_otp_service().num_subscribers(), 1u);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that no OTP could be retrieved after the user interacted with the
  // form.
  SimulateOtpResponse(/*returns_otp=*/false);

  // Simulate form submission.
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Verify that the readiness metric was logged.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = false, .assistance = false});
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  VerifyInteractedWithFormUkmMetric(otp_form,
                                    /*expected_local_record_type_count=*/0);
  EXPECT_THAT(autofill_metrics::GetUkmEvents(
                  test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
              autofill_metrics::UkmEventsAre({}));
  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kFillingReadinessName, 0},
              {Ukm::kFillingAssistanceName, 0},
              {Ukm::kAutofillFillsName, 0},
              {Ukm::kFormElementUserModificationsName, 0},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpAccepted) {
  base::HistogramTester histogram_tester;
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});
  ASSERT_EQ(sms_otp_service().num_subscribers(), 1u);
  SimulateOtpResponse(/*returns_otp=*/true);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate user accepting the suggestion
  OtpFillData fill_data = {{otp_form.fields().front().global_id(), u"123456"}};
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, otp_form.global_id(),
      otp_form.fields().front().global_id(), FillingProduct::kOneTimePassword,
      &fill_data, AutofillTriggerSource::kPopup,
      /*require_user_confirmation=*/false, /*blocked_fields=*/{});

  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  VerifySingleSubmissionKeyMetricExpectations(histogram_tester,
                                              "OneTimePassword",
                                              {.readiness = true,
                                               .acceptance = true,
                                               .assistance = true,
                                               .correctness = true});
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  VerifyInteractedWithFormUkmMetric(otp_form,
                                    /*expected_local_record_type_count=*/1);
  VerifyUkmSuggestionFilledLogged(otp_form);
  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kFillingReadinessName, 1},
              {Ukm::kFillingAcceptanceName, 1},
              {Ukm::kFillingCorrectnessName, 1},
              {Ukm::kFillingAssistanceName, 1},
              {Ukm::kAutofillFillsName, 1},
              {Ukm::kFormElementUserModificationsName, 0},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }

  // Verify that the FORM_EVENT_LOCAL_SUGGESTION_FILLED and
  // FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE events are logged by the logger
  // other events are logged by the base logger.
#if BUILDFLAG(IS_ANDROID)
  {
    using Ukm = UkmFormEventType;
    auto event_metrics = [](autofill_metrics::FormEvent e) {
      return std::vector<autofill_metrics::UkmMetricNameAndValue>{
          {Ukm::kAutofillFormEventName, e},
          {Ukm::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kOneTimePasswordForm})},
          {Ukm::kMillisecondsSinceFormParsedName, 0}};
    };
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_INTERACTED_ONCE),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE)}));
  }
#endif
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpNotAccepted) {
  base::HistogramTester histogram_tester;
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});
  ASSERT_EQ(sms_otp_service().num_subscribers(), 1u);
  SimulateOtpResponse(/*returns_otp=*/true);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate the user NOT accepting the suggestion.
  // We don't call FillOrPreviewForm.

  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Readiness should be true because an OTP was available.
  // Acceptance should be false because the suggestion wasn't filled.
  // Assistance should be false for the same reason.
  // Correctness should not be logged because no suggestion was accepted.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = true, .acceptance = false, .assistance = false});
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  VerifyInteractedWithFormUkmMetric(otp_form,
                                    /*expected_local_record_type_count=*/1);
  EXPECT_THAT(autofill_metrics::GetUkmEvents(
                  test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
              autofill_metrics::UkmEventsAre({}));
  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kFillingReadinessName, 1},
              {Ukm::kFillingAcceptanceName, 0},
              {Ukm::kFillingAssistanceName, 0},
              {Ukm::kAutofillFillsName, 0},
              {Ukm::kFormElementUserModificationsName, 0},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpAcceptedAndCorrected) {
  base::HistogramTester histogram_tester;
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});
  ASSERT_EQ(sms_otp_service().num_subscribers(), 1u);
  SimulateOtpResponse(/*returns_otp=*/true);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate user accepting the suggestion
  OtpFillData fill_data = {{otp_form.fields().front().global_id(), u"123456"}};
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, otp_form.global_id(),
      otp_form.fields().front().global_id(), FillingProduct::kOneTimePassword,
      &fill_data, AutofillTriggerSource::kPopup,
      /*require_user_confirmation=*/false, /*blocked_fields=*/{});
  // Simulate the user correcting the value.
  SimulateUserChangedFieldTo(otp_form, otp_form.fields().front().global_id(),
                             u"654321");

  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  VerifySingleSubmissionKeyMetricExpectations(histogram_tester,
                                              "OneTimePassword",
                                              {.readiness = true,
                                               .acceptance = true,
                                               .assistance = true,
                                               .correctness = false});
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.OneTimePassword",
      autofill_metrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  VerifyInteractedWithFormUkmMetric(otp_form,
                                    /*expected_local_record_type_count=*/1);
  VerifyUkmSuggestionFilledLogged(otp_form);
  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {{{Ukm::kFillingReadinessName, 1},
              {Ukm::kFillingAcceptanceName, 1},
              {Ukm::kFillingCorrectnessName, 0},
              {Ukm::kFillingAssistanceName, 1},
              {Ukm::kAutofillFillsName, 1},
              {Ukm::kFormElementUserModificationsName, 1},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }

  // Verify that the FORM_EVENT_LOCAL_SUGGESTION_FILLED and
  // FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE events are logged by the logger
  // other events are logged by the base logger.
#if BUILDFLAG(IS_ANDROID)
  {
    using Ukm = UkmFormEventType;
    auto event_metrics = [](autofill_metrics::FormEvent e) {
      return std::vector<autofill_metrics::UkmMetricNameAndValue>{
          {Ukm::kAutofillFormEventName, e},
          {Ukm::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kOneTimePasswordForm})},
          {Ukm::kMillisecondsSinceFormParsedName, 0}};
    };
    EXPECT_THAT(
        autofill_metrics::GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        autofill_metrics::UkmEventsAre(
            {event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_INTERACTED_ONCE),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE)}));
  }
#endif
}

// Sets the state of Gmail OTP suggestions. Inherited before
// OtpFormEventLoggerIntegrationTest so that the feature list outlives the
// TaskEnvironment's threads, which read features when they start on Android.
class WithGmailOtpSuggestionsFeature {
 protected:
  explicit WithGmailOtpSuggestionsFeature(bool enabled) {
    feature_list_.InitWithFeatureState(
        features::kAutofillShowGmailOtpSuggestions, enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Fixture in which the user is eligible for Gmail OTPs: Gmail OTP suggestions
// are enabled, a Gmail OTP backend exists, the user opted into Gmail OTPs, and
// the user is signed in with a Gmail account.
class OtpFormEventLoggerGmailTest : public WithGmailOtpSuggestionsFeature,
                                    public OtpFormEventLoggerIntegrationTest {
 protected:
  // `primary_account_email` is the account the user is signed in with, or
  // nullopt if signed out.
  explicit OtpFormEventLoggerGmailTest(
      bool gmail_otp_suggestions_enabled = true,
      std::optional<std::string> primary_account_email = "user@gmail.com")
      : WithGmailOtpSuggestionsFeature(gmail_otp_suggestions_enabled),
        primary_account_email_(std::move(primary_account_email)) {}

  void InitAutofillClient() override {
    OtpFormEventLoggerIntegrationTest::InitAutofillClient();
    autofill_client().set_gmail_otp_backend(
        std::make_unique<one_time_tokens::FakeGmailOtpBackend>());
    prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                             true);
    if (primary_account_email_) {
      autofill_client().identity_test_environment().MakePrimaryAccountAvailable(
          *primary_account_email_, signin::ConsentLevel::kSignin);
    }
  }

  // Simulates an OTP that is available, shown, filled, and submitted.
  void FillAndSubmitOtp(const FormData& otp_form) {
    autofill_manager().GetOtpFormEventLogger().OnOtpAvailable();
    autofill_manager().OnAskForValuesToFillTest(
        otp_form, otp_form.fields().front().global_id());
    DidShowAutofillSuggestions(otp_form, /*field_index=*/0,
                               SuggestionType::kGmailOneTimePasswordEntry);
    OtpFillData fill_data = {
        {otp_form.fields().front().global_id(), u"123456"}};
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, otp_form.global_id(),
        otp_form.fields().front().global_id(), FillingProduct::kOneTimePassword,
        &fill_data, AutofillTriggerSource::kPopup,
        /*require_user_confirmation=*/false, /*blocked_fields=*/{});
    SubmitForm(otp_form);
    DeleteDriverToCommitMetrics();
  }

 private:
  const std::optional<std::string> primary_account_email_;
};

// Tests that the key metrics are also recorded as
// `OneTimePassword.GmailEligibleClients` for users who are eligible for Gmail
// OTPs.
TEST_F(OtpFormEventLoggerGmailTest, RecordsGmailKeyMetrics) {
  base::HistogramTester histogram_tester;
  FormData otp_form = SeeParsedOtpForm();
  FillAndSubmitOtp(otp_form);

  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword.GmailEligibleClients",
      {.readiness = true,
       .acceptance = true,
       .assistance = true,
       .correctness = true});
}

// Tests that the funnel and form submission metrics are also recorded as
// `OneTimePassword.GmailEligibleClients` for users who are eligible for Gmail
// OTPs.
TEST_F(OtpFormEventLoggerGmailTest, RecordsGmailFunnelMetrics) {
  base::HistogramTester histogram_tester;
  FormData otp_form = SeeParsedOtpForm();
  FillAndSubmitOtp(otp_form);

  for (std::string_view stage :
       {"InteractionAfterParsedAsType", "SuggestionAfterInteraction",
        "FillAfterSuggestion", "SubmissionAfterFill"}) {
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Autofill.Funnel.", stage,
                      ".OneTimePassword.GmailEligibleClients"}),
        true, 1);
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.OneTimePassword."
      "GmailEligibleClients",
      true, 1);
}

// Tests that typing the OTP instead of filling it is recorded as not filled and
// not autofilled for users who are eligible for Gmail OTPs.
TEST_F(OtpFormEventLoggerGmailTest, TypedOtp_RecordsGmailNotAutofilled) {
  base::HistogramTester histogram_tester;
  FormData otp_form = SeeParsedOtpForm();
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0,
                             SuggestionType::kGmailOneTimePasswordEntry);
  SimulateUserChangedField(otp_form, otp_form.fields().front());
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.FillAfterSuggestion.OneTimePassword."
      "GmailEligibleClients",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.OneTimePassword."
      "GmailEligibleClients",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.OneTimePassword."
      "GmailEligibleClients",
      0);
}

// Reasons why a user is not eligible for Gmail OTPs.
enum class GmailOtpIneligibility {
  kNotOptedIn,
  // E.g. if the user opted in on another device.
  kGmailOtpSuggestionsDisabled,
  // E.g. in Incognito, which has no Gmail OTP backend.
  kNoGmailOtpBackend,
  kSignedOut,
  kNonGmailAccount,
};

// Returns the account the user is signed in with for `ineligibility`, or
// nullopt if signed out. Avoids signing out in the test body, which
// `signin::ClearPrimaryAccount()` doesn't support on ChromeOS.
std::optional<std::string> GetPrimaryAccountEmail(
    GmailOtpIneligibility ineligibility) {
  switch (ineligibility) {
    case GmailOtpIneligibility::kSignedOut:
      return std::nullopt;
    case GmailOtpIneligibility::kNonGmailAccount:
      return "user@example.com";
    case GmailOtpIneligibility::kNotOptedIn:
    case GmailOtpIneligibility::kGmailOtpSuggestionsDisabled:
    case GmailOtpIneligibility::kNoGmailOtpBackend:
      return "user@gmail.com";
  }
  NOTREACHED();
}

class OtpFormEventLoggerGmailIneligibleTest
    : public OtpFormEventLoggerGmailTest,
      public WithParamInterface<GmailOtpIneligibility> {
 protected:
  OtpFormEventLoggerGmailIneligibleTest()
      : OtpFormEventLoggerGmailTest(
            /*gmail_otp_suggestions_enabled=*/GetParam() !=
                GmailOtpIneligibility::kGmailOtpSuggestionsDisabled,
            GetPrimaryAccountEmail(GetParam())) {}
};

// Tests that no Gmail metrics are recorded for users who are not eligible for
// Gmail OTPs.
TEST_P(OtpFormEventLoggerGmailIneligibleTest, NoGmailMetrics) {
  switch (GetParam()) {
    case GmailOtpIneligibility::kNotOptedIn:
      prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                               false);
      break;
    case GmailOtpIneligibility::kNoGmailOtpBackend:
      autofill_client().set_gmail_otp_backend(nullptr);
      break;
    case GmailOtpIneligibility::kGmailOtpSuggestionsDisabled:
    case GmailOtpIneligibility::kSignedOut:
    case GmailOtpIneligibility::kNonGmailAccount:
      // Set up by the fixture.
      break;
  }
  base::HistogramTester histogram_tester;
  FormData otp_form = SeeParsedOtpForm();
  FillAndSubmitOtp(otp_form);

  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FillingReadiness.OneTimePassword", true, 1);
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword.GmailEligibleClients", {});
  histogram_tester.ExpectTotalCount(
      "Autofill.Funnel.InteractionAfterParsedAsType.OneTimePassword."
      "GmailEligibleClients",
      0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OtpFormEventLoggerGmailIneligibleTest,
    Values(GmailOtpIneligibility::kNotOptedIn,
           GmailOtpIneligibility::kGmailOtpSuggestionsDisabled,
           GmailOtpIneligibility::kNoGmailOtpBackend,
           GmailOtpIneligibility::kSignedOut,
           GmailOtpIneligibility::kNonGmailAccount),
    [](const TestParamInfo<GmailOtpIneligibility>& info) {
      switch (info.param) {
        case GmailOtpIneligibility::kNotOptedIn:
          return "NotOptedIn";
        case GmailOtpIneligibility::kGmailOtpSuggestionsDisabled:
          return "GmailOtpSuggestionsDisabled";
        case GmailOtpIneligibility::kNoGmailOtpBackend:
          return "NoGmailOtpBackend";
        case GmailOtpIneligibility::kSignedOut:
          return "SignedOut";
        case GmailOtpIneligibility::kNonGmailAccount:
          return "NonGmailAccount";
      }
      NOTREACHED();
    });

}  // namespace
}  // namespace autofill
