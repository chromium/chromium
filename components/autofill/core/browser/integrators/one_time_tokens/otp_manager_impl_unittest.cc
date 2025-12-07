// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::test::FormDescription;
using autofill::test::GetServerTypes;
using base::test::RunOnceCallback;
using one_time_tokens::OneTimeTokenServiceImpl;
using ::testing::_;
namespace autofill {

namespace {

constexpr char kDefaultOtpValue[] = "123456";

constexpr char kPhishGuardCheckPerformedHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.CheckPerformed";
constexpr char kPhishGuardLatencyHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.Latency";
constexpr char kPhishGuardVerdictHistogram[] =
    "Autofill.OneTimeTokens.PhishGuard.Verdict";

class MockSmsOtpBackend : public one_time_tokens::SmsOtpBackend {
 public:
  MOCK_METHOD(void,
              RetrieveSmsOtp,
              (base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>),
              (override));
};

class MockOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  MOCK_METHOD(void,
              StartOtpPhishGuardCheck,
              (const GURL&, base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace

class OtpManagerImplTest : public testing::Test,
                           public WithTestAutofillClientDriverManager<> {
 public:
  OtpManagerImplTest() = default;
  ~OtpManagerImplTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    auto otp_phish_guard_delegate =
        std::make_unique<MockOtpPhishGuardDelegate>();
    autofill_client().set_otp_phish_guard_delegate(
        std::move(otp_phish_guard_delegate));
    CreateAutofillDriver();
  }

  void AddForm(const FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));
    test_api(autofill_manager()).OnFormsParsed({form});

    // This would typically happen during parsing but is skipped if a form is
    // injected via the test API.
    autofill_manager().NotifyObservers(
        &TestBrowserAutofillManager::Observer::OnFieldTypesDetermined,
        form.global_id(),
        TestBrowserAutofillManager::Observer::FieldTypeSource::
            kAutofillAiModel);
  }

  one_time_tokens::OtpFetchReply GetDefaultOtpFetchReply() {
    return one_time_tokens::OtpFetchReply(
        one_time_tokens::OneTimeToken(
            one_time_tokens::OneTimeTokenType::kSmsOtp, kDefaultOtpValue,
            base::Time::Now()),
        /*request_complete=*/true);
  }

  void AddFormWithOtpField() {
    FormDescription form_description = {
        .fields = {
            {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp"},
        }};
    AddForm(form_description);
  }

  void AddFormWithFirstNameField() {
    FormDescription form_description = {
        .fields = {
            {.server_type = NAME_FIRST, .label = u"First name", .name = u"fn"},
        }};
    AddForm(form_description);
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
  OneTimeTokenServiceImpl one_time_token_service_{&sms_otp_backend_};
  base::HistogramTester histogram_tester_;
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

// Tests that `GetOtpSuggestions` triggers an OTP retrieval from the
// `SmsOtpBackend` the first time it is called, and that the results are
// correctly passed to the callback.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_TriggersFirstRetrieval) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(reply));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], reply.otp_value->value());
}

// Tests that `GetOtpSuggestions` waits with the callback if an SMS OTP
// retrieval is in progress.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_DoesNotTriggerWhileInProgress) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(reply);

  // The future should now be ready, and contain the OTP.
  EXPECT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], reply.otp_value->value());
}

// Tests that `GetOtpSuggestions` immediately returns any OTPs that have
// already been fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FetchesSmsOnlyOnce) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(GetDefaultOtpFetchReply()));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false))
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(future1.GetCallback());

  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0], reply.otp_value->value());

  // Adding a second OTP form should not trigger a new SMS OTP retrieval.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp).Times(0);
  AddFormWithOtpField();

  // The results of the first result should still be delivered.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(future2.GetCallback());

  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0], reply.otp_value->value());
}

// Tests that if `GetOtpSuggestions` is called twice, only the callback from
// the second call is run when OTPs are fetched.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NewCallInvalidatesOldCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(future1.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future1.IsReady());

  // Call GetOtpSuggestions again. This should invalidate the first callback.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(future2.GetCallback());

  // The first future should still not be ready.
  EXPECT_FALSE(future1.IsReady());
  // The second future should also not be ready.
  EXPECT_FALSE(future2.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(reply);

  // The first future should still not be ready (it was invalidated).
  EXPECT_FALSE(future1.IsReady());
  // The second future should now be ready, and contain the OTP.
  EXPECT_TRUE(future2.IsReady());
  ASSERT_EQ(future2.Get().size(), 1u);
  EXPECT_EQ(future2.Get()[0], reply.otp_value->value());
}

