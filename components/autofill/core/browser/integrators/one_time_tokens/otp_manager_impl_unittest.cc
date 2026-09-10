// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl_test_api.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

    using ::autofill::test::FormDescription;
using ::autofill::test::GetServerTypes;
using ::base::test::RunOnceCallback;
using ::one_time_tokens::OneTimeTokenServiceImpl;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Test;

namespace autofill {

namespace {

constexpr char kDefaultOtpValue[] = "123456";
constexpr base::TimeDelta kTestFieldDetectionToTickleLatency =
    base::Milliseconds(420);

constexpr char kPhishGuardCheckPerformedHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.CheckPerformed";
constexpr char kPhishGuardLatencyHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.Latency";
constexpr char kPhishGuardVerdictHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.Verdict";

class MockSmsOtpBackend : public one_time_tokens::SmsOtpBackend {
 public:
  MOCK_METHOD(
      void,
      RetrieveSmsOtp,
      (base::OnceCallback<
          void(base::expected<one_time_tokens::OneTimeToken,
                              one_time_tokens::OneTimeTokenRetrievalError>)>),
      (override));
};

class MockOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  MOCK_METHOD(void,
              StartOtpPhishGuardCheck,
              (LocalFrameToken, base::OnceCallback<void(bool is_phishing)>),
              (override));
};

void SetUpTickleSubscription(
    one_time_tokens::MockOneTimeTokenService& mock_ott_service,
    one_time_tokens::ExpiringSubscriptionManager<
        void(one_time_tokens::OneTimeTokenSource)>& sub_manager) {
  ON_CALL(mock_ott_service,
          SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail, _, _))
      .WillByDefault(
          [&sub_manager](
              one_time_tokens::OneTimeTokenSource, base::Time exp,
              one_time_tokens::OneTimeTokenService::TickleCallback cb) {
            return sub_manager.Subscribe(
                exp, std::move(cb), /*expiration_callback=*/base::DoNothing());
          });
}

}  // namespace

class OtpManagerImplTest : public Test,
                           public WithTestAutofillClientDriverManager<> {
 public:
  OtpManagerImplTest() : one_time_token_service_(&sms_otp_backend_, nullptr) {}
  ~OtpManagerImplTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL("https://example.test"));
    auto otp_phish_guard_delegate =
        std::make_unique<MockOtpPhishGuardDelegate>();
    autofill_client().set_otp_phish_guard_delegate(
        std::move(otp_phish_guard_delegate));
    CreateAutofillDriver();
    test_field_.set_origin(url::Origin::Create(GURL("https://example.test")));
  }

  const FormStructure* AddForm(const FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    FormGlobalId form_id = form.global_id();
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));
    test_api(autofill_manager()).OnFormsParsed({form});

    // This would typically happen during parsing but is skipped if a form is
    // injected via the test API.
    autofill_manager().NotifyObservers(
        &TestBrowserAutofillManager::Observer::OnFieldTypesDetermined, form_id,
        TestBrowserAutofillManager::Observer::FieldTypeSource::kAutofillAiModel,
        /*small_forms_were_parsed=*/false);
    return autofill_manager().FindCachedFormById(form_id);
  }

  const FormStructure* AddFormWithOtpField(
      std::optional<url::Origin> field_origin = std::nullopt,
      std::optional<url::Origin> main_frame_origin = std::nullopt,
      bool is_focusable = true) {
    FormDescription form_description = {
        .fields =
            {
                {.server_type = ONE_TIME_CODE,
                 .is_focusable = is_focusable,
                 .label = u"OTP",
                 .name = u"otp",
                 .origin = field_origin},
            },
        .main_frame_origin = main_frame_origin,
    };
    return AddForm(form_description);
  }

  const FormStructure* AddFormWithFirstNameField() {
    FormDescription form_description = {
        .fields = {
            {.server_type = NAME_FIRST, .label = u"First name", .name = u"fn"},
        }};
    return AddForm(form_description);
  }

  MockOtpPhishGuardDelegate& otp_phish_guard_delegate() {
    return static_cast<MockOtpPhishGuardDelegate&>(
        *autofill_client().GetOtpPhishGuardDelegate());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  MockSmsOtpBackend sms_otp_backend_;
  OneTimeTokenServiceImpl one_time_token_service_;
  base::HistogramTester histogram_tester_;
  FormFieldData test_field_;
};

