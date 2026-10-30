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
enum class OneTimeTokensPhishGuardVerdict {
  kUnknown = 0,
  kPhishing = 1,
  kNotPhishing = 2,
  kMaxValue = kNotPhishing,
};

std::string_view OneTimeTokensPhishGuardVerdictToStringView(
    OneTimeTokensPhishGuardVerdict verdict);

// Explicit data lifecycle state per target OTP field. Screen states (pending,
// shown, dismissed) are owned by `OtpSuggestionDropdownDispatcher`.
enum class DeliveryState {
  kListening,
  kAvailable,
  kFieldGone,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TYPES_H_
