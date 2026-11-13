// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_security_gateway.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_gateway.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenType;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Test;

class RecordingLogReceiver : public LogReceiver {
 public:
  void LogEntry(const base::DictValue& entry) override {
    entries_.push_back(TextLogReceiver().LogEntryToText(entry));
  }

  const std::vector<std::string>& entries() const { return entries_; }

 private:
  std::vector<std::string> entries_;
};

class MockOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  MOCK_METHOD(void,
              StartOtpPhishGuardCheck,
              (LocalFrameToken, base::OnceCallback<void(bool is_phishing)>),
              (override));
};

class SecurityGatewayTestAutofillClient : public TestAutofillClient {
 public:
  bool DocumentUsedWebOTP() override { return document_used_web_otp_; }

  test::AutofillTestingPrefService* GetPrefs() override {
    return return_null_prefs_ ? nullptr : TestAutofillClient::GetPrefs();
  }

  void set_document_used_web_otp(bool used) { document_used_web_otp_ = used; }
  void set_return_null_prefs(bool return_null_prefs) {
    return_null_prefs_ = return_null_prefs;
  }

 private:
  bool document_used_web_otp_ = false;
  bool return_null_prefs_ = false;
};

class OtpSecurityGatewayTest : public Test {
 public:
  void SetUp() override {
    client_.log_router().RegisterReceiver(&log_receiver_);
    auto delegate = std::make_unique<MockOtpPhishGuardDelegate>();
    phish_guard_delegate_ = delegate.get();
    client_.set_otp_phish_guard_delegate(std::move(delegate));
  }

  void TearDown() override {
    phish_guard_delegate_ = nullptr;
    client_.log_router().UnregisterReceiver(&log_receiver_);
  }

 protected:
  OtpTarget MakeTarget(const url::Origin& origin, LocalFrameToken frame) {
    return OtpTarget(origin, frame, test::MakeFormGlobalId(),
                     test::MakeFieldGlobalId(), frame);
  }

  OtpCandidate MakeSmsCandidate(LocalFrameToken frame,
                                const char* value = "123456") {
    return OtpCandidate(
        OneTimeToken(OneTimeTokenType::kSmsOtp, value, base::TimeTicks::Now()),
        url::Origin(), frame);
  }

  OtpCandidate MakeGmailCandidate(const url::Origin& verified_origin,
                                  LocalFrameToken frame,
                                  const char* value = "654321") {
    return OtpCandidate(OneTimeToken(OneTimeTokenType::kGmail, value,
                                     base::TimeTicks::Now(), "a@example.test"),
                        verified_origin, frame);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  SecurityGatewayTestAutofillClient client_;
  RecordingLogReceiver log_receiver_;
  raw_ptr<MockOtpPhishGuardDelegate> phish_guard_delegate_ = nullptr;
};

// Verifies that `ValidateDelivery` allows a safe SMS OTP candidate, stamps
// `verified_origin` and `verified_frame`, and records PhishGuard histograms.
TEST_F(OtpSecurityGatewayTest, ValidateDelivery_AllowsSafeSmsAndRecordsMetric) {
  base::HistogramTester histogram_tester;
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate = MakeSmsCandidate(LocalFrameToken());

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      future;
  gateway.ValidateDelivery(target, candidate, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<0>(), target);
  EXPECT_EQ(future.Get<1>().identity, candidate.identity);
  EXPECT_EQ(future.Get<1>().verified_origin, origin);
  EXPECT_EQ(future.Get<1>().verified_frame, frame);
  EXPECT_EQ(future.Get<2>(), (OtpDeliveryPolicy::Decision{
                                 .allowed = true,
                                 .reason = OtpDeliveryPolicy::SkipReason::kNone,
                             }));
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kVerdictHistogram,
      OneTimeTokensPhishGuardVerdict::kNotPhishing, 1);
}

