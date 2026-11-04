// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_security_gateway.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"

namespace autofill {

namespace {

constexpr bool kIsAndroid =
#if BUILDFLAG(IS_ANDROID)
    true;
#else
    false;
#endif

// Returns true only when `candidate.verified_origin` is non-opaque and matches
// `target.origin`.
bool OriginsMatch(const OtpTarget& target, const OtpCandidate& candidate) {
  return !candidate.verified_origin.opaque() &&
         candidate.verified_origin == target.origin;
}

// Returns true when an SMS candidate carries a non-opaque `verified_origin`
// that does not match `target.origin` (matching `OtpDelivery::CanAccept()`).
// Unbound SMS candidates carry an opaque `verified_origin` and return false.
bool HasMismatchedBoundSmsOrigin(const OtpTarget& target,
                                 const OtpCandidate& candidate) {
  return candidate.token.type() == one_time_tokens::OneTimeTokenType::kSmsOtp &&
         !candidate.verified_origin.opaque() &&
         candidate.verified_origin != target.origin;
}

// Substitutes `kNotPhishing` for synchronous post-verification checks only when
// `candidate` was validated for `target.frame`; fails closed with `kPhishing`
// otherwise.
OneTimeTokensPhishGuardVerdict EffectiveVerdictForVerifiedCandidate(
    const OtpTarget& target,
    const OtpCandidate& candidate) {
  return candidate.verified_frame == target.frame
             ? OneTimeTokensPhishGuardVerdict::kNotPhishing
             : OneTimeTokensPhishGuardVerdict::kPhishing;
}

void LogSuppressedDecision(LogManager* log_manager,
                           OtpDeliveryPolicy::SkipReason reason) {
  LOG_AF(log_manager) << LoggingScope::kOneTimeTokens
                      << LogMessage::kSuggestionSuppressed << " "
                      << OtpDeliveryPolicy::SkipReasonToStringView(reason);
}

}  // namespace

OtpSecurityGateway::OtpSecurityGateway(AutofillClient& client)
    : client_(client), phish_guard_gateway_(client) {}

OtpSecurityGateway::~OtpSecurityGateway() = default;

void OtpSecurityGateway::ValidateDelivery(OtpTarget target,
                                          OtpCandidate candidate,
                                          ValidationCallback callback) {
  // Fast-fail synchronous policy preconditions (Gmail consent, origin match,
  // WebOTP) before starting an asynchronous PhishGuard check. `kNotPhishing` is
  // passed only for this synchronous pre-check; the actual PhishGuard verdict
  // is evaluated in `OnPhishGuardVerdict`.
  if (OtpDeliveryPolicy::Decision precheck = EvaluateVerificationPolicy(
          target, candidate, OneTimeTokensPhishGuardVerdict::kNotPhishing);
      !precheck.allowed) {
    candidate.verified_frame = LocalFrameToken();
    std::move(callback).Run(std::move(target), std::move(candidate), precheck);
    return;
  }

  const LocalFrameToken frame = target.frame;
  phish_guard_gateway_.Check(
      frame, base::BindOnce(&OtpSecurityGateway::OnPhishGuardVerdict,
                            weak_ptr_factory_.GetWeakPtr(), std::move(target),
                            std::move(candidate), std::move(callback)));
}

OtpDeliveryPolicy::Decision OtpSecurityGateway::EvaluateProactiveTrigger(
    const OtpTarget& target,
    const OtpCandidate& candidate,
    const ProactiveTriggerContext& context) const {
  // `EvaluateProactiveTrigger` is invoked synchronously for candidates stored
  // in `OtpDelivery` that already passed asynchronous PhishGuard verification
  // in `ValidateDelivery` (phishing candidates are rejected there and never
  // admitted to `OtpDelivery`, and `OtpCandidate` does not store the raw
  // `OneTimeTokensPhishGuardVerdict`). `EffectiveVerdictForVerifiedCandidate`
  // supplies `kNotPhishing` only when `candidate.verified_frame ==
  // target.frame`, failing closed with `kPhishing` otherwise.
  const bool web_otp_used = client_->DocumentUsedWebOTP();
  OtpDeliveryPolicy::Decision decision =
      (!web_otp_used && HasMismatchedBoundSmsOrigin(target, candidate))
          ? OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
            }
          : OtpDeliveryPolicy::Evaluate({
                .source = candidate.token.type(),
                .stage = OtpDeliveryPolicy::Stage::kProactiveTrigger,
                .user_opted_into_gmail = UserOptedIntoGmailOtpFilling(),
                .origin_matches = OriginsMatch(target, candidate),
                .web_otp_used = web_otp_used,
                .target_field_exists = context.target_field_exists,
                .frame_active = context.frame_active,
                .has_non_otp_focus_anywhere =
                    context.has_non_otp_focus_anywhere,
                .any_otp_field_has_typed_input =
                    context.any_otp_field_has_typed_input,
                .typed_in_any_field_after_detection =
                    context.typed_in_any_field_after_detection,
                .is_android = kIsAndroid,
                .is_otp_field_focused = context.is_otp_field_focused,
                .is_keyboard_visible = context.is_keyboard_visible,
                .phish_guard_verdict =
                    EffectiveVerdictForVerifiedCandidate(target, candidate),
            });
  if (!decision.allowed) {
    LogSuppressedDecision(client_->GetCurrentLogManager(), decision.reason);
  }
  return decision;
}

