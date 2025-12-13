// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_

#include <string>

#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"

namespace one_time_tokens {

class OneTimeToken {
 public:
  OneTimeToken(OneTimeTokenType type,
               const std::string& value,
               base::Time on_device_arrival_time);
  OneTimeToken(const OneTimeToken&);
  OneTimeToken& operator=(const OneTimeToken&);
  OneTimeToken(OneTimeToken&&);
  OneTimeToken& operator=(OneTimeToken&&);
  ~OneTimeToken();

  [[nodiscard]] OneTimeTokenType type() const { return type_; }
  [[nodiscard]] const std::string& value() const { return value_; }
  [[nodiscard]] base::Time on_device_arrival_time() const {
    return on_device_arrival_time_;
  }

  // Performs a comparison of OneTimeTokens, ignoring the
  // `on_device_arrival_time_`.
  bool operator==(const OneTimeToken& other) const;
  bool operator!=(const OneTimeToken& other) const;

 private:
  OneTimeTokenType type_;
  std::string value_;
  base::Time on_device_arrival_time_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_H_