// Verifies that `ValidateDelivery` rejects phishing verdicts, clears
// `verified_frame`, logs formatted suppression details, and re-queries
// PhishGuard on repeat validations for the same frame.
TEST_F(OtpSecurityGatewayTest,
       ValidateDelivery_RequeriesPhishGuardPerCandidateAndBlocksPhishing) {
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate_1 = MakeSmsCandidate(frame, "123456");
  OtpCandidate candidate_2 = MakeSmsCandidate(frame, "654321");

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      first;
  gateway.ValidateDelivery(target, candidate_1, first.GetCallback());
  ASSERT_TRUE(first.Wait());
  EXPECT_TRUE(first.Get<2>().allowed);
  EXPECT_EQ(first.Get<1>().verified_frame, frame);

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      second;
  gateway.ValidateDelivery(target, candidate_2, second.GetCallback());
  ASSERT_TRUE(second.Wait());
  EXPECT_FALSE(second.Get<2>().allowed);
  EXPECT_TRUE(second.Get<1>().verified_frame.is_empty());
  EXPECT_EQ(second.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kPhishingVerdict);
  EXPECT_THAT(log_receiver_.entries(),
              Contains(HasSubstr("Autofill suggestion(s) suppressed. Reason: "
                                 "PhishGuard verdict is phishing.")));
}

// Verifies that `ValidateDelivery` allows delivery with `kUnknown` PhishGuard
// verdict when no `OtpPhishGuardDelegate` is attached.
TEST_F(OtpSecurityGatewayTest,
       ValidateDelivery_AllowsWhenPhishGuardDelegateMissing) {
  phish_guard_delegate_ = nullptr;
  client_.set_otp_phish_guard_delegate(nullptr);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      future;
  gateway.ValidateDelivery(MakeTarget(origin, frame), MakeSmsCandidate(frame),
                           future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<2>().allowed);
}

// Verifies that `ValidateDelivery` fast-fails SMS candidates when WebOTP was
// already used without starting an asynchronous PhishGuard check, and also
// suppresses delivery if WebOTP becomes used while PhishGuard is in flight.
TEST_F(OtpSecurityGatewayTest,
       ValidateDelivery_SmsEnforcesWebOtpBeforeAndAfterPhishGuard) {
  OtpSecurityGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);

  // Case 1: WebOTP already used before `ValidateDelivery` -> skips PhishGuard.
  client_.set_document_used_web_otp(true);
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      precheck_blocked;
  gateway.ValidateDelivery(target, MakeSmsCandidate(frame),
                           precheck_blocked.GetCallback());
  ASSERT_TRUE(precheck_blocked.Wait());
  EXPECT_FALSE(precheck_blocked.Get<2>().allowed);
  EXPECT_EQ(precheck_blocked.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kWebOtpBlocked);

  // Case 2: WebOTP becomes used while PhishGuard check is in flight.
  client_.set_document_used_web_otp(false);
  base::OnceCallback<void(bool)> phish_guard_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        phish_guard_cb = std::move(cb);
      });

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      inflight_blocked;
  gateway.ValidateDelivery(target, MakeSmsCandidate(frame),
                           inflight_blocked.GetCallback());
  ASSERT_TRUE(phish_guard_cb);

  client_.set_document_used_web_otp(true);
  std::move(phish_guard_cb).Run(/*is_phishing=*/false);

  ASSERT_TRUE(inflight_blocked.Wait());
  EXPECT_FALSE(inflight_blocked.Get<2>().allowed);
  EXPECT_EQ(inflight_blocked.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kWebOtpBlocked);
}

// Verifies Gmail happy path, phishing rejection, and null `GetPrefs()`
// fail-closed handling in `ValidateDelivery`.
TEST_F(
    OtpSecurityGatewayTest,
    ValidateDelivery_GmailAllowsSafeBlocksPhishingAndFailsClosedOnNullPrefs) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate = MakeGmailCandidate(origin, LocalFrameToken());

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      allowed_future;
  gateway.ValidateDelivery(target, candidate, allowed_future.GetCallback());
  ASSERT_TRUE(allowed_future.Wait());
  EXPECT_EQ(allowed_future.Get<0>(), target);
  EXPECT_EQ(allowed_future.Get<1>().identity, candidate.identity);
  EXPECT_EQ(allowed_future.Get<1>().verified_origin, origin);
  EXPECT_EQ(allowed_future.Get<1>().verified_frame, frame);
  EXPECT_EQ(allowed_future.Get<2>(),
            (OtpDeliveryPolicy::Decision{
                .allowed = true,
                .reason = OtpDeliveryPolicy::SkipReason::kNone,
            }));

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      phishing_future;
  gateway.ValidateDelivery(target, candidate, phishing_future.GetCallback());
  ASSERT_TRUE(phishing_future.Wait());
  EXPECT_EQ(phishing_future.Get<2>(),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kPhishingVerdict,
            }));

  client_.set_return_null_prefs(true);
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);
  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      null_prefs_future;
  gateway.ValidateDelivery(target, candidate, null_prefs_future.GetCallback());
  ASSERT_TRUE(null_prefs_future.Wait());
  EXPECT_EQ(null_prefs_future.Get<2>(),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kUserNotOptedIn,
            }));
}

