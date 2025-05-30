// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"

namespace password_manager {

OtpFetchReply::OtpFetchReply(std::optional<std::string> otp_value,
                             bool request_complete)
    : otp_value(otp_value), request_complete(request_complete) {}

OtpFetchReply::OtpFetchReply(const OtpFetchReply&) = default;
OtpFetchReply& OtpFetchReply::operator=(const OtpFetchReply&) = default;

OtpFetchReply::~OtpFetchReply() = default;

}  // namespace password_manager
