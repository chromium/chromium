// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenType;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

enum class Start {
  kListening,
  kAvailableAtT1,
  kConsumedAtT1,
  kPhishingAtT1,
  kExpiredAtT1,
  kFieldGone,
};

enum class Action {
  kOfferOlderT0,
  kOfferNewerT2,
  kMarkConsumed,
  kBlockOnPhishing,
  kExpireCache,
  kMarkFieldGone,
};

struct TransitionCase {
  const char* name;
  Start start;
  Action action;
  bool action_succeeds;
  DeliveryState expected_state;
  std::optional<std::string_view> expected_available;
  std::optional<std::string_view> expected_identity;
};

class OtpDeliveryTest : public Test {
 protected:
  OtpTarget MakeTarget(std::string_view origin_str = "https://example.test") {
    const LocalFrameToken frame = test::MakeLocalFrameToken();
    return OtpTarget(url::Origin::Create(GURL(origin_str)), frame,
                     test::MakeFormGlobalId(), test::MakeFieldGlobalId(),
                     frame);
  }

  OtpCandidate MakeGmailCandidate(const OtpTarget& target,
                                  std::string value,
                                  base::Time email_timestamp) {
    return OtpCandidate(OneTimeToken(OneTimeTokenType::kGmail, std::move(value),
                                     base::TimeTicks::Now(), "otp@example.test",
                                     email_timestamp),
                        target.origin, target.frame);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

class OtpDeliveryTransitionTest : public OtpDeliveryTest,
                                  public WithParamInterface<TransitionCase> {};

// Verifies single state transitions (starting state + action -> expected state,
// available candidate, and `GetNewestIdentity()`).
TEST_P(OtpDeliveryTransitionTest, Transition) {
  const TransitionCase& tc = GetParam();
  const OtpTarget target = MakeTarget();
  OtpDelivery delivery(target);

  const base::Time t0 = base::Time::FromSecondsSinceUnixEpoch(1000);
  const base::Time t1 = t0 + base::Seconds(10);
  const base::Time t2 = t0 + base::Seconds(20);

  if (tc.start != Start::kListening) {
    ASSERT_TRUE(delivery.Offer(MakeGmailCandidate(target, "111111", t1)));
  }
  switch (tc.start) {
    case Start::kListening:
    case Start::kAvailableAtT1:
      break;
    case Start::kConsumedAtT1:
      ASSERT_TRUE(delivery.MarkConsumed().has_value());
      break;
    case Start::kPhishingAtT1:
      delivery.BlockOnPhishing();
      break;
    case Start::kExpiredAtT1:
      task_environment_.FastForwardBy(
          one_time_tokens::kCacheDurationForOldTokens + base::Seconds(1));
      break;
    case Start::kFieldGone:
      delivery.MarkFieldGone();
      break;
  }

  switch (tc.action) {
    case Action::kOfferOlderT0: {
      OtpCandidate older = MakeGmailCandidate(target, "000000", t0);
      EXPECT_EQ(delivery.CanAccept(older), tc.action_succeeds);
      EXPECT_EQ(delivery.Offer(older), tc.action_succeeds);
      break;
    }
    case Action::kOfferNewerT2: {
      OtpCandidate newer = MakeGmailCandidate(target, "222222", t2);
      EXPECT_EQ(delivery.CanAccept(newer), tc.action_succeeds);
      EXPECT_EQ(delivery.Offer(newer), tc.action_succeeds);
      break;
    }
    case Action::kMarkConsumed: {
      std::optional<OtpTokenIdentity> consumed = delivery.MarkConsumed();
      ASSERT_EQ(consumed.has_value(), tc.action_succeeds);
      if (tc.action_succeeds) {
        EXPECT_EQ(consumed->value, "111111");
      }
      break;
    }
    case Action::kBlockOnPhishing:
      delivery.BlockOnPhishing();
      break;
    case Action::kExpireCache:
      task_environment_.FastForwardBy(
          one_time_tokens::kCacheDurationForOldTokens + base::Seconds(1));
      break;
    case Action::kMarkFieldGone:
      delivery.MarkFieldGone();
      break;
  }

  EXPECT_EQ(delivery.GetState(), tc.expected_state);
  std::optional<OtpCandidate> available = delivery.GetAvailableCandidate();
  ASSERT_EQ(available.has_value(), tc.expected_available.has_value());
  if (tc.expected_available) {
    EXPECT_EQ(available->token.value(), *tc.expected_available);
  }
  std::optional<OtpTokenIdentity> identity = delivery.GetNewestIdentity();
  ASSERT_EQ(identity.has_value(), tc.expected_identity.has_value());
  if (tc.expected_identity) {
    EXPECT_EQ(identity->value, *tc.expected_identity);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OtpDeliveryTransitionTest,
    Values(
        TransitionCase{"Listening_OfferSucceeds", Start::kListening,
                       Action::kOfferNewerT2, true, DeliveryState::kAvailable,
                       "222222", "222222"},
        TransitionCase{"Available_OutOfOrderOlderCheckRejected",
                       Start::kAvailableAtT1, Action::kOfferOlderT0, false,
                       DeliveryState::kAvailable, "111111", "111111"},
        TransitionCase{"Available_NewerResendReplacesCandidate",
                       Start::kAvailableAtT1, Action::kOfferNewerT2, true,
                       DeliveryState::kAvailable, "222222", "222222"},
        TransitionCase{"Available_MarkConsumedTransitionsToListening",
                       Start::kAvailableAtT1, Action::kMarkConsumed, true,
                       DeliveryState::kListening, std::nullopt, "111111"},
        TransitionCase{"Expired_MarkConsumedStillReturnsIdentity",
                       Start::kExpiredAtT1, Action::kMarkConsumed, true,
                       DeliveryState::kListening, std::nullopt, "111111"},
        TransitionCase{"Consumed_OlderCandidateRejected", Start::kConsumedAtT1,
                       Action::kOfferOlderT0, false, DeliveryState::kListening,
                       std::nullopt, "111111"},
        TransitionCase{"Consumed_NewerResendAccepted", Start::kConsumedAtT1,
                       Action::kOfferNewerT2, true, DeliveryState::kAvailable,
                       "222222", "222222"},
        TransitionCase{"Available_BlockOnPhishingRevokesCandidate",
                       Start::kAvailableAtT1, Action::kBlockOnPhishing, true,
                       DeliveryState::kListening, std::nullopt, std::nullopt},
        TransitionCase{"Phishing_OlderCandidateRejected", Start::kPhishingAtT1,
                       Action::kOfferOlderT0, false, DeliveryState::kListening,
                       std::nullopt, std::nullopt},
        TransitionCase{"Phishing_NewerCandidateRejected", Start::kPhishingAtT1,
                       Action::kOfferNewerT2, false, DeliveryState::kListening,
                       std::nullopt, std::nullopt},
        TransitionCase{"Available_ExpireCacheTransitionsToListening",
                       Start::kAvailableAtT1, Action::kExpireCache, true,
                       DeliveryState::kListening, std::nullopt, "111111"},
        TransitionCase{"Available_MarkFieldGoneIsTerminal",
                       Start::kAvailableAtT1, Action::kMarkFieldGone, true,
                       DeliveryState::kFieldGone, std::nullopt, std::nullopt},
        TransitionCase{"FieldGone_OfferNewerRejected", Start::kFieldGone,
                       Action::kOfferNewerT2, false, DeliveryState::kFieldGone,
                       std::nullopt, std::nullopt},
        TransitionCase{"FieldGone_MarkConsumedNoOp", Start::kFieldGone,
                       Action::kMarkConsumed, false, DeliveryState::kFieldGone,
                       std::nullopt, std::nullopt},
        TransitionCase{"FieldGone_BlockOnPhishingKeepsFieldGone",
                       Start::kFieldGone, Action::kBlockOnPhishing, true,
                       DeliveryState::kFieldGone, std::nullopt, std::nullopt}),
    [](const TestParamInfo<TransitionCase>& info) { return info.param.name; });

// Tests that `CanAccept()` and `Offer()` reject candidates that are already
// consumed, have an empty value, have a null or expired arrival timestamp, or
// violate origin rules for their token type.
TEST_F(OtpDeliveryTest, CandidateValidationRules) {
  const OtpTarget target = MakeTarget("https://example.test");
  OtpDelivery delivery(target);
  const base::Time t0 = base::Time::FromSecondsSinceUnixEpoch(1000);
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.test"));

  OtpCandidate consumed = MakeGmailCandidate(target, "111111", t0);
  consumed.consumed = true;
  OtpCandidate expired = MakeGmailCandidate(target, "111111", t0);
  task_environment_.FastForwardBy(one_time_tokens::kCacheDurationForOldTokens +
                                  base::Seconds(1));

  for (const OtpCandidate& rejected : {
           consumed,
           MakeGmailCandidate(target, "", t0),
           OtpCandidate(OneTimeToken(OneTimeTokenType::kGmail, "111111",
                                     base::TimeTicks(), "otp@example.test"),
                        target.origin, target.frame),
           expired,
           OtpCandidate(
               OneTimeToken(OneTimeTokenType::kGmail, "111111",
                            base::TimeTicks::Now(), "otp@example.test"),
               url::Origin(), target.frame),
           OtpCandidate(OneTimeToken(OneTimeTokenType::kGmail, "222222",
                                     base::TimeTicks::Now(), "otp@other.test"),
                        other_origin, target.frame),
           OtpCandidate(OneTimeToken(OneTimeTokenType::kSmsOtp, "333333",
                                     base::TimeTicks::Now()),
                        other_origin, target.frame),
       }) {
    EXPECT_FALSE(delivery.CanAccept(rejected));
    EXPECT_FALSE(delivery.Offer(rejected));
  }

  OtpCandidate unbound_sms(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "444444", base::TimeTicks::Now()),
      url::Origin(), target.frame);
  EXPECT_TRUE(delivery.CanAccept(unbound_sms));
  EXPECT_TRUE(delivery.Offer(unbound_sms));
}

}  // namespace
}  // namespace autofill