// Verifies that Gmail opt-out fast-fails before PhishGuard and also suppresses
// delivery if the user opts out while PhishGuard is in flight.
TEST_F(OtpSecurityGatewayTest,
       ValidateDelivery_GmailEnforcesConsentBeforeAndAfterPhishGuard) {
  OtpSecurityGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate = MakeGmailCandidate(origin, frame);

  // Case 1: User has not opted in -> skips PhishGuard.
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      not_opted_in;
  gateway.ValidateDelivery(target, candidate, not_opted_in.GetCallback());
  ASSERT_TRUE(not_opted_in.Wait());
  EXPECT_FALSE(not_opted_in.Get<2>().allowed);
  EXPECT_EQ(not_opted_in.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kUserNotOptedIn);

  // Case 2: User opts out while PhishGuard is in flight.
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  base::OnceCallback<void(bool)> phish_guard_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        phish_guard_cb = std::move(cb);
      });

  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      opted_out_inflight;
  gateway.ValidateDelivery(target, candidate, opted_out_inflight.GetCallback());
  ASSERT_TRUE(phish_guard_cb);

  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  std::move(phish_guard_cb).Run(/*is_phishing=*/false);

  ASSERT_TRUE(opted_out_inflight.Wait());
  EXPECT_FALSE(opted_out_inflight.Get<2>().allowed);
  EXPECT_EQ(opted_out_inflight.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kUserNotOptedIn);
}

// Verifies that `ValidateDelivery` fast-fails Gmail OTP candidates when
// `verified_origin` is cross-origin, opaque, sandboxed with precursor origin,
// or shares an opaque origin nonce with `target.origin`, without invoking
// PhishGuard.
TEST_F(
    OtpSecurityGatewayTest,
    ValidateDelivery_GmailRejectsMismatchedOrOpaqueOriginsWithoutPhishGuard) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.test"));
  const url::Origin shared_opaque_origin;
  const url::Origin sandboxed_opaque_origin = origin.DeriveNewOpaqueOrigin();

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  // Cross-origin non-opaque mismatch.
  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      cross_origin_future;
  gateway.ValidateDelivery(MakeTarget(origin, frame),
                           MakeGmailCandidate(other_origin, frame),
                           cross_origin_future.GetCallback());
  ASSERT_TRUE(cross_origin_future.Wait());
  EXPECT_FALSE(cross_origin_future.Get<2>().allowed);
  EXPECT_EQ(cross_origin_future.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kOriginMismatch);

  // Opaque `verified_origin` against non-opaque `target.origin`.
  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      opaque_candidate_future;
  gateway.ValidateDelivery(MakeTarget(origin, frame),
                           MakeGmailCandidate(url::Origin(), frame),
                           opaque_candidate_future.GetCallback());
  ASSERT_TRUE(opaque_candidate_future.Wait());
  EXPECT_FALSE(opaque_candidate_future.Get<2>().allowed);
  EXPECT_EQ(opaque_candidate_future.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kOriginMismatch);

  // Matching opaque origin nonce (`candidate.verified_origin ==
  // target.origin`).
  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      matching_opaque_future;
  gateway.ValidateDelivery(MakeTarget(shared_opaque_origin, frame),
                           MakeGmailCandidate(shared_opaque_origin, frame),
                           matching_opaque_future.GetCallback());
  ASSERT_TRUE(matching_opaque_future.Wait());
  EXPECT_FALSE(matching_opaque_future.Get<2>().allowed);
  EXPECT_EQ(matching_opaque_future.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kOriginMismatch);

  // Sandboxed opaque `target.origin` derived from `origin` as precursor.
  base::test::TestFuture<OtpTarget, OtpCandidate, OtpDeliveryPolicy::Decision>
      sandboxed_future;
  gateway.ValidateDelivery(MakeTarget(sandboxed_opaque_origin, frame),
                           MakeGmailCandidate(origin, frame),
                           sandboxed_future.GetCallback());
  ASSERT_TRUE(sandboxed_future.Wait());
  EXPECT_FALSE(sandboxed_future.Get<2>().allowed);
  EXPECT_EQ(sandboxed_future.Get<2>().reason,
            OtpDeliveryPolicy::SkipReason::kOriginMismatch);
}

