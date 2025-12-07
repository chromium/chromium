// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/sms_otp_backend.h"

namespace one_time_tokens {

OtpFetchReply::OtpFetchReply(std::optional<OneTimeToken> otp_value,
                             bool request_complete)
    : otp_value(std::move(otp_value)), request_complete(request_complete) {}

OtpFetchReply::OtpFetchReply(const OtpFetchReply&) = default;
OtpFetchReply& OtpFetchReply::operator=(const OtpFetchReply&) = default;

OtpFetchReply::~OtpFetchReply() = default;

SmsOtpBackend::~SmsOtpBackend() = default;

}  // namespace one_time_tokens