// Tests that an empty OTP value received from the backend is not stored.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_EmptyOtpIsNotStored) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare a reply with an empty OTP.
  one_time_tokens::OtpFetchReply reply = one_time_tokens::OtpFetchReply(
      one_time_tokens::OneTimeToken(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    "", base::Time::Now()),
      /*request_complete=*/true);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(reply));
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that `GetOtpSuggestions` filters out expired OTPs.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_FiltersExpiredOtps) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the reply from the SMS backend.
  one_time_tokens::OtpFetchReply reply = one_time_tokens::OtpFetchReply(
      one_time_tokens::OneTimeToken(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    kDefaultOtpValue,
                                    task_environment_.GetMockClock()->Now()),
      /*request_complete=*/true);
  base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
                  callback) { sms_backend_callback = std::move(callback); });
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce(RunOnceCallback<1>(false));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  // Request suggestions. The future should not be ready yet, as the SMS
  // backend has not responded.
  base::test::TestFuture<const std::vector<std::string>> future1;
  otp_manager.GetOtpSuggestions(future1.GetCallback());
  EXPECT_FALSE(future1.IsReady());

  // Now, let the SMS backend respond.
  std::move(sms_backend_callback).Run(reply);

  // The future should now be ready, and contain the fresh OTP.
  ASSERT_EQ(future1.Get().size(), 1u);
  EXPECT_EQ(future1.Get()[0], reply.otp_value->value());

  // Advance the clock by 6 minutes to make the OTP expire.
  task_environment_.AdvanceClock(base::Minutes(6));

  // Verify that the OTP is now expired and not returned.
  base::test::TestFuture<const std::vector<std::string>> future2;
  otp_manager.GetOtpSuggestions(future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

// Tests that no suggestions are returned if the phishing check returns true.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_PhishingCheckReturnsTrue) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(reply));
  base::OnceCallback<void(bool)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](const GURL&, base::OnceCallback<void(bool)> callback) {
        phish_guard_callback = std::move(callback);
      });

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  // The phish guard check is in progress, so the future should not be ready.
  EXPECT_FALSE(future.IsReady());

  // Simulate a 50ms latency in the phishing check.
  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(phish_guard_callback).Run(true);  // Phishing detected

  EXPECT_TRUE(future.Get().empty());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kPhishing*/ 1, 1);
}

// Tests that suggestions are returned if the phishing check returns false.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_PhishingCheckReturnsFalse) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(reply));
  base::OnceCallback<void(bool)> phish_guard_callback;
  EXPECT_CALL(otp_phish_guard_delegate(), StartOtpPhishGuardCheck)
      .WillOnce([&](const GURL&, base::OnceCallback<void(bool)> callback) {
        phish_guard_callback = std::move(callback);
      });

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  // The phish guard check is in progress, so the future should not be ready.
  EXPECT_FALSE(future.IsReady());

  // Simulate a 50ms latency in the phishing check.
  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(phish_guard_callback).Run(false);  // No phishing

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], reply.otp_value->value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPhishGuardLatencyHistogram, 50, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kNotPhishing*/ 2, 1);
}

// Tests that suggestions are returned if there is no phishing check delegate,
// and that the verdict is logged as kUnknown.
TEST_F(OtpManagerImplTest, GetOtpSuggestions_NoPhishingDelegate) {
  autofill_client().set_otp_phish_guard_delegate(nullptr);
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(RunOnceCallback<0>(reply));

  // Observing an OTP field is supposed to trigger an SMS OTP request.
  AddFormWithOtpField();

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  ASSERT_EQ(future.Get().size(), 1u);
  EXPECT_EQ(future.Get()[0], reply.otp_value->value());

  histogram_tester_.ExpectUniqueSample(kPhishGuardCheckPerformedHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      kPhishGuardVerdictHistogram,
      /*OneTimeTokensPhishGuardVerdict::kUnknown*/ 0, 1);
}

// Tests that `OnBeforeFocusOnFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
                  callback) { sms_backend_callback = std::move(callback); });
  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a form field. This should clear the pending callback.
  otp_manager.OnBeforeFocusOnFormField(autofill_manager(), FormGlobalId(),
                                       FieldGlobalId());

  // The future should now be ready, and contain an empty vector.
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());

  // Now, let the SMS backend respond. This should not affect the already run
  // callback.
  std::move(sms_backend_callback).Run(reply);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that `OnBeforeFocusOnNonFormField` clears the pending callback for
// `GetOtpSuggestions`.
TEST_F(OtpManagerImplTest, OnBeforeFocusOnNonFormField_ClearsPendingCallback) {
  OtpManagerImpl otp_manager(autofill_manager(), &one_time_token_service_);

  // Prepare the handling of SMS requests from the SMS backend.
  one_time_tokens::OtpFetchReply reply = GetDefaultOtpFetchReply();
  base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
      sms_backend_callback;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&](base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>
                  callback) { sms_backend_callback = std::move(callback); });

  base::test::TestFuture<const std::vector<std::string>> future;
  otp_manager.GetOtpSuggestions(future.GetCallback());

  // The future should not be ready yet, as the SMS backend has not responded.
  EXPECT_FALSE(future.IsReady());

  // Simulate a focus on a non-form field. This should clear the pending
  // callback.
  otp_manager.OnBeforeFocusOnNonFormField(autofill_manager());

  // The future should now be ready, and contain an empty vector.
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());

  // Now, let the SMS backend respond. This should not affect the already run
  // callback.
  std::move(sms_backend_callback).Run(reply);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace autofill