// Verifies that destroying `OtpSecurityGateway` while PhishGuard is in flight
// safely cancels completion without invoking `ValidationCallback`.
TEST_F(OtpSecurityGatewayTest,
       ValidateDelivery_GatewayDestroyedWhilePhishGuardInFlightCancelsCleanly) {
  auto gateway = std::make_unique<OtpSecurityGateway>(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));

  base::OnceCallback<void(bool)> phish_guard_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        phish_guard_cb = std::move(cb);
      });

  base::MockCallback<OtpSecurityGateway::ValidationCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  gateway->ValidateDelivery(MakeTarget(origin, frame), MakeSmsCandidate(frame),
                            callback.Get());
  ASSERT_TRUE(phish_guard_cb);

  gateway.reset();
  std::move(phish_guard_cb).Run(/*is_phishing=*/false);
}

// Verifies that `ProactiveTriggerContext` defaults to fail-closed values and
// allows proactive triggering only when required UX and policy preconditions
// are met without invoking PhishGuard.
TEST_F(OtpSecurityGatewayTest,
       EvaluateProactiveTrigger_DefaultContextFailsClosedAndValidAllows) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate = MakeGmailCandidate(origin, frame);

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  const OtpDeliveryPolicy::Decision default_decision =
      gateway.EvaluateProactiveTrigger(
          target, candidate, OtpSecurityGateway::ProactiveTriggerContext{});
  EXPECT_FALSE(default_decision.allowed);

  const OtpSecurityGateway::ProactiveTriggerContext valid_context{
      .target_field_exists = true,
      .frame_active = true,
      .is_otp_field_focused = true,
      .is_keyboard_visible = true,
      .has_non_otp_focus_anywhere = false,
      .any_otp_field_has_typed_input = false,
  };
  EXPECT_TRUE(gateway.EvaluateProactiveTrigger(target, candidate, valid_context)
                  .allowed);

  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  const OtpDeliveryPolicy::Decision opted_out_decision =
      gateway.EvaluateProactiveTrigger(target, candidate, valid_context);
  EXPECT_FALSE(opted_out_decision.allowed);
  EXPECT_EQ(opted_out_decision.reason,
            OtpDeliveryPolicy::SkipReason::kUserNotOptedIn);
}

// Verifies platform-specific focus and keyboard-visibility rules (`kIsAndroid`)
// in `EvaluateProactiveTrigger`.
TEST_F(OtpSecurityGatewayTest,
       EvaluateProactiveTrigger_EnforcesPlatformFocusAndKeyboardRules) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  OtpTarget target = MakeTarget(origin, frame);
  OtpCandidate candidate = MakeGmailCandidate(origin, frame);

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  OtpSecurityGateway::ProactiveTriggerContext unfocused_context{
      .target_field_exists = true,
      .frame_active = true,
      .is_otp_field_focused = false,
      .is_keyboard_visible = false,
      .has_non_otp_focus_anywhere = false,
      .any_otp_field_has_typed_input = false,
  };
  OtpSecurityGateway::ProactiveTriggerContext hidden_keyboard_context =
      unfocused_context;
  hidden_keyboard_context.is_otp_field_focused = true;

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(
      gateway.EvaluateProactiveTrigger(target, candidate, unfocused_context),
      (OtpDeliveryPolicy::Decision{
          .allowed = false,
          .reason = OtpDeliveryPolicy::SkipReason::kProactiveUnfocusedOnAndroid,
      }));
  EXPECT_EQ(
      gateway.EvaluateProactiveTrigger(target, candidate,
                                       hidden_keyboard_context),
      (OtpDeliveryPolicy::Decision{
          .allowed = false,
          .reason = OtpDeliveryPolicy::SkipReason::kKeyboardNotVisibleOnAndroid,
      }));
