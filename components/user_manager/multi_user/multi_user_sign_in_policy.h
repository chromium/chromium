// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
#define COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_

#include <string_view>

#include "components/user_manager/user_manager_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {

enum class MultiUserSignInPolicy {
  kUnrestricted = 0,
  kPrimaryOnly = 1,
  kNotAllowed = 2,
};

// A string pref that holds string enum values of how the user should behave
// in a multi-user sign-in session. See ChromeOsMultiProfileUserBehavior policy
// for more details of the valid values.
// Named MultiProfile for historical reasons.
inline constexpr char kMultiProfileUserBehaviorPref[] =
    "settings.multiprofile_user_behavior";

// Stringifies the policy to store in the pref.
USER_MANAGER_EXPORT std::string_view MultiUserSignInPolicyToPrefValue(
    MultiUserSignInPolicy policy);

// Parses the pref stored string into enum value. Returns nullopt on failure.
USER_MANAGER_EXPORT absl::optional<MultiUserSignInPolicy>
ParseMultiUserSignInPolicyPref(std::string_view s);

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
