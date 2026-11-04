// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion_dropdown_dispatcher.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;

class OtpSuggestionDropdownDispatcherTest : public Test {
 protected:
  OtpTarget MakeTarget() {
    LocalFrameToken frame = test::MakeLocalFrameToken();
    return OtpTarget(url::Origin::Create(GURL("https://example.test")), frame,
                     test::MakeFormGlobalId(), test::MakeFieldGlobalId(),
                     frame);
  }

  OtpCandidate MakeSmsCandidate(const OtpTarget& target,
                                std::string value,
                                base::TimeTicks arrival) {
    return OtpCandidate(
        OneTimeToken(OneTimeTokenType::kSmsOtp, std::move(value), arrival),
        target.origin, target.frame);
  }

  OtpCandidate MakeGmailCandidate(const OtpTarget& target,
                                  std::string value,
                                  base::Time email_received_timestamp,
                                  base::TimeTicks arrival) {
    return OtpCandidate(
        OneTimeToken(OneTimeTokenType::kGmail, std::move(value), arrival,
                     "otp@example.test", email_received_timestamp),
        target.origin, target.frame);
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests that `TriggerProactive()` rejects empty or consumed candidates without
// invoking `trigger_callback`, does not record deduplication state when
// `trigger_callback` returns false, and deduplicates identical OTP values or
// non-newer timestamps once triggered.
TEST_F(OtpSuggestionDropdownDispatcherTest,
       TriggerProactive_DeduplicationAndCallbackResult) {
  OtpSuggestionDropdownDispatcher dispatcher;
  const OtpTarget target = MakeTarget();
  const base::TimeTicks t0 = base::TimeTicks::Now();
  StrictMock<MockFunction<bool(const OtpTarget&)>> trigger_cb;

  OtpCandidate empty = MakeSmsCandidate(target, "", t0);
  OtpCandidate consumed = MakeSmsCandidate(target, "123456", t0);
  consumed.consumed = true;
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, empty,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, consumed,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, empty,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, consumed,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));

  // If `trigger_callback` returns false, deduplication state is not recorded so
  // the same candidate can be triggered on a subsequent attempt.
  const OtpCandidate c1 = MakeSmsCandidate(target, "123456", t0);
  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(false));
  EXPECT_FALSE(dispatcher.TriggerProactive(
      target, c1, /*can_show_on_screen_now=*/true, trigger_cb.AsStdFunction()));
  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(
      target, c1, /*can_show_on_screen_now=*/true, trigger_cb.AsStdFunction()));

  // Same OTP value (even with a newer timestamp) and older/equal-timestamp OTPs
  // are suppressed (both when visible and when hidden); a strictly newer OTP
  // with a different value is allowed.
  const OtpCandidate c1_dup =
      MakeSmsCandidate(target, "123456", t0 + base::Seconds(5));
  const OtpCandidate c_same_time = MakeSmsCandidate(target, "654321", t0);
  const OtpCandidate c2_newer =
      MakeSmsCandidate(target, "654321", t0 + base::Seconds(10));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c1_dup,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c_same_time,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c1_dup,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));

  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(target, c2_newer,
                                          /*can_show_on_screen_now=*/true,
                                          trigger_cb.AsStdFunction()));
}