// Tests that no query is issued to the SMS backend if a form does not contain
// an OTP field.
TEST_F(OtpManagerImplTest, NonOtpForm_NoQueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // As the form has no OTP field, the SMS backend is not queried.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  AddFormWithFirstNameField();
}

// Tests that a query is issued to the SMS backend if a form contains an OTP
// field.
TEST_F(OtpManagerImplTest, OtpForm_QueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // As the form has a OTP field, the SMS backend is queried.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(1);
  AddFormWithOtpField();
}

// Tests that the FieldDetectionToTickleLatency metric is recorded when an OTP
// field is detected and a tickle arrives.
TEST_F(OtpManagerImplTest, FieldDetectionToTickleLatency_OtpFormLogsMetric) {
  base::test::ScopedFeatureList feature_list(features::kAutofillGmailOtp);
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &mock_ott_service, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  AddFormWithOtpField();

  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);
  sub_manager.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      kTestFieldDetectionToTickleLatency, 1);
}

// Tests that the FieldDetectionToTickleLatency metric is not recorded when no
// OTP field is detected.
TEST_F(OtpManagerImplTest,
       FieldDetectionToTickleLatency_NonOtpFormDoesNotLogMetric) {
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &mock_ott_service, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  AddFormWithFirstNameField();

  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);
  sub_manager.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

// Tests that OtpMetricsTracker records latency when a tickle arrives before an
// OTP field is detected.
TEST_F(OtpManagerImplTest,
       TickleReceivedBeforeOtpForm_NotifiesOtpMetricsTracker) {
  base::test::ScopedFeatureList feature_list(features::kAutofillGmailOtp);
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &mock_ott_service, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);

  sub_manager.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);

  AddFormWithOtpField();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      kTestFieldDetectionToTickleLatency, 1);
  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

// Tests that `GetOtpSuggestions` triggers an OTP retrieval from the
// `SmsOtpBackend` the first time it is called, and that the results are
// correctly passed to the callback.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_TriggersFirstRetrieval) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());
}

// Tests that `GetOtpSuggestions` waits with the callback if an SMS OTP
// retrieval is in progress.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_DoesNotTriggerWhileInProgress) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(otp);

  // The future should now be ready, and contain the OTP.
  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());
}

// Tests that `GetOtpSuggestions` immediately returns any OTPs that have
// already been fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FetchesSmsOnlyOnce) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false))
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form1 = AddFormWithOtpField();
  ASSERT_TRUE(form1);

  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(*form1, test_field_, future1.GetCallback());

  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0], otp.value());

  // Adding a second OTP form should not trigger a new SMS OTP retrieval.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  const FormStructure* form2 = AddFormWithOtpField();
  ASSERT_TRUE(form2);

  // The results of the first result should still be delivered.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(*form2, test_field_, future2.GetCallback());

  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0], otp.value());
}

// Tests that if `GetOtpSuggestions` is called twice, only the callback from
// the second call is run when OTPs are fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NewCallInvalidatesOldCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(*form, test_field_, future1.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future1.IsReady());

  // Call GetOtpSuggestions again. This should invalidate the first callback.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(*form, test_field_, future2.GetCallback());

  // The first future should still not be ready.
  EXPECT_FALSE(future1.IsReady());
  // The second future should also not be ready.
  EXPECT_FALSE(future2.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(otp);

  // The first future should still not be ready (it was invalidated).
  EXPECT_FALSE(future1.IsReady());
  // The second future should now be ready, and contain the OTP.
  EXPECT_TRUE(future2.IsReady());
  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0], otp.value());
}

