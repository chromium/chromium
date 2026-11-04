// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_POLICY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_POLICY_H_

#include <string_view>

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"

namespace autofill {

// Stateless policy utility used by `OtpSecurityGateway` that centralizes which
// security and UX checks apply to each token source and delivery stage.
class OtpDeliveryPolicy {
 public:
  enum class Stage {
    // Evaluates whether a newly arrived OTP candidate may enter the verified
    // `OtpDelivery` cache.
    kVerification,
    // Evaluates whether a verified OTP candidate may proactively trigger
    // suggestions in the renderer.
    kProactiveTrigger,
  };

  enum class SkipReason {
    kNone,
    kUserNotOptedIn,
    kOriginMismatch,
    kWebOtpBlocked,
    kPhishingVerdict,
    kTargetFieldMissing,
    kFrameInactive,
    kNonOtpFieldFocused,
    kTypedInputPresent,
    kProactiveUnfocusedOnAndroid,
    kKeyboardNotVisibleOnAndroid,
  };

  struct Input {
    // All fields default to fail-closed values that block OTP delivery so an
    // omitted field fails the check rather than silently allowing delivery.
    one_time_tokens::OneTimeTokenType source =
        one_time_tokens::OneTimeTokenType::kGmail;
    Stage stage = Stage::kProactiveTrigger;
    bool user_opted_into_gmail = false;
    // True only when the candidate's verified origin is non-opaque and matches
    // the target field's origin.
    bool origin_matches = false;
    bool web_otp_used = true;
    bool target_field_exists = false;
    bool frame_active = false;
    bool has_non_otp_focus_anywhere = true;
    bool any_otp_field_has_typed_input = true;
    bool typed_in_any_field_after_detection = true;
    bool is_android = true;
    bool is_otp_field_focused = false;
    bool is_keyboard_visible = false;
    OneTimeTokensPhishGuardVerdict phish_guard_verdict =
        OneTimeTokensPhishGuardVerdict::kPhishing;
  };

  struct Decision {
    friend bool operator==(const Decision&, const Decision&) = default;

    bool allowed = false;
    SkipReason reason = SkipReason::kNone;
  };

  OtpDeliveryPolicy() = delete;

  // Evaluates `input` against the unified delivery policy table.
  static Decision Evaluate(const Input& input);

  // Returns a human-readable description of `reason` for logging to
  // chrome://autofill-internals.
  static std::string_view SkipReasonToStringView(SkipReason reason);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_POLICY_H_