OtpDeliveryPolicy::Decision OtpSecurityGateway::EvaluateReactiveServing(
    const OtpTarget& target,
    const OtpCandidate& candidate) const {
  // As in `EvaluateProactiveTrigger`, `EvaluateReactiveServing` is invoked
  // synchronously for candidates from `OtpDelivery` that already passed
  // PhishGuard verification in `ValidateDelivery`.
  // `EffectiveVerdictForVerifiedCandidate` supplies `kNotPhishing` only when
  // `candidate.verified_frame == target.frame` (failing closed otherwise) while
  // re-evaluating live consent, origin match, and WebOTP usage.
  return EvaluateVerificationPolicy(
      target, candidate,
      EffectiveVerdictForVerifiedCandidate(target, candidate));
}

void OtpSecurityGateway::OnPhishGuardVerdict(
    OtpTarget target,
    OtpCandidate candidate,
    ValidationCallback callback,
    OneTimeTokensPhishGuardVerdict verdict) {
  OtpDeliveryPolicy::Decision decision =
      EvaluateVerificationPolicy(target, candidate, verdict);
  if (decision.allowed) {
    if (candidate.token.type() == one_time_tokens::OneTimeTokenType::kSmsOtp &&
        candidate.verified_origin.opaque()) {
      candidate.verified_origin = target.origin;
    }
    candidate.verified_frame = target.frame;
  } else {
    candidate.verified_frame = LocalFrameToken();
  }
  std::move(callback).Run(std::move(target), std::move(candidate), decision);
}

OtpDeliveryPolicy::Decision OtpSecurityGateway::EvaluateVerificationPolicy(
    const OtpTarget& target,
    const OtpCandidate& candidate,
    OneTimeTokensPhishGuardVerdict verdict) const {
  const bool web_otp_used = client_->DocumentUsedWebOTP();
  OtpDeliveryPolicy::Decision decision =
      (!web_otp_used && HasMismatchedBoundSmsOrigin(target, candidate))
          ? OtpDeliveryPolicy::Decision{
                .allowed = false,
                .reason = OtpDeliveryPolicy::SkipReason::kOriginMismatch,
            }
          : OtpDeliveryPolicy::Evaluate({
                .source = candidate.token.type(),
                .stage = OtpDeliveryPolicy::Stage::kVerification,
                .user_opted_into_gmail = UserOptedIntoGmailOtpFilling(),
                .origin_matches = OriginsMatch(target, candidate),
                .web_otp_used = web_otp_used,
                .phish_guard_verdict = verdict,
            });
  if (!decision.allowed) {
    LogSuppressedDecision(client_->GetCurrentLogManager(), decision.reason);
  }
  return decision;
}

bool OtpSecurityGateway::UserOptedIntoGmailOtpFilling() const {
  PrefService* prefs = client_->GetPrefs();
  return prefs && prefs::IsAutofillGmailOtpFillingEnabled(prefs);
}

}  // namespace autofill
