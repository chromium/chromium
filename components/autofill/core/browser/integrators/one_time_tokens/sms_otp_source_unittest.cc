// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/sms_otp_source.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "components/one_time_tokens/core/browser/sms_otp_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenRetrievalError;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::AnyNumber;
using ::testing::Test;

class MockSmsOtpBackend : public one_time_tokens::SmsOtpBackend {
 public:
  MOCK_METHOD(void,
              RetrieveSmsOtp,
              (base::OnceCallback<void(
                   base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>),
              (override));
};

struct TestSourceObserver : public OtpSource::Observer {
  void OnSourceCandidateAvailable(
      std::optional<OtpTarget> target,
      base::expected<OtpCandidate, OneTimeTokenRetrievalError>
          candidate_or_error) override {
    EXPECT_EQ(target, std::nullopt);
    if (candidate_or_error.has_value()) {
      EXPECT_TRUE(candidate_or_error->verified_origin.opaque());
      EXPECT_TRUE(candidate_or_error->verified_frame.is_empty());
    }
    events.push_back(std::move(candidate_or_error));
  }

  std::vector<base::expected<OtpCandidate, OneTimeTokenRetrievalError>> events;
};

class SmsOtpSourceTest : public Test {
 protected:
  OtpTarget MakeTarget() {
    const LocalFrameToken frame = test::MakeLocalFrameToken();
    return OtpTarget(url::Origin::Create(GURL("https://example.test")), frame,
                     test::MakeFormGlobalId(), test::MakeFieldGlobalId(),
                     frame);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  MockSmsOtpBackend sms_backend_;
  one_time_tokens::SmsOtpServiceImpl sms_service_{&sms_backend_};
};

// Verifies that `SmsOtpSource` notifies its observer and caches the SMS OTP
// token on arrival, suppresses duplicate notifications on subsequent `Start()`
// calls, and stops serving the token once marked consumed.
TEST_F(SmsOtpSourceTest, EmitsSmsTokenAndSuppressesConsumed) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb);

  std::move(backend_cb)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "123456",
                        base::TimeTicks::Now()));

  ASSERT_EQ(observer.events.size(), 1u);
  ASSERT_TRUE(observer.events[0].has_value());
  EXPECT_EQ(observer.events[0]->token.value(), "123456");

  // Re-starting while the same token is cached in `SmsOtpService` should not
  // emit a duplicate notification.
  source.Start(target);
  EXPECT_EQ(observer.events.size(), 1u);

  std::vector<OtpCandidate> available = source.GetAvailable(target);
  ASSERT_EQ(available.size(), 1u);
  EXPECT_EQ(available[0].token.value(), "123456");
  EXPECT_TRUE(available[0].verified_origin.opaque());
  EXPECT_TRUE(available[0].verified_frame.is_empty());

  source.MarkConsumed(available[0].identity);
  EXPECT_TRUE(source.GetAvailable(target).empty());
}

// Verifies that after a newer token `T2` replaces `T1` and is consumed,
// re-starting `SmsOtpSource` (which replays cached `T1` and `T2` from
// `SmsOtpService`) does not re-emit or offer the outdated token `T1`.
TEST_F(SmsOtpSourceTest, OutdatedTokenNotOfferedAfterNewerTokenConsumed) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb_1;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb_1](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb_1 = std::move(cb); });

  OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb_1);

  std::move(backend_cb_1)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "111111",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 1u);
  EXPECT_EQ(observer.events[0]->token.value(), "111111");

  // Advance time and receive `T2` ("222222") at t0 + 10s.
  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb_2;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb_2](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb_2 = std::move(cb); });
  task_environment_.FastForwardBy(base::Seconds(10));
  ASSERT_TRUE(backend_cb_2);
  std::move(backend_cb_2)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "222222",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 2u);
  EXPECT_EQ(observer.events[1]->token.value(), "222222");

  // Consume `T2`.
  std::vector<OtpCandidate> available = source.GetAvailable(target);
  ASSERT_EQ(available.size(), 1u);
  EXPECT_EQ(available[0].token.value(), "222222");
  source.MarkConsumed(available[0].identity);
  EXPECT_TRUE(source.GetAvailable(target).empty());

  // Calling `Start()` again replays both `T1` and `T2` from `SmsOtpService`'s
  // recent token cache; neither `T1` nor `T2` should be emitted or offered.
  source.Start(MakeTarget());
  EXPECT_EQ(observer.events.size(), 2u);
  EXPECT_TRUE(source.GetAvailable(target).empty());
}

