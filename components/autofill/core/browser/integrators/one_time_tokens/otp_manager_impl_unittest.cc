// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
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
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::test::FormDescription;
using ::autofill::test::GetServerTypes;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
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

class MockOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  MOCK_METHOD(void,
              StartOtpPhishGuardCheck,
              (LocalFrameToken, base::OnceCallback<void(bool is_phishing)>),
              (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  explicit MockAutofillDriver(TestAutofillClient* client)
      : TestAutofillDriver(client) {}
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const FieldGlobalId&, AutofillSuggestionTriggerSource),
              (override));
};

}  // namespace

class OtpManagerImplTest
    : public Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 MockAutofillDriver> {
 public:
  OtpManagerImplTest() = default;
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
    const FormStructure* cached_form =
        autofill_manager().FindCachedFormById(form_id);
    if (cached_form && !cached_form->fields().empty()) {
      test_field_ = *cached_form->field(0);
    }
    return cached_form;
  }

  const FormStructure* AddFormWithOtpField(
      std::optional<url::Origin> field_origin = std::nullopt,
      std::optional<url::Origin> main_frame_origin = std::nullopt,
      bool is_focusable = true) {
    FormDescription form_description = {
        .fields =
            {
                {.server_type = ONE_TIME_CODE,
                 .host_frame = autofill_driver().GetFrameToken(),
                 .is_focusable = is_focusable,
                 .label = u"OTP",
                 .name = u"otp",
                 .origin = field_origin},
            },
        .host_frame = autofill_driver().GetFrameToken(),
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

  void SetUpOtpFieldDetector() {
    auto detector = std::make_unique<OtpFieldDetector>(nullptr);
    detector_observation_ = std::make_unique<
        base::ScopedObservation<AutofillManager, AutofillManager::Observer>>(
        detector.get());
    detector_observation_->Observe(&autofill_manager());
    autofill_client().set_otp_field_detector(std::move(detector));
  }

  one_time_tokens::GmailOtpRetriever::Result CreateSampleGmailOtpResult(
      std::string_view value = "987654") {
    return {
        .token = one_time_tokens::OneTimeToken(
            one_time_tokens::OneTimeTokenType::kGmail, std::string(value),
            base::TimeTicks::Now(), "sender@example.com"),
        .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
    };
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  one_time_tokens::FakeSmsOtpService sms_otp_service_;
  base::HistogramTester histogram_tester_;
  FormFieldData test_field_;
  std::unique_ptr<
      base::ScopedObservation<AutofillManager, AutofillManager::Observer>>
      detector_observation_;
};

// Tests that no SMS OTP subscription is started if a form does not contain an
// OTP field.
TEST_F(OtpManagerImplTest, NonOtpForm_NoQueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  // As the form has no OTP field, no SMS OTP subscription is started.
  AddFormWithFirstNameField();
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that an SMS OTP subscription is started if a form contains an OTP
// field.
TEST_F(OtpManagerImplTest, OtpForm_QueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  // As the form has a OTP field, an SMS OTP subscription is started.
  AddFormWithOtpField();
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);
}

// Tests that the FieldDetectionToTickleLatency metric is recorded when an OTP
// field is detected and a tickle arrives.
TEST_F(OtpManagerImplTest, FieldDetectionToTickleLatency_OtpFormLogsMetric) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillGmailOtpPreLaunchMetrics);
  one_time_tokens::FakeGmailOtpBackend gmail_otp_backend;

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &gmail_otp_backend, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithOtpField();

  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);
  gmail_otp_backend.OnIncomingOneTimeTokenBackendNotification(
      one_time_tokens::OneTimeTokenBackendNotification());

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      kTestFieldDetectionToTickleLatency, 1);
}

// Tests that the FieldDetectionToTickleLatency metric is not recorded when no
// OTP field is detected.
TEST_F(OtpManagerImplTest,
       FieldDetectionToTickleLatency_NonOtpFormDoesNotLogMetric) {
  one_time_tokens::FakeGmailOtpBackend gmail_otp_backend;

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &gmail_otp_backend, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithFirstNameField();

  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);
  gmail_otp_backend.OnIncomingOneTimeTokenBackendNotification(
      one_time_tokens::OneTimeTokenBackendNotification());

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

// Tests that OtpMetricsTracker records latency when a tickle arrives before an
// OTP field is detected.
TEST_F(OtpManagerImplTest,
       TickleReceivedBeforeOtpForm_NotifiesOtpMetricsTracker) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillGmailOtpPreLaunchMetrics);
  one_time_tokens::FakeGmailOtpBackend gmail_otp_backend;

  autofill_client().set_otp_metrics_tracker(std::make_unique<OtpMetricsTracker>(
      &gmail_otp_backend, autofill_client()));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  gmail_otp_backend.OnIncomingOneTimeTokenBackendNotification(
      one_time_tokens::OneTimeTokenBackendNotification());
  task_environment_.FastForwardBy(kTestFieldDetectionToTickleLatency);

  AddFormWithOtpField();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      kTestFieldDetectionToTickleLatency, 1);
  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

// Tests that observing an OTP field triggers an OTP retrieval from the
// `SmsOtpService`, and that the results are correctly passed to the
// `GetOtpSuggestions` callback.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_TriggersFirstRetrieval) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_EQ(sms_otp_service_.num_subscribers(), 1u);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());
}

// Tests that `GetOtpSuggestions` waits with the callback if an SMS OTP
// retrieval is in progress.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_DoesNotTriggerWhileInProgress) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_EQ(sms_otp_service_.num_subscribers(), 1u);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as no SMS OTP has arrived.
  EXPECT_FALSE(future.IsReady());

  // Now, let an SMS OTP arrive.
  sms_otp_service_.SimulateOtpArrived(otp);

  // The future should now be ready, and contain the OTP.
  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());
}

// Tests that `GetOtpSuggestions` immediately returns any OTPs that have
// already been fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FetchesSmsOnlyOnce) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false))
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form1 = AddFormWithOtpField();
  ASSERT_TRUE(form1);
  ASSERT_EQ(sms_otp_service_.num_subscribers(), 1u);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future1;
  otp_manager.GetOtpSuggestions(*form1, test_field_, future1.GetCallback());

  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0].value(), otp.value());

  // Adding a second OTP form should not trigger a new SMS OTP retrieval but
  // renew the existing subscription.
  const FormStructure* form2 = AddFormWithOtpField();
  ASSERT_TRUE(form2);
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);

  // The results of the first result should still be delivered.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future2;
  otp_manager.GetOtpSuggestions(*form2, test_field_, future2.GetCallback());

  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0].value(), otp.value());
}

// Tests that if `GetOtpSuggestions` is called twice, only the callback from
// the second call is run when OTPs are fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NewCallInvalidatesOldCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future1;
  otp_manager.GetOtpSuggestions(*form, test_field_, future1.GetCallback());

  // The future should not be ready yet, as no SMS OTP has arrived.
  EXPECT_FALSE(future1.IsReady());

  // Call GetOtpSuggestions again. This should invalidate the first callback by
  // running it with empty suggestions.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future2;
  otp_manager.GetOtpSuggestions(*form, test_field_, future2.GetCallback());

  // The first future should be resolved with empty suggestions.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future1.Get().empty());
  // The second future should not be ready yet.
  EXPECT_FALSE(future2.IsReady());

  // Now, let an SMS OTP arrive.
  sms_otp_service_.SimulateOtpArrived(otp);

  // The second future should now be ready, and contain the OTP.
  EXPECT_TRUE(future2.IsReady());
  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0].value(), otp.value());
}

// Tests that an empty OTP value received from the backend is not stored.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_EmptyOtpIsNotStored) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  // Prepare a otp with an empty OTP.
  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    "", base::TimeTicks::Now());

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that `GetOtpSuggestions` filters out expired OTPs.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FiltersExpiredOtps) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue,
                                    task_environment_.NowTicks());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // Request suggestions. The future should not be ready yet, as no SMS OTP has
  // arrived.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future1;
  otp_manager.GetOtpSuggestions(*form, test_field_, future1.GetCallback());
  EXPECT_FALSE(future1.IsReady());

  // Now, let an SMS OTP arrive.
  sms_otp_service_.SimulateOtpArrived(otp);

  // The future should now be ready, and contain the fresh OTP.
  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0].value(), otp.value());

  // Advance the clock by 6 minutes to make the OTP expire.
  task_environment_.AdvanceClock(base::Minutes(6));

  // Verify that the OTP is now expired and not returned.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future2;
  otp_manager.GetOtpSuggestions(*form, test_field_, future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

// Tests that no suggestions are returned if the safety check returns false
// (unsafe).
TEST_F(OtpManagerImplTest, GetOtpSuggestions_SafetyCheckReturnsFalse) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, test_field_.host_frame());
        phish_guard_callback = std::move(callback);
      });

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
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
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, test_field_.host_frame());
        phish_guard_callback = std::move(callback);
      });

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The phish guard check is in progress, so the future should not be ready.
  EXPECT_FALSE(future.IsReady());

  // Simulate a 50ms latency in the phishing check.
  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(phish_guard_callback).Run(false);  // Safe (no phishing)

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kNotPhishing*/ 2, 1);
}