// Tests that `TriggerProactive()` defers triggers when
// `can_show_on_screen_now` is false, `RetryPendingTriggers()` extracts and
// retries them, and `RecordShown()` monotonically updates the offered watermark
// and only clears pending triggers that are now deduplicated.
TEST_F(OtpSuggestionDropdownDispatcherTest,
       DeferredPendingTriggersAndRecordShown) {
  OtpSuggestionDropdownDispatcher dispatcher;
  const OtpTarget target = MakeTarget();
  const base::TimeTicks t0 = base::TimeTicks::Now();
  const OtpCandidate c1 = MakeSmsCandidate(target, "123456", t0);
  const OtpCandidate c1_dup =
      MakeSmsCandidate(target, "123456", t0 + base::Seconds(5));
  const OtpCandidate c2_newer =
      MakeSmsCandidate(target, "654321", t0 + base::Seconds(10));
  const OtpCandidate c3_newest =
      MakeSmsCandidate(target, "999999", t0 + base::Seconds(20));
  StrictMock<MockFunction<bool(const OtpTarget&)>> trigger_cb;

  // When `can_show_on_screen_now` is false, records a pending trigger without
  // running `trigger_callback`.
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c1,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target.field));

  // `RetryPendingTriggers()` extracts the pending target and invokes
  // `retry_callback`, which can re-enter `TriggerProactive()`.
  bool triggered = false;
  dispatcher.RetryPendingTriggers([&](const OtpTarget& retried_target) {
    EXPECT_EQ(retried_target, target);
    EXPECT_FALSE(dispatcher.HasPendingTrigger(retried_target.field));
    EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
    triggered = dispatcher.TriggerProactive(retried_target, c1,
                                            /*can_show_on_screen_now=*/true,
                                            trigger_cb.AsStdFunction());
  });
  EXPECT_TRUE(triggered);
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));

  // While hidden, `pending_trigger` advances monotonically to newer candidates
  // and ignores older out-of-order candidates. Empty or older `RecordShown()`
  // calls must not clear `c2_newer`'s pending trigger, whereas `RecordShown()`
  // for `c2_newer` clears it.
  const OtpCandidate c_mid1 =
      MakeSmsCandidate(target, "777777", t0 + base::Seconds(3));
  const OtpCandidate c_mid2 =
      MakeSmsCandidate(target, "888888", t0 + base::Seconds(7));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c_mid1,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c2_newer,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c_mid2,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target.field));

  dispatcher.RecordShown(
      target.field,
      MakeSmsCandidate(target, "", t0 + base::Seconds(30)).identity);
  dispatcher.RecordShown(target.field, c_mid2.identity);
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target.field));

  dispatcher.RecordShown(target.field, c2_newer.identity);
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));

  // A late `RecordShown()` with an older identity (`c1`) must not regress
  // `last_offered_identity` back from `c2_newer` to `c1`.
  dispatcher.RecordShown(target.field, c1.identity);
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c1_dup,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c2_newer,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));

  // Defer `c3_newest` while hidden, then resolve it directly via a visible
  // `TriggerProactive()` call: if `trigger_callback` fails, the pending trigger
  // remains; once `trigger_callback` succeeds, the pending trigger is cleared.
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c3_newest,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target.field));

  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(false));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, c3_newest,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target.field));

  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(target, c3_newest,
                                          /*can_show_on_screen_now=*/true,
                                          trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target.field));
}

// Tests that `TriggerProactive()` orders Gmail OTPs by server-side
// `email_received_timestamp` (taking precedence over `on_device_arrival`) and
// suppresses duplicate Gmail OTP values.
TEST_F(OtpSuggestionDropdownDispatcherTest,
       GmailCandidates_ServerTimestampPrecedenceAndDuplicateSuppression) {
  OtpSuggestionDropdownDispatcher dispatcher;
  const OtpTarget target = MakeTarget();
  const base::Time email_t0 = base::Time::Now();
  const base::TimeTicks arrival_t0 = base::TimeTicks::Now();
  StrictMock<MockFunction<bool(const OtpTarget&)>> trigger_cb;

  const OtpCandidate g1 =
      MakeGmailCandidate(target, "111111", email_t0, arrival_t0);
  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(
      target, g1, /*can_show_on_screen_now=*/true, trigger_cb.AsStdFunction()));

  // Older email received on-device later is suppressed.
  const OtpCandidate g_older_email =
      MakeGmailCandidate(target, "222222", email_t0 - base::Seconds(5),
                         arrival_t0 + base::Seconds(10));
  // Newer email with the same OTP value is suppressed.
  const OtpCandidate g_same_value =
      MakeGmailCandidate(target, "111111", email_t0 + base::Seconds(10),
                         arrival_t0 + base::Seconds(10));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target, g_older_email,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target, g_same_value,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));

  // Strictly newer email timestamp with a distinct value is accepted even if
  // its on-device arrival timestamp is equal.
  const OtpCandidate g2_newer = MakeGmailCandidate(
      target, "333333", email_t0 + base::Seconds(10), arrival_t0);
  EXPECT_CALL(trigger_cb, Call(target)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(target, g2_newer,
                                          /*can_show_on_screen_now=*/true,
                                          trigger_cb.AsStdFunction()));
}

