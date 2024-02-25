// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/signin/profile_separation_policies.h"

namespace policy {

ProfileSeparationPolicies::ProfileSeparationPolicies() = default;

ProfileSeparationPolicies::ProfileSeparationPolicies(
    int profile_separation_settings,
    std::optional<int> profile_separation_data_migration_settings)
    : profile_separation_settings_(profile_separation_settings),
      profile_separation_data_migration_settings_(
          std::move(profile_separation_data_migration_settings)) {}

ProfileSeparationPolicies::ProfileSeparationPolicies(
    const std::string& managed_accounts_signin_restrictions)
    : managed_accounts_signin_restrictions_(
          managed_accounts_signin_restrictions) {}

ProfileSeparationPolicies::ProfileSeparationPolicies(
    const ProfileSeparationPolicies& other) = default;

ProfileSeparationPolicies& ProfileSeparationPolicies::operator=(
    const ProfileSeparationPolicies& other) = default;

ProfileSeparationPolicies::~ProfileSeparationPolicies() = default;

bool ProfileSeparationPolicies::Empty() const {
  return !managed_accounts_signin_restrictions_ &&
         !profile_separation_settings_ &&
         !profile_separation_data_migration_settings_;
}

bool ProfileSeparationPolicies::Valid() const {
  return Empty() || (managed_accounts_signin_restrictions_.has_value() !=
                     (profile_separation_settings_.has_value() ||
                      profile_separation_data_migration_settings_.has_value()));
}

}  // namespace policy
