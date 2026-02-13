// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace one_time_tokens {

// static
bool OneTimeToken::IsPotentialOtp(std::u16string_view value) {
  return value.length() >= kMinOtpLength && value.length() <= kMaxOtpLength &&
         std::ranges::all_of(value,
                             [](char16_t c) { return base::IsAsciiDigit(c); });
}

OneTimeToken::OneTimeToken(OneTimeTokenType type,
                           const std::string& value,
                           base::Time on_device_arrival_time)
    : type_(type),
      value_(value),
      on_device_arrival_time_(on_device_arrival_time) {}

OneTimeToken::OneTimeToken(const OneTimeToken&) = default;
OneTimeToken& OneTimeToken::operator=(const OneTimeToken&) = default;

OneTimeToken::OneTimeToken(OneTimeToken&&) = default;
OneTimeToken& OneTimeToken::operator=(OneTimeToken&&) = default;

OneTimeToken::~OneTimeToken() = default;

bool OneTimeToken::operator==(const OneTimeToken& other) const {
  return type_ == other.type_ && value_ == other.value_;
}

bool OneTimeToken::operator!=(const OneTimeToken& other) const {
  return !(*this == other);
}

}  // namespace one_time_tokens