// Tests that GetOtpSuggestions returns empty if the origin is opaque.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_OpaqueOriginReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_EQ(sms_otp_service_.num_subscribers(), 1u);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  FormFieldData field = test_field_;
  field.set_origin(url::Origin());
  otp_manager.GetOtpSuggestions(*form, field, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that the frame token of the field is passed to
// StartOtpPhishGuardCheck.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_PhishGuardCheckPassesFrameToken) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  LocalFrameToken subframe_token = test::MakeLocalFrameToken();
  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = ONE_TIME_CODE,
               .host_frame = subframe_token,
               .label = u"OTP",
               .name = u"otp"},
          },
  });
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken frame_to_fill,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        EXPECT_EQ(frame_to_fill, subframe_token);
        std::move(callback).Run(false);
      });

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());
}

// Tests that suggestions are returned if there is no phishing check delegate,
// and that the verdict is logged as kUnknown.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NoPhishingDelegate) {
  autofill_client().set_otp_phish_guard_delegate(nullptr);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kUnknown*/ 0, 1);
}

// Tests that `OnOtpAvailable` is logged even if the PhishGuard check blocks
// delivery.
TEST_F(OtpManagerImplTest, OnOtpAvailable_LoggedEvenIfPhishGuardBlocks) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](LocalFrameToken,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback = std::move(callback);
      });

  // Observing an OTP field triggers retrieval.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // Simulate unsafe site (phishing detection).
  std::move(phish_guard_callback).Run(true);

  // Suggestions should be empty because delivery is blocked.
  EXPECT_TRUE(future.Get().empty());

  // However, the metric should still be logged as the OTP was successfully
  // retrieved.
  EXPECT_TRUE(autofill_manager()
                  .GetOtpFormEventLogger()
                  .HasLoggedDataToFillAvailableForTesting());
}

// Tests that `OnOtpAvailable` is not logged if there is no pending callback
// when the OTP arrives.
TEST_F(OtpManagerImplTest, OnOtpAvailable_NotLoggedIfNoPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  // Observing an OTP field triggers retrieval.
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // Simulate a focus on a form field. This should clear the pending callback.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), FormGlobalId(),
                                       FieldGlobalId());

  EXPECT_FALSE(autofill_manager()
                   .GetOtpFormEventLogger()
                   .HasLoggedDataToFillAvailableForTesting());

  // Receive the token. The metric should not be logged because the pending
  // callback was cleared.
  sms_otp_service_.SimulateOtpArrived(otp);

  EXPECT_FALSE(autofill_manager()
                   .GetOtpFormEventLogger()
                   .HasLoggedDataToFillAvailableForTesting());
}

