// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"

namespace user_manager {

namespace {
constexpr auto kPolicyMap =
    base::MakeFixedFlatMap<MultiUserSignInPolicy, std::string_view>({
        {MultiUserSignInPolicy::kUnrestricted, "restricted"},
        {MultiUserSignInPolicy::kPrimaryOnly, "primary-only"},
        {MultiUserSignInPolicy::kNotAllowed, "not-allowed"},
    });
}  // namespace

std::string_view MultiUserSignInPolicyToPrefValue(
    MultiUserSignInPolicy policy) {
  auto* it = kPolicyMap.find(policy);
  CHECK_NE(it, kPolicyMap.end());
  return it->second;
}

absl::optional<MultiUserSignInPolicy> ParseMultiUserSignInPolicyPref(
    std::string_view s) {
  for (const auto& entry : kPolicyMap) {
    if (s == entry.second) {
      return entry.first;
    }
  }
  return absl::nullopt;
}

}  // namespace user_manager
