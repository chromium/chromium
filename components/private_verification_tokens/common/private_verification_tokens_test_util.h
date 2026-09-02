// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"

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

inline std::optional<PrivacyPassAthmIssuer> CreateTestIssuer(
    uint8_t num_buckets,
    std::string_view deployment_id) {
  auto res = PrivacyPassAthmIssuer::try_new(
      num_buckets,
      rs_std::SliceRef<const uint8_t>(base::as_byte_span(deployment_id)));
  if (res.has_value()) {
    return std::move(*res);
  }
  return std::nullopt;
}

}  // namespace private_verification_tokens::test

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TEST_UTIL_H_