// Tests that `ClearField()` and `ClearAll()` reset pending and deduplication
// state for the targeted field(s), including when invoked re-entrantly during
// `RetryPendingTriggers()`.
TEST_F(OtpSuggestionDropdownDispatcherTest, ClearFieldAndClearAll) {
  OtpSuggestionDropdownDispatcher dispatcher;
  const OtpTarget target1 = MakeTarget();
  const OtpTarget target2 = MakeTarget();
  const base::TimeTicks t0 = base::TimeTicks::Now();
  const OtpCandidate c1 = MakeSmsCandidate(target1, "111111", t0);
  const OtpCandidate c2 = MakeSmsCandidate(target2, "222222", t0);
  StrictMock<MockFunction<bool(const OtpTarget&)>> trigger_cb;

  dispatcher.RecordShown(target1.field, c1.identity);
  dispatcher.RecordShown(target2.field, c2.identity);
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(
      target1, MakeSmsCandidate(target1, "333333", t0 + base::Seconds(5)),
      /*can_show_on_screen_now=*/false, trigger_cb.AsStdFunction()));
  EXPECT_TRUE(dispatcher.HasPendingTrigger(target1.field));

  dispatcher.ClearField(target1.field);
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target1.field));
  EXPECT_CALL(trigger_cb, Call(target1)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(target1, c1,
                                          /*can_show_on_screen_now=*/true,
                                          trigger_cb.AsStdFunction()));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target2, c2,
                                           /*can_show_on_screen_now=*/true,
                                           trigger_cb.AsStdFunction()));

  dispatcher.ClearAll();
  EXPECT_CALL(trigger_cb, Call(target2)).WillOnce(Return(true));
  EXPECT_TRUE(dispatcher.TriggerProactive(target2, c2,
                                          /*can_show_on_screen_now=*/true,
                                          trigger_cb.AsStdFunction()));

  // Re-entrant `ClearField()` and `ClearAll()` during `RetryPendingTriggers()`
  // cancel remaining queued fields before their callbacks run.
  const OtpCandidate c1_next =
      MakeSmsCandidate(target1, "444444", t0 + base::Seconds(10));
  const OtpCandidate c2_next =
      MakeSmsCandidate(target2, "555555", t0 + base::Seconds(10));
  EXPECT_CALL(trigger_cb, Call).Times(0);
  EXPECT_FALSE(dispatcher.TriggerProactive(target1, c1_next,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target2, c2_next,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  int retry_calls = 0;
  dispatcher.RetryPendingTriggers([&](const OtpTarget& retried_target) {
    ++retry_calls;
    dispatcher.ClearField(
        retried_target.field == target1.field ? target2.field : target1.field);
  });
  EXPECT_EQ(retry_calls, 1);
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target1.field));
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target2.field));

  EXPECT_FALSE(dispatcher.TriggerProactive(target1, c1_next,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  EXPECT_FALSE(dispatcher.TriggerProactive(target2, c2_next,
                                           /*can_show_on_screen_now=*/false,
                                           trigger_cb.AsStdFunction()));
  retry_calls = 0;
  dispatcher.RetryPendingTriggers([&](const OtpTarget&) {
    ++retry_calls;
    dispatcher.ClearAll();
  });
  EXPECT_EQ(retry_calls, 1);
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target1.field));
  EXPECT_FALSE(dispatcher.HasPendingTrigger(target2.field));
}

}  // namespace
}  // namespace autofill
