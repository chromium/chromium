// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery_policy.h"

#include <string>

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using one_time_tokens::OneTimeTokenType;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;
using Decision = OtpDeliveryPolicy::Decision;
using Input = OtpDeliveryPolicy::Input;
using SkipReason = OtpDeliveryPolicy::SkipReason;
using Stage = OtpDeliveryPolicy::Stage;

// Returns a baseline `Input` where every gate passes so individual test cases
// can flip only the specific condition(s) under test.
Input MakeAllowedInput(OneTimeTokenType source, Stage stage) {
  return Input{
      .source = source,
      .stage = stage,
      .user_opted_into_gmail = true,
      .origin_matches = true,
      .web_otp_used = false,
      .target_field_exists = true,
      .frame_active = true,
      .has_non_otp_focus_anywhere = false,
      .any_otp_field_has_typed_input = false,
      .is_android = false,
      .is_otp_field_focused = true,
      .is_keyboard_visible = true,
      .phish_guard_verdict = OneTimeTokensPhishGuardVerdict::kNotPhishing,
  };
}

struct PolicyTestCase {
  std::string test_name;
  Input input;
  Decision expected;
};

class OtpDeliveryPolicyTest : public Test,
                              public WithParamInterface<PolicyTestCase> {};

// Verifies that `OtpDeliveryPolicy::Evaluate` returns the expected `Decision`
// and non-empty `SkipReasonToStringView` description across all policy
// scenarios.
TEST_P(OtpDeliveryPolicyTest, Evaluate) {
  const PolicyTestCase& test_case = GetParam();
  const Decision decision = OtpDeliveryPolicy::Evaluate(test_case.input);
  EXPECT_EQ(test_case.expected, decision);
  EXPECT_FALSE(
      OtpDeliveryPolicy::SkipReasonToStringView(decision.reason).empty());
}

// Verifies that `Input` defaults every field to a fail-closed value so omitting
// any required field blocks OTP delivery instead of silently allowing it.
TEST(OtpDeliveryPolicyDefaultsTest, OmittedFieldsFailClosed) {
  EXPECT_FALSE(OtpDeliveryPolicy::Evaluate(Input{}).allowed);

  // Minimal SMS verification requires explicit `source`, `stage`,
  // `web_otp_used`, and `phish_guard_verdict`.
  EXPECT_EQ(
      (Decision{.allowed = true, .reason = SkipReason::kNone}),
      OtpDeliveryPolicy::Evaluate(
          {.source = OneTimeTokenType::kSmsOtp,
           .stage = Stage::kVerification,
           .web_otp_used = false,
           .phish_guard_verdict = OneTimeTokensPhishGuardVerdict::kUnknown}));
  // Omitting `web_otp_used` fails closed with `kWebOtpBlocked`.
  EXPECT_EQ(
      (Decision{.allowed = false, .reason = SkipReason::kWebOtpBlocked}),
      OtpDeliveryPolicy::Evaluate(
          {.source = OneTimeTokenType::kSmsOtp,
           .stage = Stage::kVerification,
           .phish_guard_verdict = OneTimeTokensPhishGuardVerdict::kUnknown}));
  // Omitting `phish_guard_verdict` fails closed with `kPhishingVerdict`.
  EXPECT_EQ(
      (Decision{.allowed = false, .reason = SkipReason::kPhishingVerdict}),
      OtpDeliveryPolicy::Evaluate({.source = OneTimeTokenType::kSmsOtp,
                                   .stage = Stage::kVerification,
                                   .web_otp_used = false}));
  // Omitting `stage` runs proactive checks, which fail closed.
  EXPECT_FALSE(
      OtpDeliveryPolicy::Evaluate(
          {.source = OneTimeTokenType::kSmsOtp,
           .web_otp_used = false,
           .phish_guard_verdict = OneTimeTokensPhishGuardVerdict::kNotPhishing})
          .allowed);

  // Minimal Gmail verification requires `user_opted_into_gmail` and
  // `origin_matches`; omitting either fails closed.
  EXPECT_EQ((Decision{.allowed = false, .reason = SkipReason::kUserNotOptedIn}),
            OtpDeliveryPolicy::Evaluate(
                {.source = OneTimeTokenType::kGmail,
                 .stage = Stage::kVerification,
                 .origin_matches = true,
                 .phish_guard_verdict =
                     OneTimeTokensPhishGuardVerdict::kNotPhishing}));
  EXPECT_EQ((Decision{.allowed = false, .reason = SkipReason::kOriginMismatch}),
            OtpDeliveryPolicy::Evaluate(
                {.source = OneTimeTokenType::kGmail,
                 .stage = Stage::kVerification,
                 .user_opted_into_gmail = true,
                 .phish_guard_verdict =
                     OneTimeTokensPhishGuardVerdict::kNotPhishing}));

  // Omitting `is_android` on Desktop proactive triggers enforces Android focus
  // and keyboard gates by default (`is_android = true`).
  EXPECT_EQ((Decision{.allowed = false,
                      .reason = SkipReason::kProactiveUnfocusedOnAndroid}),
            OtpDeliveryPolicy::Evaluate(
                {.source = OneTimeTokenType::kSmsOtp,
                 .stage = Stage::kProactiveTrigger,
                 .web_otp_used = false,
                 .target_field_exists = true,
                 .frame_active = true,
                 .has_non_otp_focus_anywhere = false,
                 .any_otp_field_has_typed_input = false,
                 .phish_guard_verdict =
                     OneTimeTokensPhishGuardVerdict::kNotPhishing}));
}