// Tests that an empty OTP value received from the backend is not stored.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_EmptyOtpIsNotStored) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare a otp with an empty OTP.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    "", base::TimeTicks::Now());

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that `GetOtpSuggestions` filters out expired OTPs.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FiltersExpiredOtps) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the otp from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue,
                                    task_environment_.NowTicks());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // Request suggestions. The future should not be ready yet, as the SMS
  // backend has not responded.
  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(*form, test_field_, future1.GetCallback());
  EXPECT_FALSE(future1.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(otp);

  // The future should now be ready, and contain the fresh OTP.
  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0], otp.value());

  // Advance the clock by 6 minutes to make the OTP expire.
  task_environment_.AdvanceClock(base::Minutes(6));

  // Verify that the OTP is now expired and not returned.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(*form, test_field_, future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

// Tests that no suggestions are returned if the safety check returns false
// (unsafe).
TEST_F(OtpManagerImplTest, GetOtpSuggestions_SafetyCheckReturnsFalse) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, test_field_.host_frame());
        phish_guard_callback = std::move(callback);
      });

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The phish guard check is in progress, so the future should not be ready.
  EXPECT_FALSE(future.IsReady());

  // Simulate a 50ms latency in the phishing check.
  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(phish_guard_callback).Run(true);  // Unsafe (phishing detected)

  EXPECT_TRUE(future.Get().empty());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kPhishing*/ 1, 1);
}

// Tests that suggestions are returned if the safety check returns true.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_SafetyCheckReturnsTrue) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, test_field_.host_frame());
        phish_guard_callback = std::move(callback);
      });

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The phish guard check is in progress, so the future should not be ready.
  EXPECT_FALSE(future.IsReady());

  // Simulate a 50ms latency in the phishing check.
  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(phish_guard_callback).Run(false);  // Safe (no phishing)

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kNotPhishing*/ 2, 1);
}

// Tests that GetOtpSuggestions returns empty if the origin is opaque.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_OpaqueOriginReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<const std::vector<std::string>> future;
  FormFieldData field = test_field_;
  field.set_origin(url::Origin());
  otp_manager.GetOtpSuggestions(*form, field, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that the frame token of the field is passed to
// StartOtpPhishGuardCheck.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_PhishGuardCheckPassesFrameToken) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  LocalFrameToken subframe_token = test::MakeLocalFrameToken();
  FormFieldData field = *form->field(0);
  field.set_host_frame(subframe_token);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, subframe_token);
        std::move(callback).Run(false);
      });

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, field, future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());
}

// Tests that suggestions are returned if there is no phishing check delegate,
// and that the verdict is logged as kUnknown.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NoPhishingDelegate) {
  autofill_client().set_otp_phish_guard_delegate(nullptr);
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kUnknown*/ 0, 1);
}

// Tests that `OnOtpAvailable` is logged even if the PhishGuard check blocks delivery.
TEST_F(OtpManagerImplTest, OnOtpAvailable_LoggedEvenIfPhishGuardBlocks) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback = std::move(callback);
      });

  // Observing an OTP field triggers retrieval.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // Simulate unsafe site (phishing detection).
  std::move(phish_guard_callback).Run(true);

  // Suggestions should be empty because delivery is blocked.
  EXPECT_TRUE(future.Get().empty());

  // However, the metric should still be logged as the OTP was successfully retrieved.
  EXPECT_TRUE(autofill_manager()
                  .GetOtpFormEventLogger()
                  .HasLoggedDataToFillAvailableForTesting());
}

// Tests that `OnOtpAvailable` is not logged if there is no pending callback
// when the OTP arrives.
TEST_F(OtpManagerImplTest, OnOtpAvailable_NotLoggedIfNoPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });

  // Observing an OTP field triggers retrieval.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // Simulate a focus on a form field. This should clear the pending callback.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), FormGlobalId(),
                                       FieldGlobalId());

  EXPECT_FALSE(autofill_manager()
                   .GetOtpFormEventLogger()
                   .HasLoggedDataToFillAvailableForTesting());

  // Receive the token. The metric should not be logged because the pending
  // callback was cleared.
  std::move(sms_backend_callback).Run(otp);

  EXPECT_FALSE(autofill_manager()
                   .GetOtpFormEventLogger()
                   .HasLoggedDataToFillAvailableForTesting());
}

