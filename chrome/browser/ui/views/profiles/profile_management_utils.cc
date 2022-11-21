// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_utils.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr base::TimeDelta kDefaultExtendedAccountInfoTimeout =
    base::Seconds(10);

absl::optional<base::TimeDelta> g_extended_account_info_timeout_for_testing =
    absl::nullopt;

}  // namespace

// -- Helper functions ---------------------------------------------------------

void FinalizeNewProfileSetup(Profile* profile,
                             const std::u16string& profile_name) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  CHECK(entry);

  entry->SetIsOmitted(false);
  if (!profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Unmark this profile ephemeral so that it isn't deleted upon next startup.
    // Profiles should never be made non-ephemeral if ephemeral mode is forced
    // by policy.
    entry->SetIsEphemeral(false);
  }
  entry->SetLocalProfileName(profile_name,
                             /*is_default_name=*/false);

  // Skip the welcome page for this profile as we already showed a profile setup
  // experience.
  profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);
}

// -- ProfileNameResolver ------------------------------------------------------

// static
ProfileNameResolver::ScopedInfoFetchTimeoutOverride
ProfileNameResolver::CreateScopedInfoFetchTimeoutOverrideForTesting(
    base::TimeDelta timeout) {
  return base::AutoReset<absl::optional<base::TimeDelta>>(
      &g_extended_account_info_timeout_for_testing, timeout);
}

ProfileNameResolver::ProfileNameResolver(
    signin::IdentityManager* identity_manager) {
  // Listen for extended account info getting fetched.
  identity_manager_observation_.Observe(identity_manager);

  // Set up a timeout for extended account info.
  std::u16string fallback_profile_name =
      profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
  extended_account_info_timeout_closure_.Reset(
      base::BindOnce(&ProfileNameResolver::OnProfileNameResolved,
                     weak_ptr_factory_.GetWeakPtr(), fallback_profile_name));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, extended_account_info_timeout_closure_.callback(),
      g_extended_account_info_timeout_for_testing.value_or(
          kDefaultExtendedAccountInfoTimeout));
}

ProfileNameResolver::~ProfileNameResolver() = default;

void ProfileNameResolver::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (!account_info.IsValid())
    return;

  OnProfileNameResolved(
      profiles::GetDefaultNameForNewSignedInProfile(account_info));
}

void ProfileNameResolver::OnProfileNameResolved(
    const std::u16string& profile_name) {
  DCHECK(!profile_name.empty());
  DCHECK(resolved_profile_name_.empty());  // Should be resolved more than once.

  // Cancel timeout and stop listening to further changes.
  extended_account_info_timeout_closure_.Cancel();
  identity_manager_observation_.Reset();

  resolved_profile_name_ = profile_name;
  if (on_profile_name_resolved_callback_)
    std::move(on_profile_name_resolved_callback_).Run();
}