// Verifies that a 5s Android SMS re-poll returning the same SMS value with a
// fresh `on_device_arrival_time()` while the consumed token is unexpired is
// ignored, whereas a strictly newer token with distinct digits is accepted.
TEST_F(SmsOtpSourceTest, IgnoresSameValueRepollAndAcceptsDistinctNewerToken) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb);

  std::move(backend_cb)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "222222",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 1u);

  std::vector<OtpCandidate> available = source.GetAvailable(target);
  ASSERT_EQ(available.size(), 1u);
  source.MarkConsumed(available[0].identity);
  EXPECT_TRUE(source.GetAvailable(target).empty());

  // A 5s re-poll returning the same SMS value ("222222") with a fresh
  // `on_device_arrival_time()` while unexpired must be ignored.
  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb_repoll;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb_repoll](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb_repoll = std::move(cb); });
  task_environment_.FastForwardBy(base::Seconds(5));
  ASSERT_TRUE(backend_cb_repoll);
  std::move(backend_cb_repoll)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "222222",
                        base::TimeTicks::Now()));
  EXPECT_EQ(observer.events.size(), 1u);
  EXPECT_TRUE(source.GetAvailable(target).empty());

  // A strictly newer token ("333333") with distinct digits within the 3-minute
  // window is accepted and resets consumption state.
  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb_distinct;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb_distinct](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb_distinct = std::move(cb); });
  task_environment_.FastForwardBy(base::Seconds(5));
  ASSERT_TRUE(backend_cb_distinct);
  std::move(backend_cb_distinct)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "333333",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 2u);
  ASSERT_TRUE(observer.events[1].has_value());
  EXPECT_EQ(observer.events[1]->token.value(), "333333");
  EXPECT_EQ(source.GetAvailable(target).size(), 1u);
}

// Verifies that once `kCacheDurationForOldTokens` elapses from a consumed
// token's initial arrival, a genuine resend with the same digits is accepted
// again.
TEST_F(SmsOtpSourceTest, AcceptsSameDigitsResendAfterCacheExpiration) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb);

  std::move(backend_cb)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "333333",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 1u);

  std::vector<OtpCandidate> available = source.GetAvailable(target);
  ASSERT_EQ(available.size(), 1u);
  source.MarkConsumed(available[0].identity);
  EXPECT_TRUE(source.GetAvailable(target).empty());

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb_resend;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb_resend](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb_resend = std::move(cb); });
  task_environment_.FastForwardBy(one_time_tokens::kCacheDurationForOldTokens +
                                  base::Seconds(1));
  ASSERT_TRUE(backend_cb_resend);

  source.Start(MakeTarget());
  std::move(backend_cb_resend)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "333333",
                        base::TimeTicks::Now()));
  ASSERT_EQ(observer.events.size(), 3u);
  EXPECT_FALSE(observer.events[1].has_value());
  ASSERT_TRUE(observer.events[2].has_value());
  EXPECT_EQ(observer.events[2]->token.value(), "333333");
  EXPECT_EQ(source.GetAvailable(target).size(), 1u);
}

// Verifies that `GetAvailable()` stops returning a cached SMS OTP once it ages
// past `kCacheDurationForOldTokens`, and that subscription expiration notifies
// the observer with `kSubscriptionExpired`.
TEST_F(SmsOtpSourceTest, ExpiredTokenAndSubscriptionExpiration) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb);

  std::move(backend_cb)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "654321",
                        base::TimeTicks::Now()));
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  // After 1 minute, the subscription expires and notifies the observer.
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp).Times(AnyNumber());
  task_environment_.FastForwardBy(base::Minutes(1));
  ASSERT_EQ(observer.events.size(), 2u);
  ASSERT_FALSE(observer.events[1].has_value());
  EXPECT_EQ(observer.events[1].error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);

  // Token is still within the 3-minute cache window at 1 minute, then expires
  // past `kCacheDurationForOldTokens`.
  EXPECT_EQ(source.GetAvailable(target).size(), 1u);
  task_environment_.FastForwardBy(one_time_tokens::kCacheDurationForOldTokens);
  EXPECT_TRUE(source.GetAvailable(target).empty());
}

