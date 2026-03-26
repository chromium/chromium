// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_PROFILE_SEPARATION_POLICIES_H_
#define COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_PROFILE_SEPARATION_POLICIES_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "components/policy/policy_export.h"

namespace policy {

enum ProfileSeparationSettings : uint16_t {
  SUGGESTED = 0,
  ENFORCED = 1,
  DISABLED = 2
};

enum ProfileSeparationDataMigrationSettings : uint16_t {
  // Let users decide to bring existing browsing data into their managed
  // profile.
  USER_OPT_IN = 1,
  // Suggest to users to bring their existing data in the managed profile and
  // give them a choice not to.
  USER_OPT_OUT = 2,
  // Users cannot bring existing browsing data in their managed profile.
  ALWAYS_SEPARATE = 3
};

class POLICY_EXPORT ProfileSeparationPolicies {
 public:
  ProfileSeparationPolicies();
  ProfileSeparationPolicies(
      int profile_separation_settings,
      std::optional<int> profile_separation_data_migration_settings);
  explicit ProfileSeparationPolicies(
      const std::string& managed_accounts_signin_restrictions);
  ProfileSeparationPolicies(const ProfileSeparationPolicies&);
  ProfileSeparationPolicies& operator=(const ProfileSeparationPolicies&);
  ~ProfileSeparationPolicies();

  bool Empty() const;

  // Returns true if there is no conflict between legacy and newer policy
  // values. If `managed_accounts_signin_restrictions_` is set, both
  // `profile_separation_settings_` and
  // `profile_separation_data_migration_settings_` should not be set and vice
  // versa.
  bool Valid() const;

  const std::optional<std::string>& managed_accounts_signin_restrictions()
      const {
    return managed_accounts_signin_restrictions_;
  }
  std::optional<int> profile_separation_settings() const {
    return profile_separation_settings_;
  }
  std::optional<int> profile_separation_data_migration_settings() const {
    return profile_separation_data_migration_settings_;
  }

 private:
  std::optional<std::string> managed_accounts_signin_restrictions_;
  std::optional<int> profile_separation_settings_;
  std::optional<int> profile_separation_data_migration_settings_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_PROFILE_SEPARATION_POLICIES_H_
