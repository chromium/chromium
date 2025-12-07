// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/signatures.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/password_manager/core/browser/features/password_features.h"
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

class MockSmsOtpBackend : public one_time_tokens::SmsOtpBackend {
 public:
  MockSmsOtpBackend() = default;
  ~MockSmsOtpBackend() override = default;

  MOCK_METHOD(void,
              RetrieveSmsOtp,
              (base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>),
              (override));
};

class OtpFormEventLoggerIntegrationTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 protected:
  OtpFormEventLoggerIntegrationTest() = default;
  base::test::ScopedFeatureList feature_list_;

  void SetUp() override {
    SetUpHelper();
    ResetCrowdsourcingManager();
  }
  void TearDown() override { TearDownHelper(); }

  void InitAutofillClient() override {
    autofill_metrics::AutofillMetricsBaseTest::InitAutofillClient();
    // Inject the mocked SMS OTP backend into the TestAutofillClient.
    auto mock_sms_otp_backend = std::make_unique<MockSmsOtpBackend>();
    autofill_client().set_sms_otp_backend(std::move(mock_sms_otp_backend));
    autofill_client().set_one_time_token_service(
        std::make_unique<one_time_tokens::OneTimeTokenServiceImpl>(
            autofill_client().GetSmsOtpBackend()));
  }

  void ResetCrowdsourcingManager() {
    auto mock_crowdsourcing_manager =
        std::make_unique<testing::NiceMock<MockAutofillCrowdsourcingManager>>(
            &autofill_client());
    // Default action: always run the callback with a default/empty response
    ON_CALL(*mock_crowdsourcing_manager, StartQueryRequest)
        .WillByDefault(
            [](const std::vector<
                   raw_ptr<const FormStructure, VectorExperimental>>&,
               std::optional<net::IsolationInfo>,
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
                const std::vector<
                    raw_ptr<const FormStructure, VectorExperimental>>&,
                std::optional<net::IsolationInfo>,
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

  void SetupMockedOtpResponse(bool returns_otp) {
    MockSmsOtpBackend* backend =
        static_cast<MockSmsOtpBackend*>(autofill_client().GetSmsOtpBackend());
    one_time_tokens::OtpFetchReply reply = CreateOtpFetchReply(returns_otp);
    EXPECT_CALL(*backend, RetrieveSmsOtp)
        .WillRepeatedly([this, returns_otp, reply](auto callback) {
          if (returns_otp) {
            autofill_manager().GetOtpFormEventLogger().OnOtpAvailable();
          }
          std::move(callback).Run(reply);
        });
  }

  one_time_tokens::OtpFetchReply CreateOtpFetchReply(bool returns_otp) {
    std::optional<one_time_tokens::OneTimeToken> token;
    if (returns_otp) {
      token = one_time_tokens::OneTimeToken(
          one_time_tokens::OneTimeTokenType::kSmsOtp, "123456",
          base::Time::Now());
    }

    return one_time_tokens::OtpFetchReply(token, /*request_complete=*/true);
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
  SetupMockedOtpResponse(true);
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).otp_form_interactions_flow_id();
  // Simulate the WillSubmit event.
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Verify that the readiness metric was logged.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = true, .assistance = false});
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
              {Ukm::kFlowIdName, flow_id.value()},
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
  SetupMockedOtpResponse(false);
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).otp_form_interactions_flow_id();
  // Simulate the WillSubmit event.
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Verify that the readiness metric was logged.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = false, .assistance = false});
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
              {Ukm::kFlowIdName, flow_id.value()},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpAccepted) {
#if BUILDFLAG(IS_ANDROID)
  feature_list_.InitAndEnableFeature(
      password_manager::features::kAndroidSmsOtpFilling);
#endif
  base::HistogramTester histogram_tester;
  SetupMockedOtpResponse(true);
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate user accepting the suggestion
  OtpFillData fill_data = {{otp_form.fields().front().global_id(), u"123456"}};
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, otp_form,
      otp_form.fields().front().global_id(), &fill_data,
      AutofillTriggerSource::kPopup);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).otp_form_interactions_flow_id();
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  VerifySingleSubmissionKeyMetricExpectations(histogram_tester,
                                              "OneTimePassword",
                                              {.readiness = true,
                                               .acceptance = true,
                                               .assistance = true,
                                               .correctness = true});
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
              {Ukm::kFlowIdName, flow_id.value()},
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
                               FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE)}));
  }
#endif
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpNotAccepted) {
  base::HistogramTester histogram_tester;
  SetupMockedOtpResponse(true);
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate the user NOT accepting the suggestion.
  // We don't call FillOrPreviewForm.

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).otp_form_interactions_flow_id();
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  // Readiness should be true because an OTP was available.
  // Acceptance should be false because the suggestion wasn't filled.
  // Assistance should be false for the same reason.
  // Correctness should not be logged because no suggestion was accepted.
  VerifySingleSubmissionKeyMetricExpectations(
      histogram_tester, "OneTimePassword",
      {.readiness = true, .acceptance = false, .assistance = false});
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
              {Ukm::kFlowIdName, flow_id.value()},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kOneTimePasswordForm})}}}));
    EXPECT_THAT(
        autofill_metrics::GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
        Each(otp_form.main_frame_origin().GetURL()));
  }
}

TEST_F(OtpFormEventLoggerIntegrationTest, OtpAcceptedAndCorrected) {
#if BUILDFLAG(IS_ANDROID)
  feature_list_.InitAndEnableFeature(
      password_manager::features::kAndroidSmsOtpFilling);
#endif
  base::HistogramTester histogram_tester;
  SetupMockedOtpResponse(true);
  FormData otp_form = CreateOtpForm();
  SetupMockedServerPredictionResponse(
      CreateMockedServerResponseString(otp_form),
      CalculateFormSignature(otp_form));
  SeeForm(otp_form);

  // This line marks the form as interacted with which is a prerequisite for key
  // metrics to be emitted.
  autofill_manager().OnAskForValuesToFillTest(
      otp_form, otp_form.fields().front().global_id());

  // Trigger field type determination to start OTP retrieval.
  test_api(autofill_manager()).OnFormsParsed({otp_form});

  // Simulate that the suggestions are actually shown.
  DidShowAutofillSuggestions(otp_form, /*field_index=*/0);

  // Simulate user accepting the suggestion
  OtpFillData fill_data = {{otp_form.fields().front().global_id(), u"123456"}};
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, otp_form,
      otp_form.fields().front().global_id(), &fill_data,
      AutofillTriggerSource::kPopup);
  // Simulate the user correcting the value.
  SimulateUserChangedFieldTo(otp_form, otp_form.fields().front().global_id(),
                             u"654321");

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).otp_form_interactions_flow_id();
  SubmitForm(otp_form);
  DeleteDriverToCommitMetrics();

  VerifySingleSubmissionKeyMetricExpectations(histogram_tester,
                                              "OneTimePassword",
                                              {.readiness = true,
                                               .acceptance = true,
                                               .assistance = true,
                                               .correctness = false});
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
              {Ukm::kFlowIdName, flow_id.value()},
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
            {event_metrics(autofill_metrics::FORM_EVENT_INTERACTED_ONCE),
             event_metrics(autofill_metrics::FORM_EVENT_DID_PARSE_FORM),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN),
             event_metrics(autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED),
             event_metrics(
                 autofill_metrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE),
             event_metrics(autofill_metrics::
                               FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE)}));
  }
#endif
}

}  // namespace
}  // namespace autofill