#else
  EXPECT_EQ(
      gateway.EvaluateProactiveTrigger(target, candidate, unfocused_context),
      (OtpDeliveryPolicy::Decision{
          .allowed = true,
          .reason = OtpDeliveryPolicy::SkipReason::kNone,
      }));
  EXPECT_EQ(gateway.EvaluateProactiveTrigger(target, candidate,
                                             hidden_keyboard_context),
            (OtpDeliveryPolicy::Decision{
                .allowed = true,
                .reason = OtpDeliveryPolicy::SkipReason::kNone,
            }));
#endif
}

// Verifies that `EvaluateReactiveServing` and `EvaluateProactiveTrigger`
// enforce live consent, non-opaque origin matching (including bound SMS
// origins), frame binding (`verified_frame == target.frame`), and WebOTP
// blocking without invoking PhishGuard.
TEST_F(OtpSecurityGatewayTest,
       EvaluateReactiveAndProactive_EnforcesLiveConsentOriginFrameAndWebOtp) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  OtpSecurityGateway gateway(client_);

  const LocalFrameToken frame_a = test::MakeLocalFrameToken();
  const LocalFrameToken frame_b = test::MakeLocalFrameToken();
  const url::Origin origin = url::Origin::Create(GURL("https://example.test"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.test"));
  const url::Origin opaque_origin;
  OtpTarget target_a = MakeTarget(origin, frame_a);
  OtpTarget target_b = MakeTarget(origin, frame_b);
  OtpCandidate gmail_candidate = MakeGmailCandidate(origin, frame_a);
  OtpCandidate bound_sms_candidate(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()),
      origin, frame_a);

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck).Times(0);

  EXPECT_TRUE(
      gateway.EvaluateReactiveServing(target_a, gmail_candidate).allowed);
  EXPECT_TRUE(
      gateway.EvaluateReactiveServing(target_a, bound_sms_candidate).allowed);

  const OtpSecurityGateway::ProactiveTriggerContext valid_context{
      .target_field_exists = true,
      .frame_active = true,
      .is_otp_field_focused = true,
      .is_keyboard_visible = true,
      .has_non_otp_focus_anywhere = false,
      .any_otp_field_has_typed_input = false,
  };

  // Cross-frame or unverified frame (`verified_frame != target.frame`) fails
  // closed with `kPhishingVerdict`.
  EXPECT_EQ(gateway.EvaluateReactiveServing(target_b, gmail_candidate),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kPhishingVerdict,
            }));
  EXPECT_EQ(gateway.EvaluateProactiveTrigger(target_b, bound_sms_candidate,
                                             valid_context),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kPhishingVerdict,
            }));
  EXPECT_EQ(gateway.EvaluateReactiveServing(
                target_a, MakeGmailCandidate(origin, LocalFrameToken())),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kPhishingVerdict,
            }));

  // Cross-origin and opaque targets fail with `kOriginMismatch` for both Gmail
  // and origin-bound SMS candidates (e.g. after a same-RFH navigation).
  EXPECT_EQ(gateway.EvaluateReactiveServing(MakeTarget(other_origin, frame_a),
                                            gmail_candidate),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
            }));
  EXPECT_EQ(gateway.EvaluateReactiveServing(
                MakeTarget(opaque_origin, frame_a),
                MakeGmailCandidate(opaque_origin, frame_a)),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
            }));
  EXPECT_EQ(gateway.EvaluateReactiveServing(MakeTarget(other_origin, frame_a),
                                            bound_sms_candidate),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
            }));
  EXPECT_EQ(
      gateway.EvaluateProactiveTrigger(MakeTarget(other_origin, frame_a),
                                       bound_sms_candidate, valid_context),
      (OtpDeliveryPolicy::Decision{
          .allowed = false,
          .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
      }));

  // Opt-out blocks reactive serving of a previously verified Gmail candidate.
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  EXPECT_EQ(gateway.EvaluateReactiveServing(target_a, gmail_candidate),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kUserNotOptedIn,
            }));

  // WebOTP usage blocks reactive serving of a previously verified SMS
  // candidate (and takes precedence over SMS origin mismatch).
  client_.set_document_used_web_otp(true);
  EXPECT_EQ(gateway.EvaluateReactiveServing(MakeTarget(other_origin, frame_a),
                                            bound_sms_candidate),
            (OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kWebOtpBlocked,
            }));
}

}  // namespace
}  // namespace autofill