// Tests that `OnBeforeFocusOnFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as no SMS OTP has arrived.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a form field. This should clear the pending callback.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), FormGlobalId(),
                                       FieldGlobalId());

  // The future should now contain an empty vector.
  EXPECT_TRUE(future.Get().empty());

  // Now, let an SMS OTP arrive. This should not affect the already run
  // callback.
  sms_otp_service_.SimulateOtpArrived(otp);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `OnBeforeFocusOnNonFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnNonFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  // The future should not be ready yet, as no SMS OTP has arrived.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a non-form field. This should clear the pending
  // callback.
  otp_manager.OnBeforeFocusOnNonFormField(autofill_manager());

  // The future should now contain an empty vector.
  EXPECT_TRUE(future.Get().empty());

  // Now, let an SMS OTP arrive. This should not affect the already run
  // callback.
  sms_otp_service_.SimulateOtpArrived(otp);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `SelectMostRecentToken` returns `last_retrieved_gmail_otp_` when
// it is present and not expired.
TEST_F(OtpManagerImplTest, SelectMostRecentToken_ReturnsLastRetrievedGmailOtp) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  one_time_tokens::OneTimeToken token(
      one_time_tokens::OneTimeTokenType::kGmail, "789",
      base::TimeTicks::Now() - base::Seconds(15), "sender@example.com");
  test_api(otp_manager).set_last_retrieved_gmail_otp(token);

  std::optional<one_time_tokens::OneTimeToken> selected_token =
      test_api(otp_manager).SelectMostRecentToken();
  ASSERT_TRUE(selected_token.has_value());
  EXPECT_EQ(selected_token->value(), "789");
}

// Tests that `SelectMostRecentToken` returns nullopt when
// `last_retrieved_gmail_otp_` is expired (older than 3 minutes).
TEST_F(OtpManagerImplTest,
       SelectMostRecentToken_ReturnsNulloptWhenTokenExpired) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager)
      .set_last_retrieved_gmail_otp(one_time_tokens::OneTimeToken(
          one_time_tokens::OneTimeTokenType::kGmail, "999",
          base::TimeTicks::Now() - base::Minutes(5), "sender@example.com"));

  EXPECT_FALSE(test_api(otp_manager).SelectMostRecentToken().has_value());
}

// Tests that `SelectMostRecentToken` returns nullopt when no Gmail OTP has been
// retrieved.
TEST_F(OtpManagerImplTest,
       SelectMostRecentToken_ReturnsNulloptWhenCacheIsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  EXPECT_FALSE(test_api(otp_manager).SelectMostRecentToken().has_value());
}

// Tests that `GetOtpSuggestions` synchronously serves a cached Gmail OTP token
// if present in memory, while still renewing subscriptions.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_CachedGmailOtpServedSynchronously) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  one_time_tokens::OneTimeToken gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "sender@example.com");

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager).set_last_retrieved_gmail_otp(gmail_otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), "654321");
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_TRUE(autofill_manager()
                  .GetOtpFormEventLogger()
                  .HasLoggedDataToFillAvailableForTesting());
  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kNotPhishing,
      1);
}

// Tests that `GetOtpSuggestions` suppresses delivery of cached Gmail OTPs
// when PhishGuard identifies a phishing risk, while still renewing
// subscriptions.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_CachedGmailOtpSuppressedWhenPhishing) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  one_time_tokens::OneTimeToken gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "sender@example.com");

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager).set_last_retrieved_gmail_otp(gmail_otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);
  EXPECT_TRUE(autofill_manager()
                  .GetOtpFormEventLogger()
                  .HasLoggedDataToFillAvailableForTesting());
  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kPhishing,
      1);
}

// Tests that subsequent tokens returned by `GetRecentOneTimeTokens` do not
// overwrite or duplicate a cached Gmail OTP that was already served.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_SubsequentBackendTokensDoNotOverwriteCachedGmailOtp) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  one_time_tokens::OneTimeToken gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "sender@example.com");
  one_time_tokens::OneTimeToken backend_sms_otp(
      one_time_tokens::OneTimeTokenType::kSmsOtp, "999999",
      base::TimeTicks::Now());

  // Cache an SMS OTP in the `SmsOtpService` so that it is returned by
  // `GetRecentOneTimeTokens`.
  sms_otp_service_.SimulateOtpArrived(backend_sms_otp);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager).set_last_retrieved_gmail_otp(gmail_otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), "654321");
}

// Tests that `GetOtpSuggestions` falls back to querying the backend when the
// cached Gmail OTP token is expired.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_ExpiredCachedGmailOtpFallsBackToBackend) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  one_time_tokens::OneTimeToken expired_gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now() - base::Minutes(4), "sender@example.com");

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager).set_last_retrieved_gmail_otp(expired_gmail_otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);
}

// Tests that no SMS OTP subscription is started if the OTP field is in a
// cross-origin iframe with mismatched TLD+1.
TEST_F(OtpManagerImplTest, CrossOriginOtpFormNoQueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that an SMS OTP subscription is started if the OTP field is in a
// same-TLD+1 iframe.
TEST_F(OtpManagerImplTest, SameTldPlusOneOtpFormQueryIssued) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 1u);
}

// Tests that `GetOtpSuggestions` immediately returns empty suggestions without
// checking phishing or subscribing to SMS OTPs when the form contains a
// cross-origin OTP field.
TEST_F(OtpManagerImplTest, GetOtpSuggestionsCrossOriginFormReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that when the driver is for an embedded frame tree (e.g. fenced frame),
// no SMS OTP subscription is started even if field and main frame origins
// match.
TEST_F(OtpManagerImplTest, EmbeddedFrameTreeNoQueryIssued) {
  autofill_driver().SetIsEmbedded(true);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that `GetOtpSuggestions` immediately returns empty suggestions when the
// driver is for an embedded frame tree (e.g. fenced frame).
TEST_F(OtpManagerImplTest, EmbeddedFrameTreeGetOtpSuggestionsReturnsEmpty) {
  autofill_driver().SetIsEmbedded(true);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that `GetOtpSuggestions` returns OTP suggestions when the form is on a
// same-TLD+1 origin.
TEST_F(OtpManagerImplTest, GetOtpSuggestionsSameTldPlusOneFormReturnsOtp) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);
  sms_otp_service_.SimulateOtpArrived(otp);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), otp.value());
}

// Tests that `GetOtpSuggestions` immediately returns empty suggestions if the
// OTP field is not focusable.
TEST_F(OtpManagerImplTest, GetOtpSuggestionsUnfocusableOtpFieldReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/std::nullopt, /*main_frame_origin=*/std::nullopt,
      /*is_focusable=*/false);
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that in a same-origin form containing both a password `ONE_TIME_CODE`
// field and a text `ONE_TIME_CODE` field, `GetOtpSuggestions` returns empty
// suggestions when triggered on the password field, and returns the OTP when
// triggered on the text field.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestionsPasswordFieldInMixedFormReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  one_time_tokens::OneTimeToken otp(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue, base::TimeTicks::Now());

  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = ONE_TIME_CODE,
               .is_focusable = true,
               .label = u"Password OTP",
               .name = u"pw_otp",
               .form_control_type = FormControlType::kInputPassword},
              {.server_type = ONE_TIME_CODE,
               .is_focusable = true,
               .label = u"Text OTP",
               .name = u"text_otp",
               .form_control_type = FormControlType::kInputText},
          },
  });
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 2u);
  sms_otp_service_.SimulateOtpArrived(otp);

  // Triggering on the password field must not return suggestions.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      password_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                password_future.GetCallback());
  EXPECT_TRUE(password_future.IsReady());
  EXPECT_TRUE(password_future.Get().empty());

  // Triggering on the text OTP field returns the OTP suggestion.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      text_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(1),
                                text_future.GetCallback());
  ASSERT_EQ(text_future.Get().size(), 1u);
  EXPECT_EQ(text_future.Get()[0].value(), otp.value());
}

// Tests that a cross-origin password `ONE_TIME_CODE` field injected alongside a
// same-origin text `ONE_TIME_CODE` field blocks OTP retrieval and suggestions.
TEST_F(OtpManagerImplTest,
       CrossOriginPasswordOtpFieldWithSameOriginTextOtpFieldReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = ONE_TIME_CODE,
               .is_focusable = true,
               .label = u"Cross-origin Password OTP",
               .name = u"attacker_otp",
               .form_control_type = FormControlType::kInputPassword,
               .origin = url::Origin::Create(GURL("https://attacker.test"))},
              {.server_type = ONE_TIME_CODE,
               .is_focusable = true,
               .label = u"Main-frame Text OTP",
               .name = u"main_otp",
               .form_control_type = FormControlType::kInputText,
               .origin = url::Origin::Create(GURL("https://example.test"))},
          },
      .main_frame_origin = url::Origin::Create(GURL("https://example.test")),
  });
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 2u);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      cross_origin_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                cross_origin_future.GetCallback());
  EXPECT_TRUE(cross_origin_future.IsReady());
  EXPECT_TRUE(cross_origin_future.Get().empty());

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      same_origin_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(1),
                                same_origin_future.GetCallback());
  EXPECT_TRUE(same_origin_future.IsReady());
  EXPECT_TRUE(same_origin_future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that `GetOtpSuggestions` returns empty suggestions if the trigger field
// itself has a cross-origin (mismatched eTLD+1) origin, even when `form` is a
// valid same-origin OTP form.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestionsCrossOriginTriggerFieldOnValidFormReturnsEmpty) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 1u);
  ASSERT_EQ(sms_otp_service_.num_subscribers(), 1u);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  FormFieldData cross_origin_field = *form->field(0);
  cross_origin_field.set_origin(
      url::Origin::Create(GURL("https://attacker.test")));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, cross_origin_field,
                                future.GetCallback());
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

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // When GetOtpSuggestions is called, the mock OTP switch takes precedence and
  // is returned immediately without triggering PhishGuard checks.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      future.Get(),
      ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "987654")));

  // When mock OTP is set, no SMS OTP retrieval should occur.
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that `GetOtpSuggestions` returns empty suggestions when the form is not
// an OTP form, even if `kMockOtpValue` is specified.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_MockOtpIgnoredForNonOtpForm) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      one_time_tokens::switches::kMockOtpValue, "987654");

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  FormData form_data;
  form_data.set_fields({autofill::test::CreateTestFormField(
      "Username", "username", "john_doe", FormControlType::kInputText)});
  auto form = std::make_unique<FormStructure>(form_data);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_EQ(sms_otp_service_.num_subscribers(), 0u);
}

// Tests that a tickle subscription is not created in the constructor or when a
// non-OTP form is parsed, and is lazily created with the `GmailOtpBackend` once
// an OTP form is parsed.
TEST_F(OtpManagerImplTest, TickleSubscriptionCreatedLazilyOnOtpFormParsed) {
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  EXPECT_FALSE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Parsing a non-OTP form should still not subscribe to tickles.
  AddFormWithFirstNameField();
  EXPECT_FALSE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Parsing an OTP form should lazily create the tickle subscription.
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() +
          OtpManagerImplTestApi::kGmailOtpTickleSubscriptionDuration);
}

// Tests that no tickle subscription is created if no `GmailOtpBackend` is
// available.
TEST_F(OtpManagerImplTest, NoTickleSubscriptionWithoutGmailOtpBackend) {
  autofill_client().set_gmail_otp_backend(nullptr);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithOtpField();
  EXPECT_FALSE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
}

// Tests that an existing tickle subscription's expiration is renewed when a
// subsequent OTP form is parsed.
TEST_F(OtpManagerImplTest, TickleSubscriptionRenewedOnSubsequentOtpFormParsed) {
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithOtpField();
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  base::Time initial_expiration =
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime();

  // Advance clock by 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  // Parsing another OTP form should renew the subscription expiration to 5
  // minutes from now.
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_GT(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      initial_expiration);
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() +
          OtpManagerImplTestApi::kGmailOtpTickleSubscriptionDuration);
}

// Tests that a tickle subscription is recreated when an OTP form is parsed if
// the previous subscription expired.
TEST_F(OtpManagerImplTest,
       TickleSubscriptionRecreatedOnOtpFormParsedIfExpired) {
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithOtpField();
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Fast-forward past expiration so the subscription expires.
  task_environment_.FastForwardBy(
      OtpManagerImplTestApi::kGmailOtpTickleSubscriptionDuration +
      base::Minutes(1));
  EXPECT_FALSE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());

  // Parsing an OTP form should recreate the subscription with a new 5-minute
  // expiration.
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  EXPECT_EQ(
      test_api(otp_manager).gmail_otp_tickle_subscription().GetExpirationTime(),
      base::Time::Now() +
          OtpManagerImplTestApi::kGmailOtpTickleSubscriptionDuration);
}

// Tests that a tickle notified by the `GmailOtpBackend` is handled by
// `OnTickleReceived` and starts `GmailOtpRetriever`.
TEST_F(OtpManagerImplTest, GmailOtpBackendTickleStartsGmailOtpRetriever) {
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  auto backend = std::make_unique<one_time_tokens::FakeGmailOtpBackend>();
  one_time_tokens::FakeGmailOtpBackend* backend_ptr = backend.get();
  autofill_client().set_gmail_otp_backend(std::move(backend));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  // As the user has not opted into Gmail OTP filling yet, parsing an OTP form
  // subscribes to tickles but does not start `GmailOtpRetriever`.
  AddFormWithOtpField();
  ASSERT_TRUE(test_api(otp_manager).gmail_otp_tickle_subscription().IsAlive());
  ASSERT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  // Once the user opted in, a tickle notified by the backend starts
  // `GmailOtpRetriever`.
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  backend_ptr->OnIncomingOneTimeTokenBackendNotification(
      one_time_tokens::OneTimeTokenBackendNotification());
  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that IsOtpFieldDetected returns false when no OtpFieldDetector is
// set on AutofillClient.
TEST_F(OtpManagerImplTest, IsOtpFieldDetected_NoDetector) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
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

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
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

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  AddFormWithOtpField();
  EXPECT_TRUE(test_api(otp_manager).IsOtpFieldDetected());
}

// Tests that AnyOtpFieldContainsTypedInput returns false when no forms are
// cached in AutofillManager.
TEST_F(OtpManagerImplTest, AnyOtpFieldContainsTypedInput_NoForms) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns false when a non-OTP field
// has user typed input.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_NonOtpFieldWithTypedInput) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
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
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  EXPECT_FALSE(test_api(otp_manager).AnyOtpFieldContainsTypedInput());
}

// Tests that AnyOtpFieldContainsTypedInput returns true when an OTP field has
// user typed input.
TEST_F(OtpManagerImplTest,
       AnyOtpFieldContainsTypedInput_OtpFieldWithTypedInput) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
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
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

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
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  EXPECT_FALSE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

// Tests that UserOptedIntoGmailOtpFilling returns true when the user opt-in
// preference is enabled.
TEST_F(OtpManagerImplTest, UserOptedIntoGmailOtpFilling_Enabled) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  EXPECT_TRUE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

// Tests that UserOptedIntoGmailOtpFilling returns false when the user opt-in
// preference is disabled.
TEST_F(OtpManagerImplTest, UserOptedIntoGmailOtpFilling_Disabled) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  ASSERT_TRUE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());

  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  EXPECT_FALSE(test_api(otp_manager).UserOptedIntoGmailOtpFilling());
}

// Tests that GetOtpSuggestions immediately invokes the callback with empty
// suggestions if SmsOtpService is nullptr.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NullServiceInvokesCallback) {
  OtpManagerImpl otp_manager(autofill_manager(),
                             /*sms_otp_service=*/nullptr);
  const FormStructure* form = AddFormWithOtpField();
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that GetOtpSuggestions invokes the callback with empty suggestions
// when SmsOtpService has no SMS backend (e.g. Desktop).
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NullSmsBackendInvokesCallback) {
  sms_otp_service_.set_platform_supported(false);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that a pending GetOtpSuggestions callback is invoked with empty
// suggestions if overwritten by a subsequent call or if subscription expires.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_OverwrittenOrExpiredCallbackInvoked) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      first_future;
  otp_manager.GetOtpSuggestions(*form, test_field_, first_future.GetCallback());
  EXPECT_FALSE(first_future.IsReady());

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      second_future;
  otp_manager.GetOtpSuggestions(*form, test_field_,
                                second_future.GetCallback());
  EXPECT_TRUE(first_future.IsReady());
  EXPECT_TRUE(first_future.Get().empty());
  EXPECT_FALSE(second_future.IsReady());

  // Fast-forward past subscription expiration.
  task_environment_.FastForwardBy(
      OtpManagerImplTestApi::kSmsOtpSubscriptionDuration + base::Seconds(1));
  EXPECT_TRUE(second_future.IsReady());
  EXPECT_TRUE(second_future.Get().empty());
}

// Tests that destroying `OtpManagerImpl` while a `GetOtpSuggestions` request
// is still pending resolves the callback with empty suggestions.
TEST_F(OtpManagerImplTest,
       Destructor_ResolvesPendingGetOtpSuggestionsCallbackWithEmpty) {
  auto otp_manager =
      std::make_unique<OtpManagerImpl>(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager->GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  otp_manager.reset();
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that focus events correctly track the currently focused field and form.
TEST_F(OtpManagerImplTest, FocusTracking_SetsAndClearsFocusedFieldAndForm) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();
  FormGlobalId form_id = form->global_id();

  EXPECT_FALSE(test_api(otp_manager).currently_focused_field_id().has_value());
  EXPECT_FALSE(test_api(otp_manager).currently_focused_form_id().has_value());

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form_id,
                                       otp_field_id);
  EXPECT_EQ(test_api(otp_manager).currently_focused_field_id(), otp_field_id);
  EXPECT_EQ(test_api(otp_manager).currently_focused_form_id(), form_id);

  otp_manager.OnBeforeFocusOnNonFormField(autofill_manager());
  EXPECT_FALSE(test_api(otp_manager).currently_focused_field_id().has_value());
  EXPECT_FALSE(test_api(otp_manager).currently_focused_form_id().has_value());
}

// Tests that `GetTargetOtpField` returns the focused field when it is an OTP
// field.
TEST_F(OtpManagerImplTest, GetTargetOtpField_ReturnsOtpFieldWhenFocused) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_FALSE(form->fields().empty());
  FieldGlobalId otp_field_id = form->field(0)->global_id();
  FormGlobalId form_id = form->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form_id,
                                       otp_field_id);
  const AutofillField* focused_otp_field =
      test_api(otp_manager).GetTargetOtpField();
  ASSERT_NE(focused_otp_field, nullptr);
  EXPECT_EQ(focused_otp_field->global_id(), otp_field_id);
}

// Tests that `GetTargetOtpField` returns nullptr when the currently focused
// field is not an OTP field, even if an OTP field exists in cached forms.
TEST_F(OtpManagerImplTest,
       GetTargetOtpField_ReturnsNullptrWhenNonOtpFieldFocused) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = NAME_FIRST,
               .label = u"First name",
               .name = u"fn"},
              {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
          },
  });
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 2u);
  FieldGlobalId name_field_id = form->field(0)->global_id();
  FormGlobalId form_id = form->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form_id,
                                       name_field_id);
  EXPECT_EQ(test_api(otp_manager).GetTargetOtpField(), nullptr);
}

// Tests that `GetTargetOtpField` returns the OTP field from cached forms when
// no field is focused.
TEST_F(OtpManagerImplTest,
       GetTargetOtpField_ReturnsOtpFieldWhenNoFieldFocused) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_FALSE(form->fields().empty());

  const AutofillField* target_otp_field =
      test_api(otp_manager).GetTargetOtpField();
  ASSERT_NE(target_otp_field, nullptr);
  EXPECT_EQ(target_otp_field->global_id(), form->field(0)->global_id());
}

// Tests that `GetTargetOtpField` returns nullptr when no OTP field exists in
// cached forms.
TEST_F(OtpManagerImplTest,
       GetTargetOtpField_ReturnsNullptrWhenNoOtpFieldInCachedForms) {
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithFirstNameField();
  ASSERT_TRUE(form);

  EXPECT_EQ(test_api(otp_manager).GetTargetOtpField(), nullptr);
}

class OtpManagerImplDeliveryTest : public OtpManagerImplTest {
 public:
  void SetUp() override {
    OtpManagerImplTest::SetUp();
    prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                             true);
    otp_manager_.emplace(autofill_manager(), &sms_otp_service_);
    form_ = AddFormWithOtpField();
    ASSERT_TRUE(form_);
  }

  void TearDown() override {
    form_ = nullptr;
    otp_manager_.reset();
    OtpManagerImplTest::TearDown();
  }

  OtpManagerImpl& otp_manager() LIFETIME_BOUND { return *otp_manager_; }
  const FormStructure& form() const LIFETIME_BOUND { return *form_; }

  void RequestOtpSuggestions(
      base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>&
          future) {
    otp_manager().GetOtpSuggestions(form(), test_field_, future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  void VerifyIgnoresStaleFrameVerdictWhenFrameChanges(
      one_time_tokens::OneTimeTokenType token_type) {
    LocalFrameToken frame_a = autofill_driver().GetFrameToken();
    LocalFrameToken frame_b = test::MakeLocalFrameToken();
    const FormStructure* multi_frame_form = AddForm({
        .fields =
            {
                {.server_type = ONE_TIME_CODE,
                 .host_frame = frame_a,
                 .label = u"OTP1",
                 .name = u"otp1"},
                {.server_type = ONE_TIME_CODE,
                 .host_frame = frame_b,
                 .label = u"OTP2",
                 .name = u"otp2"},
            },
        .host_frame = frame_a,
    });
    ASSERT_TRUE(multi_frame_form);
    ASSERT_EQ(multi_frame_form->fields().size(), 2u);

    base::OnceCallback<void(bool is_phishing)> phish_guard_callback_a_safe;
    base::OnceCallback<void(bool is_phishing)> phish_guard_callback_a_phishing;
    base::OnceCallback<void(bool is_phishing)> phish_guard_callback_b;

    EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck(frame_a, _))
        .WillOnce([&](LocalFrameToken,
                      base::OnceCallback<void(bool is_phishing)> callback) {
          phish_guard_callback_a_safe = std::move(callback);
        })
        .WillOnce([&](LocalFrameToken,
                      base::OnceCallback<void(bool is_phishing)> callback) {
          phish_guard_callback_a_phishing = std::move(callback);
        });
    EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck(frame_b, _))
        .WillOnce([&](LocalFrameToken,
                      base::OnceCallback<void(bool is_phishing)> callback) {
          phish_guard_callback_b = std::move(callback);
        });

    std::optional<std::string> sender_address =
        token_type == one_time_tokens::OneTimeTokenType::kGmail
            ? std::optional<std::string>("sender@example.com")
            : std::nullopt;

    // Start request 1 on `frame_a` and trigger two in-flight PhishGuard checks
    // on `frame_a` (to test both stale safe and stale phishing verdicts).
    base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future_a;
    otp_manager().GetOtpSuggestions(
        *multi_frame_form, *multi_frame_form->field(0), future_a.GetCallback());
    EXPECT_FALSE(future_a.IsReady());

    test_api(otp_manager())
        .OnOneTimeTokenReceived(one_time_tokens::OneTimeToken(
            token_type, "111111", base::TimeTicks::Now(), sender_address));
    test_api(otp_manager())
        .OnOneTimeTokenReceived(one_time_tokens::OneTimeToken(
            token_type, "111111", base::TimeTicks::Now(), sender_address));
    ASSERT_FALSE(phish_guard_callback_a_safe.is_null());
    ASSERT_FALSE(phish_guard_callback_a_phishing.is_null());

    // Before the `frame_a` checks finish, start request 2 on `frame_b` (cancels
    // `future_a` with empty suggestions) and receive an OTP for `frame_b`.
    base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future_b;
    otp_manager().GetOtpSuggestions(
        *multi_frame_form, *multi_frame_form->field(1), future_b.GetCallback());
    EXPECT_TRUE(future_a.IsReady());
    EXPECT_TRUE(future_a.Get().empty());
    EXPECT_FALSE(future_b.IsReady());

    test_api(otp_manager())
        .OnOneTimeTokenReceived(one_time_tokens::OneTimeToken(
            token_type, "222222", base::TimeTicks::Now(), sender_address));
    ASSERT_FALSE(phish_guard_callback_b.is_null());

    // Stale safe verdict from `frame_a` must not resolve `future_b`, cache the
    // token, or mutate `last_triggered_otp_value_`.
    std::move(phish_guard_callback_a_safe).Run(/*is_phishing=*/false);
    EXPECT_FALSE(future_b.IsReady());
    EXPECT_EQ(test_api(otp_manager()).SelectMostRecentToken(), std::nullopt);
    EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());

    // Stale phishing verdict from `frame_a` must not suppress/cancel
    // `future_b`.
    std::move(phish_guard_callback_a_phishing).Run(/*is_phishing=*/true);
    EXPECT_FALSE(future_b.IsReady());

    // Check B completes with a safe verdict; `future_b` resolves with the
    // second token.
    std::move(phish_guard_callback_b).Run(/*is_phishing=*/false);
    EXPECT_TRUE(future_b.IsReady());
    EXPECT_THAT(
        future_b.Get(),
        ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "222222")));

    histogram_tester_.ExpectBucketCount(
        kPhishGuardVerdictHistogram,
        OneTimeTokensPhishGuardVerdict::kNotPhishing, 2);
    histogram_tester_.ExpectBucketCount(
        kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kPhishing,
        1);
  }

 private:
  std::optional<OtpManagerImpl> otp_manager_;
  raw_ptr<const FormStructure> form_ = nullptr;
};

// Tests that SMS OTP suggestion delivery works and delivers the suggestion to
// the pending callback when PhishGuard approves.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForSms_DeliversSuggestionsWhenNotPhishing) {
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  one_time_tokens::OneTimeToken token(
      one_time_tokens::OneTimeTokenType::kSmsOtp, kDefaultOtpValue,
      base::TimeTicks::Now());
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(),
              ElementsAre(Property(&one_time_tokens::OneTimeToken::value,
                                   kDefaultOtpValue)));
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kNotPhishing,
      1);
  histogram_tester_.ExpectTotalCount(kPhishGuardLatencyHistogram, 1);
}

// Tests that SMS OTP suggestion delivery is suppressed when PhishGuard reports
// phishing.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForSms_SuppressedWhenPhishing) {
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  one_time_tokens::OneTimeToken token(
      one_time_tokens::OneTimeTokenType::kSmsOtp, kDefaultOtpValue,
      base::TimeTicks::Now());
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kPhishing,
      1);
}

// Tests that Gmail OTP suggestion delivery works and delivers the suggestion to
// the pending callback when PhishGuard approves.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForGmail_DeliversSuggestionsWhenNotPhishing) {
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  one_time_tokens::OneTimeToken token(one_time_tokens::OneTimeTokenType::kGmail,
                                      kDefaultOtpValue, base::TimeTicks::Now(),
                                      "sender@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(),
              ElementsAre(Property(&one_time_tokens::OneTimeToken::value,
                                   kDefaultOtpValue)));
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kNotPhishing,
      1);
  histogram_tester_.ExpectTotalCount(kPhishGuardLatencyHistogram, 1);
}

// Tests that Gmail OTP suggestion delivery is suppressed when PhishGuard
// reports phishing.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForGmail_SuppressedWhenPhishing) {
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  one_time_tokens::OneTimeToken token(one_time_tokens::OneTimeTokenType::kGmail,
                                      kDefaultOtpValue, base::TimeTicks::Now(),
                                      "sender@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram, OneTimeTokensPhishGuardVerdict::kPhishing,
      1);
}

// Tests that Gmail OTP suggestion delivery invokes the callback with empty
// suggestions if the token value is empty.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForGmail_EmptyTokenInvokesCallbackWithEmpty) {
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  one_time_tokens::OneTimeToken empty_token(
      one_time_tokens::OneTimeTokenType::kGmail, "", base::TimeTicks::Now(),
      "sender@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(empty_token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that Gmail OTP suggestion delivery invokes the pending callback with
// empty suggestions without starting a PhishGuard check if the user has not
// opted into Gmail OTP filling.
TEST_F(OtpManagerImplDeliveryTest,
       OnOneTimeTokenReceived_GmailSuppressedWhenUserNotOptedIn) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  RequestOtpSuggestions(future);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  one_time_tokens::OneTimeToken token(one_time_tokens::OneTimeTokenType::kGmail,
                                      kDefaultOtpValue, base::TimeTicks::Now(),
                                      "sender@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that PhishGuard check latency is measured per-request correctly when
// multiple checks overlap.
TEST_F(OtpManagerImplDeliveryTest, PhishGuardLatency_PerRequestMeasurement) {
  base::OnceCallback<void(bool is_phishing)> phish_guard_callback_1;
  base::OnceCallback<void(bool is_phishing)> phish_guard_callback_2;

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(autofill_driver().GetFrameToken(), _))
      .WillOnce([&](LocalFrameToken frame,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback_1 = std::move(callback);
      })
      .WillOnce([&](LocalFrameToken frame,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback_2 = std::move(callback);
      });

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future1;
  RequestOtpSuggestions(future1);

  // Check 1 starts at t=0.
  one_time_tokens::OneTimeToken token1(
      one_time_tokens::OneTimeTokenType::kGmail, "111111",
      base::TimeTicks::Now(), "sender1@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token1));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future2;
  RequestOtpSuggestions(future2);

  // Check 2 starts at t=20ms.
  task_environment_.AdvanceClock(base::Milliseconds(20));
  one_time_tokens::OneTimeToken token2(
      one_time_tokens::OneTimeTokenType::kGmail, "222222",
      base::TimeTicks::Now(), "sender2@example.com");
  test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token2));

  // Check 1 completes at t=50ms (total latency 50ms).
  task_environment_.AdvanceClock(base::Milliseconds(30));
  std::move(phish_guard_callback_1).Run(/*is_phishing=*/false);

  // Check 2 completes at t=80ms (total latency 60ms since t=20ms).
  task_environment_.AdvanceClock(base::Milliseconds(30));
  std::move(phish_guard_callback_2).Run(/*is_phishing=*/false);

  histogram_tester_.ExpectBucketCount(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectBucketCount(kPhishGuardLatencyHistogram, 60, 1);
  histogram_tester_.ExpectTotalCount(kPhishGuardLatencyHistogram, 2);
}

// Tests that if a new suggestion request is started for a different frame
// while PhishGuard checks are in flight, stale safe and phishing verdicts from
// the first frame are ignored and neither resolve nor cancel the new frame's
// pending callback.
TEST_F(OtpManagerImplDeliveryTest,
       MaybeShowOtpSuggestionsForSms_IgnoresStaleFrameVerdictWhenFrameChanges) {
  VerifyIgnoresStaleFrameVerdictWhenFrameChanges(
      one_time_tokens::OneTimeTokenType::kSmsOtp);
}

TEST_F(
    OtpManagerImplDeliveryTest,
    MaybeShowOtpSuggestionsForGmail_IgnoresStalePhishGuardResultFromDifferentFrame) {
  VerifyIgnoresStaleFrameVerdictWhenFrameChanges(
      one_time_tokens::OneTimeTokenType::kGmail);
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "222222");
}

class OtpManagerImplProactiveTriggerTest : public OtpManagerImplTest {
 public:
  void SetUp() override {
    OtpManagerImplTest::SetUp();
    prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                             true);
    otp_manager_.emplace(autofill_manager(), &sms_otp_service_);
  }

  void TearDown() override {
    otp_manager_.reset();
    OtpManagerImplTest::TearDown();
  }

  OtpManagerImpl& otp_manager() LIFETIME_BOUND { return *otp_manager_; }

  void ReceiveGmailOtp(std::string value = kDefaultOtpValue) {
    one_time_tokens::OneTimeToken token(
        one_time_tokens::OneTimeTokenType::kGmail, std::move(value),
        base::TimeTicks::Now(), "sender@example.com");
    test_api(otp_manager()).OnOneTimeTokenReceived(std::move(token));
  }

 private:
  std::optional<OtpManagerImpl> otp_manager_;
};

// Tests that arriving Gmail OTPs proactively trigger suggestions on the
// focused OTP field when no callback is pending.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_FocusedOtpField) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  // Focus the OTP field.
  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);
  EXPECT_EQ(test_api(otp_manager()).currently_focused_field_id(), otp_field_id);

  // When PhishGuard check is performed, approve it (not phishing).
  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  // Verify that suggestions are proactively triggered on the renderer.
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp();

  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(),
            kDefaultOtpValue);
}

// Tests that proactive triggering is suppressed if the user has not opted into
// Gmail OTP filling.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_SuppressedWhenUserNotOptedIn) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  ReceiveGmailOtp();

  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
}

// Tests that arriving Gmail OTPs proactively trigger suggestions on the
// cached OTP field even when no field is currently focused.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_UnfocusedOtpField) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp();

  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(),
            kDefaultOtpValue);
}

// Tests that proactive triggering is suppressed if a non-OTP field is focused.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_SuppressedWhenNonOtpFieldFocused) {
  FormDescription form_description = {
      .fields = {
          {.server_type = NAME_FIRST, .label = u"First name", .name = u"fn"},
          {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
      }};
  const FormStructure* form = AddForm(form_description);
  ASSERT_TRUE(form);
  FieldGlobalId non_otp_field_id = form->field(0)->global_id();

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         non_otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  ReceiveGmailOtp();

  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
}

// Tests that proactive triggering is suppressed if no OTP field exists in
// cached forms.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_SuppressedWhenNoOtpFieldInCachedForms) {
  const FormStructure* form = AddFormWithFirstNameField();
  ASSERT_TRUE(form);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);

  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  ReceiveGmailOtp();

  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
}

// Tests that proactive triggering is suppressed if an OTP field has user-typed
// input.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_SuppressedWhenFieldHasUserInput) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);

  // Add user modifier to simulate typed input.
  test_api(autofill_manager())
      .FindCachedFormById(form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  ReceiveGmailOtp();

  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
}

// Tests that proactive triggering is suppressed if PhishGuard determines the
// site is phishing.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_SuppressedWhenPhishGuardBlocks) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);

  // Simulate phishing detection.
  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  ReceiveGmailOtp();

  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      static_cast<int>(OneTimeTokensPhishGuardVerdict::kPhishing), 1);
}

// Tests that duplicate OTP tokens are ignored, but a newer/different OTP
// triggers suggestions again.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_DeduplicationAndSecondDifferentOtp) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(3)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));

  // First token triggers suggestions.
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp("123456");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "123456");

  // Duplicate token with identical value should NOT trigger suggestions again.
  ReceiveGmailOtp("123456");

  // A different (newer) OTP token SHOULD trigger suggestions again.
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp("654321");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "654321");
}

// Tests that changing focus to another OTP field clears
// `last_triggered_otp_value_` allowing the same OTP value to trigger
// suggestions on the newly focused field.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_FocusChangeClearsLastTriggeredValue) {
  FormDescription form_description = {
      .fields = {
          {.server_type = ONE_TIME_CODE, .label = u"OTP1", .name = u"otp1"},
          {.server_type = ONE_TIME_CODE, .label = u"OTP2", .name = u"otp2"},
      }};
  const FormStructure* form = AddForm(form_description);
  ASSERT_TRUE(form);
  FieldGlobalId field1_id = form->field(0)->global_id();
  FieldGlobalId field2_id = form->field(1)->global_id();

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));

  // Focus field 1 and trigger.
  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         field1_id);

  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          field1_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp("123456");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "123456");

  // Focus field 2. This must clear `last_triggered_otp_value_`.
  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         field2_id);
  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());

  // Same OTP value should now trigger on field 2!
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          field2_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  ReceiveGmailOtp("123456");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "123456");
}

// Tests that blurring a form field (`OnBeforeFocusOnNonFormField`) clears
// `last_triggered_otp_value_`, allowing the same OTP value to proactively
// trigger suggestions again.
TEST_F(OtpManagerImplProactiveTriggerTest,
       ProactiveTriggerSuggestions_BlurClearsLastTriggeredValue) {
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));

  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         otp_field_id);
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));
  ReceiveGmailOtp("123456");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "123456");

  // Blurring the field must clear `last_triggered_otp_value_`.
  otp_manager().OnBeforeFocusOnNonFormField(autofill_manager());
  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());

  // The same OTP value can now trigger suggestions again.
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));
  ReceiveGmailOtp("123456");
  EXPECT_EQ(test_api(otp_manager()).last_triggered_otp_value(), "123456");
}

// Tests that if focus moves to an OTP field in a different frame while a
// PhishGuard check is in flight, the first frame's PhishGuard verdict does not
// trigger suggestions on the second frame.
TEST_F(
    OtpManagerImplProactiveTriggerTest,
    ProactiveTriggerSuggestions_SuppressedWhenFrameChangesDuringPhishGuardCheck) {
  FormDescription form_description = {
      .fields = {
          {.server_type = ONE_TIME_CODE, .label = u"OTP1", .name = u"otp1"},
          {.server_type = ONE_TIME_CODE, .label = u"OTP2", .name = u"otp2"},
      }};
  const FormStructure* form = AddForm(form_description);
  ASSERT_TRUE(form);

  LocalFrameToken frame_a = form->field(0)->host_frame();
  LocalFrameToken frame_b = test::MakeLocalFrameToken();
  test_api(autofill_manager())
      .FindCachedFormById(form->global_id())
      ->field(1)
      ->set_host_frame(frame_b);

  FieldGlobalId field1_id = form->field(0)->global_id();
  FieldGlobalId field2_id = form->field(1)->global_id();

  base::OnceCallback<void(bool is_phishing)> phish_guard_callback_a;
  base::OnceCallback<void(bool is_phishing)> phish_guard_callback_b;

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck(frame_a, _))
      .WillOnce([&](LocalFrameToken,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback_a = std::move(callback);
      });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck(frame_b, _))
      .WillOnce([&](LocalFrameToken,
                    base::OnceCallback<void(bool is_phishing)> callback) {
        phish_guard_callback_b = std::move(callback);
      });

  // Focus field 1 in frame_a and receive an OTP (starts check A).
  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         field1_id);
  ReceiveGmailOtp("111111");

  // Before check A finishes, focus field 2 in frame_b and receive an OTP
  // (starts check B).
  otp_manager().OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                         field2_id);
  ReceiveGmailOtp("222222");

  // Check A completes with safe verdict, but focus is now in frame_b, so it
  // must NOT trigger suggestions on field 2.
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);
  std::move(phish_guard_callback_a).Run(/*is_phishing=*/false);

  // Check B completes with phishing verdict; still no suggestions triggered.
  std::move(phish_guard_callback_b).Run(/*is_phishing=*/true);
  EXPECT_TRUE(test_api(otp_manager()).last_triggered_otp_value().empty());
}

// Tests that GetOtpSuggestions starts GmailOtpRetriever when AffiliationService
// and GmailOtpBackend are present.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_StartsGmailOtpRetrieverWhenAffiliationServicePresent) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that GetOtpSuggestions does not start GmailOtpRetriever when
// AffiliationService is missing.
TEST_F(
    OtpManagerImplTest,
    GetOtpSuggestions_DoesNotStartGmailOtpRetrieverWhenAffiliationServiceMissing) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  autofill_client().set_affiliation_service(nullptr);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that GetOtpSuggestions does not start GmailOtpRetriever when
// GmailOtpBackend is missing.
TEST_F(
    OtpManagerImplTest,
    GetOtpSuggestions_DoesNotStartGmailOtpRetrieverWhenGmailOtpBackendMissing) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(nullptr);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that GetOtpSuggestions does not start GmailOtpRetriever when the user
// is not opted into Gmail OTP filling.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_DoesNotStartGmailOtpRetrieverWhenUserNotOptedIn) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that when a domain-verified cached Gmail token is present,
// GetOtpSuggestions delivers it immediately and does not start
// GmailOtpRetriever even if AffiliationService and GmailOtpBackend are present.
TEST_F(OtpManagerImplTest,
       GetOtpSuggestions_CachedGmailOtpDoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  one_time_tokens::OneTimeToken gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "sender@example.com");

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  test_api(otp_manager).set_last_retrieved_gmail_otp(gmail_otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      future.Get(),
      ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "654321")));
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that OnGmailOtpRetrieved delivers OTP suggestions when retrieval
// succeeds and PhishGuard approves.
TEST_F(OtpManagerImplTest, OnGmailOtpRetrieved_DeliversSuggestionsOnSuccess) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "789012",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      future.Get(),
      ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "789012")));
  EXPECT_TRUE(autofill_manager()
                  .GetOtpFormEventLogger()
                  .HasLoggedDataToFillAvailableForTesting());
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnGmailOtpRetrieved does not invoke the pending callback when
// retrieval fails.
TEST_F(OtpManagerImplTest, OnGmailOtpRetrieved_DoesNotInvokeCallbackOnFailure) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  test_api(otp_manager)
      .OnGmailOtpRetrieved(
          base::unexpected(one_time_tokens::OneTimeTokenRetrievalError::
                               kGmailOtpBackendNetworkError));

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnGmailOtpRetrieved does not invoke the pending callback when
// retrieved token is empty.
TEST_F(OtpManagerImplTest,
       OnGmailOtpRetrieved_DoesNotInvokeCallbackWhenTokenEmpty) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that Gmail OTPs returned by `SmsOtpService` are ignored, even if
// neither `AffiliationService` nor `GmailOtpBackend` is available, because
// Gmail OTPs are only retrieved from the `GmailOtpBackend`.
TEST_F(OtpManagerImplTest, GetRecentOneTimeTokens_IgnoresGmailOtp) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // Cache a Gmail token in the `SmsOtpService` so that it is delivered through
  // `GetRecentOneTimeTokens`.
  sms_otp_service_.SimulateOtpArrived(one_time_tokens::OneTimeToken(
      one_time_tokens::OneTimeTokenType::kGmail, "123456",
      base::TimeTicks::Now(), "sender@example.com"));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  // Delivering a Gmail token through GetRecentOneTimeTokens should be ignored.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, test_field_, future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(autofill_manager()
                   .GetOtpFormEventLogger()
                   .HasLoggedDataToFillAvailableForTesting());
}

using OtpManagerImplOnTickleReceivedTest = OtpManagerImplTest;

// Tests that OnTickleReceived starts GmailOtpRetriever when all conditions are
// met.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       AffiliationServiceAndBackendPresent_StartsGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not restart retrieval if GmailOtpRetriever
// is already in flight.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       GmailOtpRetrieverAlreadyRunning_DoesNotRestart) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  one_time_tokens::GmailOtpRetriever* in_flight_retriever =
      test_api(otp_manager).gmail_otp_retriever();
  ASSERT_NE(in_flight_retriever, nullptr);

  // A second tickle arrives while the first retrieval is still in flight.
  test_api(otp_manager).OnTickleReceived();

  // The in-flight retriever instance must not be clobbered or restarted.
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), in_flight_retriever);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when no OTP
// field was detected on the page.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       OtpFieldNotDetected_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  // Do NOT call SetUpOtpFieldDetector(), so IsOtpFieldDetected() is false.
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived starts GmailOtpRetriever even when no field is
// currently focused, as long as an OTP field is cached.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       NoFieldFocused_StartsGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when a non-OTP
// field is focused.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       NonOtpFieldFocused_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = NAME_FIRST,
               .label = u"First name",
               .name = u"fn"},
              {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
          },
  });
  ASSERT_TRUE(form);
  FieldGlobalId name_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       name_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when the OTP
// field contains typed input.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       FieldHasTypedInput_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(autofill_manager())
      .FindCachedFormById(form->global_id())
      ->field(0)
      ->AddFieldModifier(FieldModifier::kUser);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when the user
// is not opted into Gmail OTP filling.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       UserNotOptedIn_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when
// AffiliationService is missing.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       AffiliationServiceMissing_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  autofill_client().set_affiliation_service(nullptr);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnTickleReceived does not start GmailOtpRetriever when
// GmailOtpBackend is missing.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       GmailOtpBackendMissing_DoesNotStartGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(nullptr);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  test_api(otp_manager).OnTickleReceived();

  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that `OnTickleReceived` passes the target OTP field's frame origin
// (rather than the primary main frame origin) to `GmailOtpRetriever` when the
// OTP field is in a same-TLD+1 subframe.
TEST_F(OtpManagerImplOnTickleReceivedTest,
       SameTldPlusOneSubframe_UsesTargetFieldOriginForGmailOtpRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);

  auto backend = std::make_unique<one_time_tokens::FakeGmailOtpBackend>();
  backend->ProcessCallbacks(one_time_tokens::OneTimeToken(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "otp@sub.example.test"));
  autofill_client().set_gmail_otp_backend(std::move(backend));

  const FormStructure* form = AddFormWithOtpField(
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.test")),
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.test")));
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  test_api(otp_manager).OnTickleReceived();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).last_triggered_otp_value() == "654321";
  }));
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that OnGmailOtpRetrieved proactively triggers suggestions when
// retrieval succeeds and PhishGuard approves.
TEST_F(OtpManagerImplTest,
       OnGmailOtpRetrieved_ProactivelyTriggersSuggestionsOnSuccess) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  test_api(otp_manager).OnGmailOtpRetrieved(CreateSampleGmailOtpResult());

  EXPECT_EQ(test_api(otp_manager).last_triggered_otp_value(), "987654");
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
}

// Tests that OnGmailOtpRetrieved suppresses proactive suggestions when
// PhishGuard classifies the site as phishing.
TEST_F(OtpManagerImplTest, OnGmailOtpRetrieved_SuppressedWhenPhishGuardBlocks) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  test_api(otp_manager).OnGmailOtpRetrieved(CreateSampleGmailOtpResult());

  EXPECT_TRUE(test_api(otp_manager).last_triggered_otp_value().empty());
}

// Tests that OnGmailOtpRetrieved still proactively triggers suggestions on the
// cached OTP field and cleans up the retriever even if the user blurs /
// unfocuses the OTP field while GmailOtpRetriever is in flight.
TEST_F(OtpManagerImplTest,
       InFlightOtpRetrieval_TriggersSuggestionsAndCleansUpWhenFieldBlurred) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  // Tickle arrives, starting GmailOtpRetriever.
  test_api(otp_manager).OnTickleReceived();
  ASSERT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  // User unfocuses the OTP field (focuses a non-form field) while retrieval is
  // in flight.
  otp_manager.OnBeforeFocusOnNonFormField(autofill_manager());
  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  test_api(otp_manager).OnGmailOtpRetrieved(CreateSampleGmailOtpResult());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).last_triggered_otp_value() == "987654" &&
           test_api(otp_manager).gmail_otp_retriever() == nullptr;
  }));
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that OnGmailOtpRetrieved does not trigger suggestions when
// retrieval fails.
TEST_F(OtpManagerImplTest,
       OnGmailOtpRetrieved_DoesNotTriggerSuggestionsOnFailure) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  test_api(otp_manager)
      .OnGmailOtpRetrieved(
          base::unexpected(one_time_tokens::OneTimeTokenRetrievalError::
                               kGmailOtpBackendNetworkError));

  EXPECT_TRUE(test_api(otp_manager).last_triggered_otp_value().empty());
}

// Tests that OnGmailOtpRetrieved does not trigger suggestions when
// retrieved token is empty.
TEST_F(OtpManagerImplTest,
       OnGmailOtpRetrieved_DoesNotTriggerSuggestionsOnEmptyToken) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  test_api(otp_manager).OnGmailOtpRetrieved(CreateSampleGmailOtpResult(""));

  EXPECT_TRUE(test_api(otp_manager).last_triggered_otp_value().empty());
}

// Tests that when an OTP is delivered reactively, subsequent tickle retrieval
// with the same OTP value is suppressed by deduplication.
TEST_F(OtpManagerImplTest,
       ReactiveThenProactiveTickle_DeduplicationSuppressesSubsequentTickle) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  // Expect PhishGuard check on reactive path.
  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  // Simulate reactive delivery of token "555123".
  test_api(otp_manager)
      .OnGmailOtpRetrieved(CreateSampleGmailOtpResult("555123"));

  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), "555123");
  EXPECT_EQ(test_api(otp_manager).last_triggered_otp_value(), "555123");

  // A tickle arrives and retrieves the identical token value. Proactive
  // suggestions must NOT be triggered again.
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  test_api(otp_manager)
      .OnGmailOtpRetrieved(CreateSampleGmailOtpResult("555123"));

  EXPECT_EQ(test_api(otp_manager).last_triggered_otp_value(), "555123");
}

// Tests that when a Gmail OTP retrieval is already in flight (e.g., started
// upon tickle arrival), subsequent reactive focus via GetOtpSuggestions does
// not restart GmailOtpRetriever, and the in-flight retriever delivers
// suggestions to the pending callback upon completion.
TEST_F(OtpManagerImplTest,
       ConcurrentReactiveFocusDuringInFlightTickleRetrieval) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  // Focus the field so tickle arrival can start GmailOtpRetriever.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  // Tickle arrives, starting GmailOtpRetriever.
  test_api(otp_manager).OnTickleReceived();
  one_time_tokens::GmailOtpRetriever* in_flight_retriever =
      test_api(otp_manager).gmail_otp_retriever();
  ASSERT_NE(in_flight_retriever, nullptr);

  // User focus triggers GetOtpSuggestions while retrieval is still in flight.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());

  // Verify that GmailOtpRetriever was NOT re-created or clobbered.
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), in_flight_retriever);
  EXPECT_FALSE(future.IsReady());

  // When the in-flight retriever completes with success and PhishGuard
  // approves, the suggestions callback should be resolved.
  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  test_api(otp_manager)
      .OnGmailOtpRetrieved(CreateSampleGmailOtpResult("123456"));

  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0].value(), "123456");

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).gmail_otp_retriever() == nullptr;
  }));
  autofill_client().set_affiliation_service(nullptr);
}

// Tests the end-to-end proactive tickle flow: when
// kAutofillShowGmailOtpSuggestions is enabled and no token is cached yet,
// GetOtpSuggestions does not return a hardcoded fallback; when a tickle
// subsequently arrives and retrieves a token, RendererShouldTriggerSuggestions
// is called and the renderer's follow-up GetOtpSuggestions call synchronously
// receives the retrieved token.
TEST_F(
    OtpManagerImplTest,
    ProactiveTickle_DeliversRetrievedTokenOnRendererFollowUpGetOtpSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowGmailOtpSuggestions};
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  // Initial focus query when no OTP has arrived yet must not resolve with a
  // hardcoded fallback.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      initial_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                initial_future.GetCallback());
  EXPECT_FALSE(initial_future.IsReady());

  // Simulate initial reactive retrieval finishing with no token and SMS
  // subscription expiring so no pending callback remains when the tickle
  // arrives later.
  test_api(otp_manager)
      .OnGmailOtpRetrieved(
          base::unexpected(one_time_tokens::OneTimeTokenRetrievalError::
                               kGmailOtpBackendNetworkError));
  task_environment_.FastForwardBy(base::Minutes(1));
  ASSERT_TRUE(initial_future.IsReady());
  EXPECT_TRUE(initial_future.Get().empty());

  // Now a Gmail tickle arrives while the OTP field is still focused.
  test_api(otp_manager).OnTickleReceived();
  ASSERT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "654321",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });
  ASSERT_EQ(test_api(otp_manager).last_triggered_otp_value(), "654321");

  // Simulate the renderer's follow-up AskForValuesToFill -> GetOtpSuggestions
  // call triggered by RendererShouldTriggerSuggestions.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      followup_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                followup_future.GetCallback());
  ASSERT_TRUE(followup_future.IsReady());
  EXPECT_THAT(followup_future.Get(),
              ElementsAre(AllOf(
                  Property(&one_time_tokens::OneTimeToken::value, "654321"),
                  Property(&one_time_tokens::OneTimeToken::sender_address,
                           Optional(std::string("sender@example.com"))))));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).gmail_otp_retriever() == nullptr;
  }));
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when a second, newer Gmail OTP arrives via a tickle for the same
// focused OTP field after the first OTP was already shown, suggestions are
// re-triggered and the follow-up GetOtpSuggestions call delivers the newer
// token.
TEST_F(OtpManagerImplTest,
       ProactiveTickle_SecondNewerGmailOtpReTriggersAndDeliversNewerToken) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowGmailOtpSuggestions};
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FieldGlobalId otp_field_id = form->field(0)->global_id();

  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(4)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(/*is_phishing=*/false));

  // 1. First tickle arrives and retrieves "111111".
  test_api(otp_manager).OnTickleReceived();
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));
  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "111111",
              base::TimeTicks::Now(), "first@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });

  // Renderer requests suggestions for the first trigger.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      first_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                first_future.GetCallback());
  ASSERT_TRUE(first_future.IsReady());
  EXPECT_THAT(
      first_future.Get(),
      ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "111111")));

  // 2. Second tickle arrives later and retrieves a newer OTP "222222".
  task_environment_.FastForwardBy(base::Seconds(5));
  test_api(otp_manager).OnTickleReceived();
  ASSERT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));
  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "222222",
              base::TimeTicks::Now(), "second@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });
  ASSERT_EQ(test_api(otp_manager).last_triggered_otp_value(), "222222");

  // Renderer requests suggestions for the second trigger and receives "222222".
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      second_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                second_future.GetCallback());
  ASSERT_TRUE(second_future.IsReady());
  EXPECT_THAT(second_future.Get(),
              ElementsAre(AllOf(
                  Property(&one_time_tokens::OneTimeToken::value, "222222"),
                  Property(&one_time_tokens::OneTimeToken::sender_address,
                           Optional(std::string("second@example.com"))))));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).gmail_otp_retriever() == nullptr;
  }));
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when a cached Gmail OTP is already present when an OTP form is
// detected via OnFieldTypesDetermined, GetRecentOtpsAndRenewSubscription()
// evaluates and triggers the cached token without starting GmailOtpRetriever.
TEST_F(
    OtpManagerImplTest,
    OnFieldTypesDetermined_CachedGmailOtpTriggersSuggestionsWithoutStartingRetriever) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  one_time_tokens::OneTimeToken gmail_otp(
      one_time_tokens::OneTimeTokenType::kGmail, "654321",
      base::TimeTicks::Now(), "sender@example.com");
  test_api(otp_manager).set_last_retrieved_gmail_otp(gmail_otp);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          _, AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable));

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when AffiliationService and GmailOtpBackend are present,
// unverified Gmail tokens cached by the `GmailOtpBackend` are ignored by
// SelectMostRecentToken() and GetOtpSuggestions(), preventing cross-origin
// leakage of domain-mismatched tokens.
TEST_F(
    OtpManagerImplTest,
    SelectMostRecentToken_IgnoresUnverifiedBackendGmailOtpWhenAffiliationPresent) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);

  auto backend = std::make_unique<one_time_tokens::FakeGmailOtpBackend>();
  backend->ProcessCallbacks(one_time_tokens::OneTimeToken(
      one_time_tokens::OneTimeTokenType::kGmail, "999888",
      base::TimeTicks::Now(), "otp@bank.test"));
  autofill_client().set_gmail_otp_backend(std::move(backend));

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);

  // Unverified token in backend cache must not be returned by
  // SelectMostRecentToken or suppress starting GmailOtpRetriever.
  EXPECT_FALSE(test_api(otp_manager).SelectMostRecentToken().has_value());
  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when OtpPhishGuardDelegate completes asynchronously and the SMS
// backend is null (as on Desktop), GetOtpSuggestions() delivers the cached
// Gmail OTP rather than having its callback stolen by the synchronous
// kSmsOtpBackendPlatformNotSupported error.
TEST_F(
    OtpManagerImplTest,
    ProactiveTickle_AsyncPhishGuardWithNullSmsBackendDeliversCachedGmailOtp) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  // Simulate a platform without SMS OTP support (as on Desktop).
  sms_otp_service_.set_platform_supported(false);
  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);

  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_EQ(form->field_count(), 1u);
  FieldGlobalId otp_field_id = form->field(0)->global_id();
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .Times(2)
      .WillRepeatedly([](LocalFrameToken,
                         base::OnceCallback<void(bool is_phishing)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      });
  base::test::TestFuture<void> trigger_future;
  EXPECT_CALL(
      autofill_driver(),
      RendererShouldTriggerSuggestions(
          otp_field_id,
          AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable))
      .WillOnce(base::test::RunOnceClosure(trigger_future.GetCallback()));

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "654321",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });
  ASSERT_TRUE(trigger_future.Wait());
  EXPECT_EQ(test_api(otp_manager).last_triggered_otp_value(), "654321");

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>>
      followup_future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0),
                                followup_future.GetCallback());

  EXPECT_THAT(
      followup_future.Get(),
      ElementsAre(Property(&one_time_tokens::OneTimeToken::value, "654321")));

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when PhishGuard returns a phishing verdict, the cached Gmail OTP
// in `last_retrieved_gmail_otp_` is cleared so it does not remain cached in
// OtpManagerImpl on a confirmed phishing site.
TEST_F(OtpManagerImplTest,
       OnGmailOtpRetrieved_PhishGuardPhishingVerdictClearsCachedGmailOtp) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       form->field(0)->global_id());
  test_api(otp_manager)
      .set_last_retrieved_gmail_otp(one_time_tokens::OneTimeToken(
          one_time_tokens::OneTimeTokenType::kGmail, "111111",
          base::TimeTicks::Now(), "cached@example.com"));
  ASSERT_TRUE(test_api(otp_manager).SelectMostRecentToken().has_value());

  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(0)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "654321",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !test_api(otp_manager).SelectMostRecentToken().has_value();
  }));

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that a Gmail OTP cached in `last_retrieved_gmail_otp_` expires after
// `kCacheDurationForOldTokens` (3 minutes), causing `SelectMostRecentToken()`
// to return `std::nullopt` and allowing `GetOtpSuggestions()` to start a fresh
// `GmailOtpRetriever`.
TEST_F(
    OtpManagerImplTest,
    SelectMostRecentToken_ExpiredLastRetrievedGmailOtpIsIgnoredAndAllowsNewRetrieval) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  ASSERT_GT(form->field_count(), 0u);

  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));
  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "654321",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });
  ASSERT_TRUE(test_api(otp_manager).SelectMostRecentToken().has_value());
  ASSERT_FALSE(test_api(otp_manager).gmail_otp_retriever());

  // Fast-forward past `kCacheDurationForOldTokens` so the cached Gmail OTP
  // expires.
  task_environment_.FastForwardBy(one_time_tokens::kCacheDurationForOldTokens +
                                  base::Seconds(1));
  EXPECT_FALSE(test_api(otp_manager).SelectMostRecentToken().has_value());

  // Requesting suggestions now should start a fresh `GmailOtpRetriever` instead
  // of returning the expired token.
  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(0), future.GetCallback());
  EXPECT_TRUE(test_api(otp_manager).gmail_otp_retriever());

  autofill_client().set_affiliation_service(nullptr);
}

// Tests that enabling `kAutofillGmailOtpFillingEnabled` (e.g. when the user
// accepts the opt-in bubble) immediately starts `GmailOtpRetriever` on a page
// with an unedited OTP field.
TEST_F(OtpManagerImplTest,
       PrefChangeToEnabled_StartsGmailOtpRetrieverWhenOtpFieldPresent) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);

  // Enabling the pref reactive-fetches the Gmail OTP.
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  EXPECT_NE(test_api(otp_manager).gmail_otp_retriever(), nullptr);
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that enabling `kAutofillGmailOtpFillingEnabled` does not start
// `GmailOtpRetriever` if the OTP field already contains user-typed input.
TEST_F(OtpManagerImplTest,
       PrefChangeToEnabled_DoesNotStartRetrieverWhenOtpFieldHasTypedInput) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddFormWithOtpField();
  ASSERT_TRUE(form);
  FormStructure* cached_form =
      test_api(autofill_manager()).FindCachedFormById(form->global_id());
  ASSERT_TRUE(cached_form);
  ASSERT_GT(cached_form->field_count(), 0u);
  cached_form->field(0)->AddFieldModifier(FieldModifier::kUser);

  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  EXPECT_EQ(test_api(otp_manager).gmail_otp_retriever(), nullptr);
  autofill_client().set_affiliation_service(nullptr);
}

// Tests that when `OnGmailOtpRetrieved` completes while a non-OTP field is
// focused (so `GetTargetOtpField()` returns `nullptr` and proactive suggestions
// are suppressed), the domain-verified Gmail OTP is still cached in
// `last_retrieved_gmail_otp_` and is delivered (after PhishGuard verification)
// when the user subsequently focuses the OTP field and requests suggestions.
TEST_F(
    OtpManagerImplTest,
    OnGmailOtpRetrieved_CachesTokenWhenNonOtpFieldFocusedAndDeliversOnLaterOtpFieldFocus) {
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  SetUpOtpFieldDetector();
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service;
  autofill_client().set_affiliation_service(&mock_affiliation_service);
  autofill_client().set_gmail_otp_backend(
      std::make_unique<one_time_tokens::FakeGmailOtpBackend>());

  OtpManagerImpl otp_manager(autofill_manager(), &sms_otp_service_);
  const FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = NAME_FIRST,
               .label = u"First name",
               .name = u"fn"},
              {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
          },
  });
  ASSERT_TRUE(form);
  ASSERT_EQ(form->field_count(), 2u);
  FieldGlobalId non_otp_field_id = form->field(0)->global_id();
  FieldGlobalId otp_field_id = form->field(1)->global_id();

  // While the initial `GmailOtpRetriever` started by `OnFieldTypesDetermined`
  // is in flight, the user focuses the non-OTP field.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       non_otp_field_id);

  // When `OnGmailOtpRetrieved` finishes while the non-OTP field is focused,
  // proactive suggestions must not be triggered, but the retrieved token must
  // be cached in `last_retrieved_gmail_otp_`.
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck).Times(0);
  EXPECT_CALL(autofill_driver(), RendererShouldTriggerSuggestions).Times(0);

  test_api(otp_manager)
      .OnGmailOtpRetrieved(one_time_tokens::GmailOtpRetriever::Result{
          .token = one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kGmail, "654321",
              base::TimeTicks::Now(), "sender@example.com"),
          .source = one_time_tokens::GmailOtpRetriever::Source::kReceived,
      });

  ASSERT_TRUE(test_api(otp_manager).SelectMostRecentToken().has_value());
  EXPECT_EQ(test_api(otp_manager).SelectMostRecentToken()->value(), "654321");

  // Later, the user focuses the OTP field and requests suggestions. The cached
  // token is verified via PhishGuard and delivered without starting a new
  // `GmailOtpRetriever`.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), form->global_id(),
                                       otp_field_id);
  EXPECT_CALL(otp_phish_guard_delegate(),
              StartOtpPhishGuardCheck(form->field(1)->host_frame(), _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<std::vector<one_time_tokens::OneTimeToken>> future;
  otp_manager.GetOtpSuggestions(*form, *form->field(1), future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(),
              ElementsAre(AllOf(
                  Property(&one_time_tokens::OneTimeToken::value, "654321"),
                  Property(&one_time_tokens::OneTimeToken::sender_address,
                           Optional(std::string("sender@example.com"))))));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_api(otp_manager).gmail_otp_retriever() == nullptr;
  }));

  autofill_client().set_affiliation_service(nullptr);
}

}  // namespace autofill
