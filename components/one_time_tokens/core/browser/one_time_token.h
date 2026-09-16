// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"

namespace one_time_tokens {

class OneTimeToken {
 public:
  static constexpr int kMinOtpLength = 4;
  static constexpr int kMaxOtpLength = 6;

  // Returns true if `value` has the format of a potential OTP (i.e. it consists
  // only of digits and its length is between `kMinOtpLength` and
  // `kMaxOtpLength`).
  static bool IsPotentialOtp(std::u16string_view value);

  OneTimeToken(
      OneTimeTokenType type,
      const std::string& value,
      base::TimeTicks on_device_arrival_time,
      std::optional<std::string> sender_address = std::nullopt,
      std::optional<base::Time> email_received_timestamp = std::nullopt);
  OneTimeToken(const OneTimeToken&);
  OneTimeToken& operator=(const OneTimeToken&);
  OneTimeToken(OneTimeToken&&);
  OneTimeToken& operator=(OneTimeToken&&);
  ~OneTimeToken();

  [[nodiscard]] OneTimeTokenType type() const { return type_; }
  [[nodiscard]] const std::string& value() const { return value_; }
  [[nodiscard]] base::TimeTicks on_device_arrival_time() const {
    return on_device_arrival_time_;
  }
  [[nodiscard]] const std::optional<std::string>& sender_address() const {
    return sender_address_;
  }
  [[nodiscard]] const std::optional<base::Time>& email_received_timestamp()
      const {
    return email_received_timestamp_;
  }

 private:
  OneTimeTokenType type_;
  std::string value_;
  base::TimeTicks on_device_arrival_time_;

  // The sender of the OTP email. This is only relevant for Gmail OTPs
  // and is `std::nullopt` otherwise.
  std::optional<std::string> sender_address_;

  // The timestamp when the OTP email was received on the server. This is only
  // relevant for Gmail OTPs and is `std::nullopt` otherwise. Since this is
  // server time, only use it for comparisons with other
  // `email_received_timestamp_` values, not to compute time elapsed on device.
  std::optional<base::Time> email_received_timestamp_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_