INSTANTIATE_TEST_SUITE_P(
    OtpDeliveryPolicyTests,
    OtpDeliveryPolicyTest,
    Values(
        // --- Baseline allowed cases across sources, stages, and platforms ---
        PolicyTestCase{
            .test_name = "SmsVerificationAllowedWithMinimalRequiredFields",
            .input = {.source = OneTimeTokenType::kSmsOtp,
                      .stage = Stage::kVerification,
                      .web_otp_used = false,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kUnknown},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "SmsVerificationIgnoresNonMatchingOrigin",
            .input = {.source = OneTimeTokenType::kSmsOtp,
                      .stage = Stage::kVerification,
                      .origin_matches = false,
                      .web_otp_used = false,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kNotPhishing},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "GmailVerificationAllowedWithConsentAndMatchingOrigin",
            .input = {.source = OneTimeTokenType::kGmail,
                      .stage = Stage::kVerification,
                      .user_opted_into_gmail = true,
                      .origin_matches = true,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kNotPhishing},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "SmsProactiveDesktopUnfocusedAllowed",
            .input = {.source = OneTimeTokenType::kSmsOtp,
                      .stage = Stage::kProactiveTrigger,
                      .web_otp_used = false,
                      .target_field_exists = true,
                      .frame_active = true,
                      .has_non_otp_focus_anywhere = false,
                      .any_otp_field_has_typed_input = false,
                      .is_android = false,
                      .is_otp_field_focused = false,
                      .is_keyboard_visible = false,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kNotPhishing},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "GmailProactiveDesktopUnfocusedAllowed",
            .input = {.source = OneTimeTokenType::kGmail,
                      .stage = Stage::kProactiveTrigger,
                      .user_opted_into_gmail = true,
                      .origin_matches = true,
                      .target_field_exists = true,
                      .frame_active = true,
                      .has_non_otp_focus_anywhere = false,
                      .any_otp_field_has_typed_input = false,
                      .is_android = false,
                      .is_otp_field_focused = false,
                      .is_keyboard_visible = false,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kNotPhishing},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "AndroidProactiveFocusedWithKeyboardAllowed",
            .input = {.source = OneTimeTokenType::kGmail,
                      .stage = Stage::kProactiveTrigger,
                      .user_opted_into_gmail = true,
                      .origin_matches = true,
                      .target_field_exists = true,
                      .frame_active = true,
                      .has_non_otp_focus_anywhere = false,
                      .any_otp_field_has_typed_input = false,
                      .is_android = true,
                      .is_otp_field_focused = true,
                      .is_keyboard_visible = true,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kNotPhishing},
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },

        // --- Source & security gating flags (both stages) ---
        PolicyTestCase{
            .test_name = "GmailVerificationBlockedWithoutConsent",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.user_opted_into_gmail = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kUserNotOptedIn},
        },
        PolicyTestCase{
            .test_name = "GmailProactiveBlockedWithoutConsent",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.user_opted_into_gmail = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kUserNotOptedIn},
        },
        PolicyTestCase{
            .test_name = "GmailVerificationBlockedOnOriginMismatch",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.origin_matches = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kOriginMismatch},
        },
        PolicyTestCase{
            .test_name = "GmailProactiveBlockedOnOriginMismatch",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.origin_matches = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kOriginMismatch},
        },
        PolicyTestCase{
            .test_name = "SmsVerificationBlockedByWebOtp",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kVerification);
                  input.web_otp_used = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kWebOtpBlocked},
        },
        PolicyTestCase{
            .test_name = "SmsProactiveBlockedByWebOtp",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.web_otp_used = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kWebOtpBlocked},
        },
        PolicyTestCase{
            .test_name = "GmailAllowedWhenWebOtpUsed",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.web_otp_used = true;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "PhishingBlocksSmsVerification",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kVerification);
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishingVerdict},
        },
        PolicyTestCase{
            .test_name = "PhishingBlocksSmsProactive",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishingVerdict},
        },
        PolicyTestCase{
            .test_name = "PhishingBlocksGmailVerification",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishingVerdict},
        },
        PolicyTestCase{
            .test_name = "PhishingBlocksGmailProactive",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishingVerdict},
        },
        PolicyTestCase{
            .test_name = "PhishGuardTimeoutBlocksVerification",
            .input = {.source = OneTimeTokenType::kSmsOtp,
                      .stage = Stage::kVerification,
                      .web_otp_used = false,
                      .phish_guard_verdict =
                          OneTimeTokensPhishGuardVerdict::kTimedOut},
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishGuardTimedOut},
        },

        // --- Proactive-only UX preconditions (and verification-stage pass) ---
        PolicyTestCase{
            .test_name = "ProactiveBlockedWhenNonOtpFieldFocused",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.has_non_otp_focus_anywhere = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kNonOtpFieldFocused},
        },
        PolicyTestCase{
            .test_name = "VerificationIgnoresNonOtpFieldFocused",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.has_non_otp_focus_anywhere = true;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "ProactiveBlockedWhenTargetFieldMissing",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.target_field_exists = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kTargetFieldMissing},
        },
        PolicyTestCase{
            .test_name = "VerificationIgnoresTargetFieldMissing",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.target_field_exists = false;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "ProactiveBlockedWhenFrameInactive",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.frame_active = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kFrameInactive},
        },
        PolicyTestCase{
            .test_name = "VerificationIgnoresFrameInactive",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.frame_active = false;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "ProactiveBlockedWhenOtpFieldHasTypedInput",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.any_otp_field_has_typed_input = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kTypedInputPresent},
        },
        PolicyTestCase{
            .test_name = "VerificationIgnoresOtpFieldTypedInput",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.any_otp_field_has_typed_input = true;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },
        PolicyTestCase{
            .test_name = "AndroidProactiveBlockedWhenOtpFieldUnfocused",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.is_android = true;
                  input.is_otp_field_focused = false;
                  input.is_keyboard_visible = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kProactiveUnfocusedOnAndroid},
        },
        PolicyTestCase{
            .test_name = "AndroidProactiveBlockedWhenKeyboardHidden",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.is_android = true;
                  input.is_otp_field_focused = true;
                  input.is_keyboard_visible = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kKeyboardNotVisibleOnAndroid},
        },
        PolicyTestCase{
            .test_name = "AndroidVerificationIgnoresUnfocusedAndHiddenKeyboard",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kVerification);
                  input.is_android = true;
                  input.is_otp_field_focused = false;
                  input.is_keyboard_visible = false;
                  return input;
                }(),
            .expected = {.allowed = true, .reason = SkipReason::kNone},
        },

        // --- Skip-reason precedence hierarchy ---
        PolicyTestCase{
            .test_name = "PrecedenceUserNotOptedInBeforeOriginMismatch",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.user_opted_into_gmail = false;
                  input.origin_matches = false;
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kUserNotOptedIn},
        },
        PolicyTestCase{
            .test_name = "PrecedenceOriginMismatchBeforePhishing",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kGmail,
                                                 Stage::kProactiveTrigger);
                  input.origin_matches = false;
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kOriginMismatch},
        },
        PolicyTestCase{
            .test_name = "PrecedenceWebOtpBlockedBeforePhishing",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.web_otp_used = true;
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kWebOtpBlocked},
        },
        PolicyTestCase{
            .test_name = "PrecedencePhishingBeforeProactiveChecks",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.target_field_exists = false;
                  input.has_non_otp_focus_anywhere = true;
                  input.phish_guard_verdict =
                      OneTimeTokensPhishGuardVerdict::kPhishing;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kPhishingVerdict},
        },
        PolicyTestCase{
            .test_name = "PrecedenceNonOtpFieldFocusedBeforeTargetFieldMissing",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.target_field_exists = false;
                  input.frame_active = false;
                  input.has_non_otp_focus_anywhere = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kNonOtpFieldFocused},
        },
        PolicyTestCase{
            .test_name = "PrecedenceTargetFieldMissingBeforeFrameInactive",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.target_field_exists = false;
                  input.frame_active = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kTargetFieldMissing},
        },
        PolicyTestCase{
            .test_name = "PrecedenceFrameInactiveBeforeTypedInput",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.frame_active = false;
                  input.any_otp_field_has_typed_input = true;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kFrameInactive},
        },
        PolicyTestCase{
            .test_name = "PrecedenceTypedInputBeforeAndroidUnfocused",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.any_otp_field_has_typed_input = true;
                  input.is_android = true;
                  input.is_otp_field_focused = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kTypedInputPresent},
        },
        PolicyTestCase{
            .test_name = "PrecedenceAndroidUnfocusedBeforeKeyboardHidden",
            .input =
                [] {
                  Input input = MakeAllowedInput(OneTimeTokenType::kSmsOtp,
                                                 Stage::kProactiveTrigger);
                  input.is_android = true;
                  input.is_otp_field_focused = false;
                  input.is_keyboard_visible = false;
                  return input;
                }(),
            .expected = {.allowed = false,
                         .reason = SkipReason::kProactiveUnfocusedOnAndroid},
        }),
    [](const TestParamInfo<PolicyTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace autofill
