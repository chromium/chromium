// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery.h"

#include <utility>

#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "url/origin.h"

namespace autofill {
namespace {

bool IsExpired(const OtpCandidate& candidate) {
  const base::TimeTicks arrival = candidate.token.on_device_arrival_time();
  return arrival.is_null() ||
         base::TimeTicks::Now() - arrival > candidate.identity.CacheDuration();
}

}  // namespace

OtpDelivery::OtpDelivery(OtpTarget target) : target_(std::move(target)) {}

OtpDelivery::OtpDelivery(const OtpDelivery&) = default;
OtpDelivery& OtpDelivery::operator=(const OtpDelivery&) = default;
OtpDelivery::OtpDelivery(OtpDelivery&&) noexcept = default;
OtpDelivery& OtpDelivery::operator=(OtpDelivery&&) noexcept = default;
OtpDelivery::~OtpDelivery() = default;

DeliveryState OtpDelivery::GetState() const {
  if (field_gone_) {
    return DeliveryState::kFieldGone;
  }
  return HasAvailableCandidate() ? DeliveryState::kAvailable
                                 : DeliveryState::kListening;
}

std::optional<OtpTokenIdentity> OtpDelivery::GetNewestIdentity() const {
  if (!newest_verified_.has_value()) {
    return std::nullopt;
  }
  return newest_verified_->identity;
}

bool OtpDelivery::CanAccept(const OtpCandidate& candidate) const {
  if (field_gone_ || blocked_by_phishing_ || candidate.consumed ||
      candidate.token.value().empty() || IsExpired(candidate)) {
    return false;
  }
  if (candidate.token.type() == one_time_tokens::OneTimeTokenType::kGmail) {
    if (candidate.verified_origin.opaque() ||
        candidate.verified_origin != target_.origin) {
      return false;
    }
  } else if (!candidate.verified_origin.opaque() &&
             candidate.verified_origin != target_.origin) {
    return false;
  }
  if (newest_verified_.has_value()) {
    return candidate.identity.IsStrictlyNewerThan(newest_verified_->identity);
  }
  return true;
}

bool OtpDelivery::Offer(OtpCandidate candidate) {
  if (!CanAccept(candidate)) {
    return false;
  }
  newest_verified_ = std::move(candidate);
  return true;
}

std::optional<OtpCandidate> OtpDelivery::GetAvailableCandidate() const {
  if (!HasAvailableCandidate()) {
    return std::nullopt;
  }
  return newest_verified_;
}

bool OtpDelivery::HasAvailableCandidate() const {
  return !field_gone_ && !blocked_by_phishing_ &&
         newest_verified_.has_value() && !newest_verified_->consumed &&
         !IsExpired(*newest_verified_);
}

std::optional<OtpTokenIdentity> OtpDelivery::MarkConsumed() {
  if (!newest_verified_.has_value()) {
    return std::nullopt;
  }
  newest_verified_->consumed = true;
  return newest_verified_->identity;
}

void OtpDelivery::MarkFieldGone() {
  newest_verified_.reset();
  field_gone_ = true;
}

void OtpDelivery::BlockOnPhishing() {
  newest_verified_.reset();
  blocked_by_phishing_ = true;
}

}  // namespace autofill
