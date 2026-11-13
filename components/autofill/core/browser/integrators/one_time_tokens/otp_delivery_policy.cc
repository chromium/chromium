// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery_policy.h"

#include <string_view>

#include "base/notreached.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"

namespace autofill {

// static
OtpDeliveryPolicy::Decision OtpDeliveryPolicy::Evaluate(const Input& input) {
  switch (input.source) {
    case one_time_tokens::OneTimeTokenType::kGmail:
      if (!input.user_opted_into_gmail) {
        return {.allowed = false, .reason = SkipReason::kUserNotOptedIn};
      }
      if (!input.origin_matches) {
        return {.allowed = false, .reason = SkipReason::kOriginMismatch};
      }
      break;
    case one_time_tokens::OneTimeTokenType::kSmsOtp:
      // TODO(crbug.com/415273270): Decide whether WebOTP blocking should also
      // apply to Gmail OTPs. Keep SMS-only for now.
      if (input.web_otp_used) {
        return {.allowed = false, .reason = SkipReason::kWebOtpBlocked};
      }
      break;
  }

  switch (input.phish_guard_verdict) {
    case OneTimeTokensPhishGuardVerdict::kPhishing:
      return {.allowed = false, .reason = SkipReason::kPhishingVerdict};
    case OneTimeTokensPhishGuardVerdict::kTimedOut:
      return {.allowed = false, .reason = SkipReason::kPhishGuardTimedOut};
    case OneTimeTokensPhishGuardVerdict::kUnknown:
    case OneTimeTokensPhishGuardVerdict::kNotPhishing:
      break;
  }

  switch (input.stage) {
    case Stage::kVerification:
      return {.allowed = true, .reason = SkipReason::kNone};
    case Stage::kProactiveTrigger:
      if (input.has_non_otp_focus_anywhere) {
        return {.allowed = false, .reason = SkipReason::kNonOtpFieldFocused};
      }
      if (!input.target_field_exists) {
        return {.allowed = false, .reason = SkipReason::kTargetFieldMissing};
      }
      if (!input.frame_active) {
        return {.allowed = false, .reason = SkipReason::kFrameInactive};
      }
      // Tab visibility and window focus deferral are handled downstream by
      // `OtpSuggestionDropdownDispatcher` so triggers arriving while a tab is
      // backgrounded can be shown when the user returns.
      if (input.any_otp_field_has_typed_input) {
        return {.allowed = false, .reason = SkipReason::kTypedInputPresent};
      }
      if (input.is_android) {
        if (!input.is_otp_field_focused) {
          return {.allowed = false,
                  .reason = SkipReason::kProactiveUnfocusedOnAndroid};
        }
        if (!input.is_keyboard_visible) {
          return {.allowed = false,
                  .reason = SkipReason::kKeyboardNotVisibleOnAndroid};
        }
      }
      return {.allowed = true, .reason = SkipReason::kNone};
  }
  NOTREACHED();
}

// static
std::string_view OtpDeliveryPolicy::SkipReasonToStringView(SkipReason reason) {
  switch (reason) {
    case SkipReason::kNone:
      return "Allowed.";
    case SkipReason::kUserNotOptedIn:
      return "Reason: User has not opted into Gmail OTP filling.";
    case SkipReason::kOriginMismatch:
      return "Reason: Verified origin does not match target field origin.";
    case SkipReason::kWebOtpBlocked:
      return "Reason: OTP delivery is blocked due to the WebOTP API.";
    case SkipReason::kPhishingVerdict:
      return "Reason: PhishGuard verdict is phishing.";
    case SkipReason::kPhishGuardTimedOut:
      return "Reason: PhishGuard check timed out.";
    case SkipReason::kTargetFieldMissing:
      return "Reason: Target OTP field is missing or frame has changed.";
    case SkipReason::kFrameInactive:
      return "Reason: Target frame is inactive.";
    case SkipReason::kNonOtpFieldFocused:
      return "Reason: A non-OTP form field currently has focus.";
    case SkipReason::kTypedInputPresent:
      return "Reason: User has already typed into an OTP field.";
    case SkipReason::kProactiveUnfocusedOnAndroid:
      return "Reason: Proactive trigger on Android requires an OTP field to be "
             "focused so the Keyboard Accessory does not pop up unexpectedly.";
    case SkipReason::kKeyboardNotVisibleOnAndroid:
      return "Reason: Proactive trigger on Android requires the soft keyboard "
             "to be visible.";
  }
  NOTREACHED();
}

}  // namespace autofill
