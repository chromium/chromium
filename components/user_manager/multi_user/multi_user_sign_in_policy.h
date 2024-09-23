// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
#define COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_

#include <optional>
#include <string_view>

#include "components/user_manager/user_manager_export.h"

namespace user_manager {

class User;

enum class MultiUserSignInPolicy {
  // The user is allowed to be either a primary user or secondary user in
  // multi user sign-in sessions.
  kUnrestricted = 0,

  // The user can be only be a primary user in multi user sign-in sessions.
  kPrimaryOnly = 1,

  // The user cannot be a part of multi user sign-in sessions.
  kNotAllowed = 2,
};

// Stringifies the policy to store in the pref.
USER_MANAGER_EXPORT std::string_view MultiUserSignInPolicyToPrefValue(
    MultiUserSignInPolicy policy);

// Parses the pref stored string into enum value. Returns nullopt on failure.
USER_MANAGER_EXPORT std::optional<MultiUserSignInPolicy>
ParseMultiUserSignInPolicyPref(std::string_view s);

// Returns the policy for the given User. If the pref is not available
// returns std::nullopt.
USER_MANAGER_EXPORT std::optional<MultiUserSignInPolicy>
GetMultiUserSignInPolicy(const User* user);

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