// Tests that `OnBeforeFocusOnFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a form field. This should clear the pending callback.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), FormGlobalId(),
                                       FieldGlobalId());

  // The future should now contain an empty vector.
  EXPECT_TRUE(future.Get().empty());

  // Now, let the SMS backend respond. This should not affect the already run
  // callback.
  std::move(sms_backend_callback).Run(otp);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `OnBeforeFocusOnNonFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnNonFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  base::OnceCallback<void(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(
                  base::expected<one_time_tokens::OneTimeToken,
                                 one_time_tokens::OneTimeTokenRetrievalError>)>
                  callback) { sms_backend_callback = std::move(callback); });

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a non-form field. This should clear the pending
  // callback.
  otp_manager.OnBeforeFocusOnNonFormField(autofill_manager());

  // The future should now contain an empty vector.
  EXPECT_TRUE(future.Get().empty());

  // Now, let the SMS backend respond. This should not affect the already run
  // callback.
  std::move(sms_backend_callback).Run(otp);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `SelectMostRecentToken` returns the most recent token.
TEST_F(OtpManagerImplTest, SelectMostRecentToken) {
  std::vector<one_time_tokens::OneTimeToken> tokens = {
      {one_time_tokens::OneTimeTokenType::kSmsOtp, "123",
       base::TimeTicks() + base::Seconds(10)},
      {one_time_tokens::OneTimeTokenType::kSmsOtp, "456",
       base::TimeTicks() + base::Seconds(20)},
      {one_time_tokens::OneTimeTokenType::kSmsOtp, "789",
       base::TimeTicks() + base::Seconds(15)}};

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  test_api(otp_manager).SetReceivedOtps(tokens);

  std::optional<one_time_tokens::OneTimeToken> selected_token =
      otp_manager.SelectMostRecentToken();
  ASSERT_TRUE(selected_token.has_value());
  EXPECT_EQ(selected_token->value(), "456");

  test_api(otp_manager).SetReceivedOtps({});
  EXPECT_FALSE(otp_manager.SelectMostRecentToken().has_value());
}

// Tests that when `kAutofillRestrictOtpToSameTldPlusOne` is enabled, no query
// is issued to the SMS backend if the OTP field is in a cross-origin iframe
// with mismatched TLD+1.
TEST_F(OtpManagerImplTest, CrossOriginOtpFormFeatureEnabledNoQueryIssued) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
}

// Tests that when `kAutofillRestrictOtpToSameTldPlusOne` is enabled, a query is
// issued to the SMS backend if the OTP field is in a same-TLD+1 iframe.
TEST_F(OtpManagerImplTest, SameTldPlusOneOtpFormFeatureEnabledQueryIssued) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(1);
  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
}

// Tests that when `kAutofillRestrictOtpToSameTldPlusOne` is disabled, a query
// is issued even if the OTP field has a mismatched TLD+1.
TEST_F(OtpManagerImplTest, CrossOriginOtpFormFeatureDisabledQueryIssued) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(1);
  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
}

// Tests that `GetOtpSuggestions` immediately returns empty suggestions without
// checking phishing or querying backend when the form contains a cross-origin
// OTP field and `kAutofillRestrictOtpToSameTldPlusOne` is enabled.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestionsCrossOriginFormFeatureEnabledReturnsEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `GetOtpSuggestions` returns OTP suggestions when the form is on a
// same-TLD+1 origin and `kAutofillRestrictOtpToSameTldPlusOne` is enabled.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestionsSameTldPlusOneFormFeatureEnabledReturnsOtp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());
}

// Tests that `GetOtpSuggestions` returns OTP suggestions on mismatched TLD+1
// when `kAutofillRestrictOtpToSameTldPlusOne` is disabled.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestionsCrossOriginFormFeatureDisabledReturnsOtp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(otp));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], otp.value());
}

// Tests that `GetOtpSuggestions` immediately returns empty suggestions if the
// OTP field is not focusable.
TEST_F(OtpManagerImplTest, GetOtpSuggestionsUnfocusableOtpFieldReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/std::nullopt, /*main_frame_origin=*/std::nullopt,
      /*is_focusable=*/false);
  ASSERT_TRUE(form);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `GetOtpSuggestions` immediately returns the mock OTP value
// when `kMockOtpValue` is specified on the command line, short-circuiting
// backend retrieval and PhishGuard checks.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_ReturnsMockOtpWhenSwitchIsSet) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      one_time_tokens::switches::kMockOtpValue, "987654");

  // When mock OTP is set, no SMS backend retrieval should occur.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // When GetOtpSuggestions is called, the mock OTP switch takes precedence and
  // is returned immediately without triggering PhishGuard checks.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), testing::ElementsAre("987654"));
}

// Tests that `GetOtpSuggestions` returns empty suggestions when the form is not
// an OTP form, even if `kMockOtpValue` is specified.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_MockOtpIgnoredForNonOtpForm) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      one_time_tokens::switches::kMockOtpValue, "987654");

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  FormData form_data;
  form_data.set_fields({autofill::test::CreateTestFormField(
      "Username", "username", "john_doe", FormControlType::kInputText)});
  auto form = std::make_unique<FormStructure>(form_data);

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that a tickle subscription is created upon OtpManagerImpl construction.
TEST_F(OtpManagerImplTest, TickleSubscriptionCreatedInConstructor) {
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  EXPECT_CALL(mock_ott_service,
              SubscribeToTickles(
                  one_time_tokens::OneTimeTokenSource::kGmail,
                  base::Time::Now() +
                      OtpManagerImpl::kGmailOtpTickleSubscriptionDuration,
                  _));

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() + OtpManagerImpl::kGmailOtpTickleSubscriptionDuration);
}

// Tests that an existing tickle subscription's expiration is renewed when an
// OTP form is parsed (OnFieldTypesDetermined).
TEST_F(OtpManagerImplTest, TickleSubscriptionRenewedOnOtpFormParsed) {
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  base::Time initial_expiration =
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime();

  // Advance clock by 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  // Parsing an OTP form should renew the subscription expiration to 5 minutes
  // from now.
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_GT(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      initial_expiration);
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() + OtpManagerImpl::kGmailOtpTickleSubscriptionDuration);
}

// Tests that a tickle subscription is recreated when an OTP form is parsed if
// the previous subscription expired.
TEST_F(OtpManagerImplTest,
       TickleSubscriptionRecreatedOnOtpFormParsedIfExpired) {
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Fast-forward past expiration so the subscription expires.
  task_environment_.FastForwardBy(
      OtpManagerImpl::kGmailOtpTickleSubscriptionDuration + base::Minutes(1));
  EXPECT_FALSE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Parsing an OTP form should recreate the subscription with a new 5-minute
  // expiration.
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() + OtpManagerImpl::kGmailOtpTickleSubscriptionDuration);
}

// Tests that receiving a push notification tickle triggers OnTickleReceived
// without crashing when subscribed.
TEST_F(OtpManagerImplTest, TickleReceivedTriggersOnTickleReceived) {
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      sub_manager;
  SetUpTickleSubscription(mock_ott_service, sub_manager);

  OtpManagerImpl otp_manager(autofill_manager(), &mock_ott_service);
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Notify tickle to trigger OnTickleReceived.
  sub_manager.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
}

// Tests that IsOtpFieldDetected returns false when no OtpFieldDetector is
// set on AutofillClient.
TEST_F(OtpManagerImplTest, IsOtpFieldDetected_NoDetector) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  EXPECT_FALSE(test_api(otp_manager).IsOtpFieldDetected());
}

// Tests that IsOtpFieldDetected returns false when an OtpFieldDetector is
// set but no OTP fields have been detected on the page.
TEST_F(OtpManagerImplTest, IsOtpFieldDetected_DetectorWithoutOtpField) {
  auto detector = std::make_unique<OtpFieldDetector>(nullptr);
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation{detector.get()};
  observation.Observe(&autofill_manager());
  autofill_client().set_otp_field_detector(std::move(detector));

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  EXPECT_FALSE(test_api(otp_manager).IsOtpFieldDetected());

  AddFormWithFirstNameField();
  EXPECT_FALSE(test_api(otp_manager).IsOtpFieldDetected());
}

// Tests that IsOtpFieldDetected returns true when an OtpFieldDetector is
// set and an OTP field is detected on the page.
TEST_F(OtpManagerImplTest, IsOtpFieldDetected_DetectorWithOtpField) {
  auto detector = std::make_unique<OtpFieldDetector>(nullptr);
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation{detector.get()};
  observation.Observe(&autofill_manager());
  autofill_client().set_otp_field_detector(std::move(detector));

  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).IsOtpFieldDetected());
}

// Tests that AnyOtpFieldContainsTypedInput returns false when no forms are
// cached in AutofillManager.
TEST_F(OtpManagerImplTest, AnyOtpFieldContainsTypedInput_NoForms) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns false when a non-OTP field
// has user typed input.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_NonOtpFieldWithTypedInput) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  const FormStructure* form = AddFormWithFirstNameField();
  ASSERT_TRUE(form);
  test_api(autofill_manager())
      .FindCachedFormById(form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns false when an OTP field has
// no user typed input.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_OtpFieldWithoutTypedInput) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns true when an OTP field has
// user typed input.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_OtpFieldWithTypedInput) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  test_api(autofill_manager())
      .FindCachedFormById(form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  EXPECT_TRUE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns false if only non-OTP
// fields have user typed input, but returns true once an OTP field has user
// typed input across multiple fields and forms.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_MultipleFieldsAndForms) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  FormDescription form_description = {
      .fields =
          {
              {.server_type = NAME_FIRST,
               .label = u"First name",
               .name = u"fn"},
              {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
          },
  };
  const FormStructure* multi_field_form = AddForm(form_description);
  ASSERT_TRUE(multi_field_form);

  const FormStructure* non_otp_form = AddFormWithFirstNameField();
  ASSERT_TRUE(non_otp_form);

  // Add user modifier to the non-OTP field in the multi-field form.
  test_api(autofill_manager())
      .FindCachedFormById(multi_field_form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  // Add user modifier to the non-OTP form.
  test_api(autofill_manager())
      .FindCachedFormById(non_otp_form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  // Since only non-OTP fields have user input, it should still return false.
  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());

  // Now add user modifier to the OTP field in the multi-field form.
  test_api(autofill_manager())
      .FindCachedFormById(multi_field_form->global_id())
      ->field(1)
      ->AddFieldModifier(FieldModifier::kUser);

  // Now an OTP field contains typed input, so it should return true.
  EXPECT_TRUE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that UserOptedIntoGmailOtpFilling returns false when the user opt-in
// preference is in its default state (disabled).
TEST_F(OtpManagerImplTest, UserOptedIntoGmailOtpFilling_Default) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  EXPECT_FALSE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

// Tests that UserOptedIntoGmailOtpFilling returns true when the user opt-in
// preference is enabled.
TEST_F(OtpManagerImplTest, UserOptedIntoGmailOtpFilling_Enabled) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  EXPECT_TRUE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

// Tests that UserOptedIntoGmailOtpFilling returns false when the user opt-in
// preference is disabled.
TEST_F(OtpManagerImplTest, UserOptedIntoGmailOtpFilling_Disabled) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);
  ASSERT_TRUE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());

  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  EXPECT_FALSE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

}  // namespace autofill
