// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace private_verification_tokens::test {

struct FutureExpiration {
  base::Time time;
  std::string string_rep;
};

inline FutureExpiration GetFutureExpiration(
    base::TimeDelta delta = base::Days(30)) {
  const base::TimeDelta future_seconds = base::Seconds(
      (base::Time::Now() + delta - base::Time::UnixEpoch()).InSeconds());
  return {base::Time::UnixEpoch() + future_seconds,
          base::NumberToString(future_seconds.InSeconds())};
}

}  // namespace private_verification_tokens::test

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_
