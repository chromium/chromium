// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "url/origin.h"

namespace autofill {

OtpTarget::OtpTarget() = default;

OtpTarget::OtpTarget(url::Origin origin,
                     LocalFrameToken frame,
                     FormGlobalId form,
                     FieldGlobalId field,
                     LocalFrameToken form_owner)
    : origin(std::move(origin)),
      frame(frame),
      form(form),
      field(field),
      form_owner(form_owner) {}

OtpTarget::OtpTarget(const OtpTarget&) = default;
OtpTarget& OtpTarget::operator=(const OtpTarget&) = default;
OtpTarget::OtpTarget(OtpTarget&&) = default;
OtpTarget& OtpTarget::operator=(OtpTarget&&) = default;
OtpTarget::~OtpTarget() = default;

OtpTokenIdentity::OtpTokenIdentity() = default;

OtpTokenIdentity::OtpTokenIdentity(
    std::string value,
    std::optional<std::string> sender,
    std::optional<base::Time> email_received_timestamp,
    base::TimeTicks on_device_arrival)
    : value(std::move(value)),
      sender(std::move(sender)),
      email_received_timestamp(email_received_timestamp),
      on_device_arrival(on_device_arrival) {}

OtpTokenIdentity::OtpTokenIdentity(const OtpTokenIdentity&) = default;
OtpTokenIdentity& OtpTokenIdentity::operator=(const OtpTokenIdentity&) =
    default;
OtpTokenIdentity::OtpTokenIdentity(OtpTokenIdentity&&) = default;
OtpTokenIdentity& OtpTokenIdentity::operator=(OtpTokenIdentity&&) = default;
OtpTokenIdentity::~OtpTokenIdentity() = default;

// static
OtpTokenIdentity OtpTokenIdentity::FromOneTimeToken(
    const one_time_tokens::OneTimeToken& token) {
  return OtpTokenIdentity(token.value(), token.sender_address(),
                          token.email_received_timestamp(),
                          token.on_device_arrival_time());
}

bool OtpTokenIdentity::IsStrictlyNewerThan(
    const OtpTokenIdentity& other) const {
  // Two Gmail OTPs: the email that reached the server later wins.
  if (email_received_timestamp && other.email_received_timestamp &&
      *email_received_timestamp != *other.email_received_timestamp) {
    return *email_received_timestamp > *other.email_received_timestamp;
  }
  // SMS vs Gmail, SMS vs SMS, or equal email timestamps: the OTP that
  // reached the device later wins. An exact tie is not "newer".
  return on_device_arrival > other.on_device_arrival;
}

OtpCandidate::OtpCandidate(one_time_tokens::OneTimeToken token,
                           url::Origin verified_origin,
                           LocalFrameToken verified_frame)
    : identity(OtpTokenIdentity::FromOneTimeToken(token)),
      token(std::move(token)),
      verified_origin(std::move(verified_origin)),
      verified_frame(verified_frame) {}

OtpCandidate::OtpCandidate(const OtpCandidate&) = default;
OtpCandidate& OtpCandidate::operator=(const OtpCandidate&) = default;
OtpCandidate::OtpCandidate(OtpCandidate&&) = default;
OtpCandidate& OtpCandidate::operator=(OtpCandidate&&) = default;
OtpCandidate::~OtpCandidate() = default;

OtpSuggestionData::OtpSuggestionData(one_time_tokens::OneTimeToken token)
    : token(std::move(token)) {}

OtpSuggestionData::OtpSuggestionData(const OtpSuggestionData&) = default;
OtpSuggestionData& OtpSuggestionData::operator=(const OtpSuggestionData&) =
    default;
OtpSuggestionData::OtpSuggestionData(OtpSuggestionData&&) = default;
OtpSuggestionData& OtpSuggestionData::operator=(OtpSuggestionData&&) = default;
OtpSuggestionData::~OtpSuggestionData() = default;

std::string_view OneTimeTokensPhishGuardVerdictToStringView(
    OneTimeTokensPhishGuardVerdict verdict) {
  switch (verdict) {
    case OneTimeTokensPhishGuardVerdict::kUnknown:
      return "kUnknown";
    case OneTimeTokensPhishGuardVerdict::kPhishing:
      return "kPhishing";
    case OneTimeTokensPhishGuardVerdict::kNotPhishing:
      return "kNotPhishing";
  }
  NOTREACHED();
}

}  // namespace autofill
