// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_MATCHER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_MATCHER_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "url/origin.h"

namespace url {
class SchemeHostPort;
}

namespace affiliations {
enum class MatchType;
class DomainRelationChecker;
}  // namespace affiliations

namespace one_time_tokens {

// The relation between the email sender's domain and the origin of the frame
// the OTP would be filled into. It describes both accepted and rejected
// senders.
// LINT.IfChange(GmailOtpSenderDomainMatchType)
enum class GmailOtpSenderDomainMatchType {
  kUnknown = 0,
  kNoMatch = 1,
  kGrouped = 2,
  kPsl = 3,
  kGroupedAndPsl = 4,
  kExact = 5,
  kAffiliated = 6,
  kFrameIsWwwPsl = 7,
  kMaxValue = kFrameIsWwwPsl
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:GmailOtpSenderDomainMatchType)

std::ostream& operator<<(std::ostream& os,
                         GmailOtpSenderDomainMatchType match_type);

// Determines how the domain of an email sender relates to the origin of the
// frame a One-Time Password (OTP) would be filled into.
//
// The frame origin is fixed for the lifetime of this object, which can answer
// that question for any number of senders, including concurrently. It never
// fetches an OTP itself, and it does not decide whether a given relation is
// good enough to fill the frame - that policy belongs to the caller.
// Destroying it cancels all in-flight checks and discards their callbacks.
class GmailOtpSenderDomainMatcher {
 public:
  using ResultCallback =
      base::OnceCallback<void(GmailOtpSenderDomainMatchType)>;

  // `domain_relation_checker` must be non-null.
  GmailOtpSenderDomainMatcher(
      std::unique_ptr<affiliations::DomainRelationChecker>
          domain_relation_checker,
      const url::Origin& otp_frame_origin);

  ~GmailOtpSenderDomainMatcher();

  GmailOtpSenderDomainMatcher(const GmailOtpSenderDomainMatcher&) = delete;
  GmailOtpSenderDomainMatcher& operator=(const GmailOtpSenderDomainMatcher&) =
      delete;

  // Determines how `sender_address` relates to the frame origin.
  // `sender_address` is a full email address (e.g. "no-reply@example.com"); an
  // address without a domain part never matches. `callback` may be invoked
  // synchronously. The frame origin must not be opaque, since no sender can
  // ever be related to it.
  void Check(std::string_view sender_address, ResultCallback callback);

 private:
  void OnDomainRelationChecked(
      const url::SchemeHostPort& sender_tuple,
      ResultCallback callback,
      std::optional<affiliations::MatchType> match_type);

  GmailOtpSenderDomainMatchType ResolveMatchType(
      std::optional<affiliations::MatchType> match_type,
      const url::SchemeHostPort& sender_tuple) const;

  const std::unique_ptr<affiliations::DomainRelationChecker>
      domain_relation_checker_;
  const url::Origin otp_frame_origin_;

  base::WeakPtrFactory<GmailOtpSenderDomainMatcher> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_MATCHER_H_