// Verifies that `Stop()` cancels the active subscription so no further tokens
// or subscription-expired notifications are delivered.
TEST_F(SmsOtpSourceTest, StopCancelsSubscription) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  source.Start(MakeTarget());
  ASSERT_TRUE(backend_cb);

  source.Stop();

  // Resolving the backend callback or advancing past subscription expiry after
  // `Stop()` should not notify the observer.
  std::move(backend_cb)
      .Run(OneTimeToken(OneTimeTokenType::kSmsOtp, "123456",
                        base::TimeTicks::Now()));
  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_TRUE(observer.events.empty());
}

// Verifies that constructing `SmsOtpSource(nullptr)` is a safe no-op.
TEST_F(SmsOtpSourceTest, NullServiceIsNoOp) {
  TestSourceObserver observer;
  SmsOtpSource source(nullptr);
  source.SetObserver(&observer);
  source.Start(MakeTarget());
  EXPECT_TRUE(source.GetAvailable(MakeTarget()).empty());
  EXPECT_TRUE(observer.events.empty());
}

// Verifies that calling `Start()` while a subscription is alive renews its
// expiration time via `SetExpirationTime()`.
TEST_F(SmsOtpSourceTest, StartRenewsActiveSubscriptionExpiration) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  EXPECT_CALL(sms_backend_, RetrieveSmsOtp).Times(1);
  const OtpTarget target = MakeTarget();
  source.Start(target);

  // Calling `Start()` again at t0 + 40s extends the 1-minute subscription to
  // t0 + 100s, so it does not expire at t0 + 60s.
  task_environment_.FastForwardBy(base::Seconds(40));
  source.Start(target);
  task_environment_.FastForwardBy(base::Seconds(25));
  EXPECT_TRUE(observer.events.empty());

  // Advance to t0 + 100s (35s more) to trigger the renewed expiration.
  task_environment_.FastForwardBy(base::Seconds(35));
  ASSERT_EQ(observer.events.size(), 1u);
  ASSERT_FALSE(observer.events[0].has_value());
  EXPECT_EQ(observer.events[0].error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);
}

// Verifies that backend retrieval errors are forwarded to the observer and
// drop the subscription so a subsequent `Start()` initiates a fresh fetch.
TEST_F(SmsOtpSourceTest, ForwardsBackendErrorAndResetsSubscription) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);

  base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      backend_cb;
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
      .WillOnce(
          [&backend_cb](
              base::OnceCallback<void(
                  base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                  cb) { backend_cb = std::move(cb); });

  const OtpTarget target = MakeTarget();
  source.Start(target);
  ASSERT_TRUE(backend_cb);

  std::move(backend_cb)
      .Run(base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));
  ASSERT_EQ(observer.events.size(), 1u);
  ASSERT_FALSE(observer.events[0].has_value());
  EXPECT_EQ(observer.events[0].error(),
            OneTimeTokenRetrievalError::kSmsOtpBackendError);

  // Because the error ended the subscription, calling `Start()` again starts a
  // new backend retrieval immediately.
  EXPECT_CALL(sms_backend_, RetrieveSmsOtp).Times(1);
  source.Start(target);
}

// Verifies that empty, null-arrival, or already-expired tokens from the
// backend are ignored and never offered.
TEST_F(SmsOtpSourceTest, IgnoresInvalidOrAlreadyExpiredTokens) {
  TestSourceObserver observer;
  SmsOtpSource source(&sms_service_);
  source.SetObserver(&observer);
  const OtpTarget target = MakeTarget();

  for (const OneTimeToken& invalid_token : {
           OneTimeToken(OneTimeTokenType::kSmsOtp, "", base::TimeTicks::Now()),
           OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks()),
           OneTimeToken(OneTimeTokenType::kSmsOtp, "123456",
                        base::TimeTicks::Now() -
                            one_time_tokens::kCacheDurationForOldTokens -
                            base::Seconds(1)),
       }) {
    base::OnceCallback<void(
        base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
        cb;
    EXPECT_CALL(sms_backend_, RetrieveSmsOtp)
        .WillOnce(
            [&cb](base::OnceCallback<void(
                      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
                      backend_callback) { cb = std::move(backend_callback); });
    source.Start(target);
    ASSERT_TRUE(cb);
    std::move(cb).Run(invalid_token);
    EXPECT_TRUE(observer.events.empty());
    EXPECT_TRUE(source.GetAvailable(target).empty());
    source.Stop();
  }
}

}  // namespace
}  // namespace autofill
