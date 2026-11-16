// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TYPES_H_

// Shared value types and enums passed across the OTP autofill pipeline
// (`OtpSource`, `OtpSecurityGateway`, `OtpDelivery`, `OtpCoordinator`, and
// `OtpSuggestionProvider`). Only passive data structures and enums representing
// OTP targets, candidates, delivery states, and security verdicts belong here;
// stateful orchestration or business logic belongs in the respective component
// headers.

#include <optional>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "url/origin.h"

namespace autofill {

// Identifies a target OTP field, its hosting frame, its origin, and the frame
// whose `BrowserAutofillManager` owns the flattened form.
struct OtpTarget {
  OtpTarget();
  OtpTarget(url::Origin origin,
            LocalFrameToken frame,
            FormGlobalId form,
            FieldGlobalId field,
            LocalFrameToken form_owner);
  OtpTarget(const OtpTarget&);
  OtpTarget& operator=(const OtpTarget&);
  OtpTarget(OtpTarget&&);
  OtpTarget& operator=(OtpTarget&&);
  ~OtpTarget();

  friend bool operator==(const OtpTarget&, const OtpTarget&) = default;

  // Origin of the frame hosting the field. Can be opaque; opaque targets never
  // receive Gmail OTPs.
  url::Origin origin;
  // Frame hosting the field (used for PhishGuard and delivery verification).
  LocalFrameToken frame;
  // Form owning the field.
  FormGlobalId form;
  // Target OTP field ID.
  FieldGlobalId field;
  // Frame whose `BrowserAutofillManager` owns the flattened form and receives
  // `RendererShouldTriggerSuggestions()`.
  LocalFrameToken form_owner;
};

// Duration for which an arrived Gmail OTP remains valid in the Autofill layer.
inline constexpr base::TimeDelta kGmailOtpCacheDuration = base::Minutes(3);

// Uniquely identifies an OTP candidate and establishes chronological ordering
// across resends.
struct OtpTokenIdentity {
  OtpTokenIdentity();
  OtpTokenIdentity(std::string value,
                   std::optional<std::string> sender,
                   std::optional<base::Time> email_received_timestamp,
                   base::TimeTicks on_device_arrival);
  OtpTokenIdentity(const OtpTokenIdentity&);
  OtpTokenIdentity& operator=(const OtpTokenIdentity&);
  OtpTokenIdentity(OtpTokenIdentity&&);
  OtpTokenIdentity& operator=(OtpTokenIdentity&&);
  ~OtpTokenIdentity();

  static OtpTokenIdentity FromOneTimeToken(
      const one_time_tokens::OneTimeToken& token);

  // Returns true if `this` was received strictly later than `other`
  // (timestamp-only comparison where ties return false, regardless of token
  // value or sender).
  bool IsStrictlyNewerThan(const OtpTokenIdentity& other) const;

  // Returns the cache TTL for this token identity.
  base::TimeDelta CacheDuration() const;

  friend bool operator==(const OtpTokenIdentity&,
                         const OtpTokenIdentity&) = default;

  std::string value;
  std::optional<std::string> sender;
  // Server-side email receipt timestamp (ordering key for Gmail OTPs).
  std::optional<base::Time> email_received_timestamp;
  // On-device arrival timestamp (ordering key for SMS OTPs and fallback).
  base::TimeTicks on_device_arrival;
};

// Represents an OTP candidate produced by an `OtpSource` and managed by
// `OtpCoordinator`.
struct OtpCandidate {
  OtpCandidate(one_time_tokens::OneTimeToken token,
               url::Origin verified_origin,
               LocalFrameToken verified_frame);
  OtpCandidate(const OtpCandidate&);
  OtpCandidate& operator=(const OtpCandidate&);
  OtpCandidate(OtpCandidate&&);
  OtpCandidate& operator=(OtpCandidate&&);
  ~OtpCandidate();

  // Declared before `token` so the constructor can initialize `identity` from
  // the `token` parameter before moving it into `this->token`.
  OtpTokenIdentity identity;
  one_time_tokens::OneTimeToken token;
  url::Origin verified_origin;
  LocalFrameToken verified_frame;
  // True once the OTP has been filled or its form has been submitted.
  bool consumed = false;
};

// Lightweight suggestion data exposed to `OtpSuggestionProvider` and BAM.
struct OtpSuggestionData {
  explicit OtpSuggestionData(one_time_tokens::OneTimeToken token);
  OtpSuggestionData(const OtpSuggestionData&);
  OtpSuggestionData& operator=(const OtpSuggestionData&);
  OtpSuggestionData(OtpSuggestionData&&);
  OtpSuggestionData& operator=(OtpSuggestionData&&);
  ~OtpSuggestionData();

  const std::string& value() const LIFETIME_BOUND { return token.value(); }
  one_time_tokens::OneTimeTokenType source() const { return token.type(); }
  const std::optional<std::string>& sender_for_display() const LIFETIME_BOUND {
    return token.sender_address();
  }

  one_time_tokens::OneTimeToken token;
};

// Used for histograms. Do not reorder.
// LINT.IfChange(OneTimeTokensPhishGuardVerdict)
enum class OneTimeTokensPhishGuardVerdict {
  kUnknown = 0,
  kPhishing = 1,
  kNotPhishing = 2,
  kTimedOut = 3,
  kMaxValue = kTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:OneTimeTokensPhishGuardVerdict)

std::string_view OneTimeTokensPhishGuardVerdictToStringView(
    OneTimeTokensPhishGuardVerdict verdict);

// Explicit data lifecycle state per target OTP field. Screen states (pending,
// shown, dismissed) are owned by `OtpSuggestionDropdownDispatcher`.
enum class DeliveryState {
  kListening,
  kAvailable,
  kFieldGone,
};

// Outcome of evaluating a proactive OTP suggestion popup trigger on a field.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OtpProactiveTriggerOutcome)
enum class OtpProactiveTriggerOutcome {
  kSuppressed = 0,
  kTriggeredAndSuggestionsShown = 1,
  kMaxValue = kTriggeredAndSuggestionsShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:OtpProactiveTriggerOutcome)

// Reason a proactive OTP suggestion popup trigger was suppressed after passing
// verification-stage delivery gates.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OtpProactiveTriggerSuppressionReason)
enum class OtpProactiveTriggerSuppressionReason {
  kAlreadyOfferedOrOlderToken = 0,
  kNonOtpFieldFocused = 1,
  kTypedInputPresent = 2,
  kDifferentOtpFieldFocused = 3,
  kProactiveUnfocusedOnAndroid = 4,
  // Not recorded until Android soft-keyboard visibility is plumbed into
  // `OtpSecurityGateway::ProactiveTriggerContext` (crbug.com/451991285).
  kKeyboardNotVisibleOnAndroid = 5,
  kTargetFieldMissing = 6,
  // Currently never recorded: `OtpCoordinator` passes `frame_active = true`
  // because `IsTargetActiveAndEligible()` already excludes inactive frames.
  kFrameInactive = 7,
  // Edge case: the document started using WebOTP after the SMS candidate was
  // verified, e.g. while its proactive trigger was deferred.
  kWebOtpBlocked = 8,
  // Edge case: the user turned off Gmail OTP filling after the candidate was
  // verified, e.g. while its proactive trigger was deferred.
  kUserNotOptedIn = 9,
  kMaxValue = kUserNotOptedIn,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:OtpProactiveTriggerSuppressionReason)

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TYPES_H_
