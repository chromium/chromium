// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_customization_util.h"

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

constexpr base::TimeDelta kDefaultExtendedAccountInfoTimeout =
    base::Seconds(10);

std::optional<base::TimeDelta> g_extended_account_info_timeout_for_testing =
    std::nullopt;

}  // namespace

// -- Helper functions ---------------------------------------------------------

void FinalizeNewProfileSetup(Profile* profile,
                             const std::u16string& profile_name,
                             bool is_default_name) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  CHECK(entry);
  CHECK(!profile_name.empty());

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // We don't expect this to be run for profiles where the user already had a
  // chance to set a custom profile name. One exception might be post-migration
  // Lacros silent first run, see crbug.com/1443026.
  DCHECK(entry->IsUsingDefaultName());
#endif

  entry->SetLocalProfileName(profile_name, is_default_name);

  if (signin_util::IsForceSigninEnabled() &&
      base::FeatureList::IsEnabled(kForceSigninFlowInProfilePicker)) {
    // Managed accounts do not need to have Sync consent set.
    // TODO(crbug.com/40280466): Align Managed and Consumer accounts.
    if (!entry->CanBeManaged()) {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync))
          << "A non syncing account should not be able to finalize the "
             "profile.";
    }
    entry->LockForceSigninProfile(/*is_lock=*/false);
  }

  if (!entry->IsOmitted()) {
    // The profile has already been created outside of the classic "profile
    // creation" flow and did not start as omitted. The rest of the finalization
    // is not necessary.
    // TODO(crbug.com/40264199): Improve the API to clarify this inconsistency.
    return;
  }

  entry->SetIsOmitted(false);

  if (!profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Unmark this profile ephemeral so that it isn't deleted upon next
    // startup. Profiles should never be made non-ephemeral if ephemeral mode
    // is forced by policy.
    entry->SetIsEphemeral(false);
  }
}

// -- ProfileNameResolver ------------------------------------------------------

// static
ProfileNameResolver::ScopedInfoFetchTimeoutOverride
ProfileNameResolver::CreateScopedInfoFetchTimeoutOverrideForTesting(
    base::TimeDelta timeout) {
  return base::AutoReset<std::optional<base::TimeDelta>>(
      &g_extended_account_info_timeout_for_testing, timeout);
}

ProfileNameResolver::ProfileNameResolver(
    signin::IdentityManager* identity_manager,
    const CoreAccountInfo& core_account_info)
    : core_account_info_(core_account_info) {
  CHECK(!core_account_info_.IsEmpty());

  auto extended_account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info_);
  if (extended_account_info.IsValid()) {
    OnExtendedAccountInfoUpdated(extended_account_info);
    return;
  }

  // Listen for extended account info getting fetched.
  identity_manager_observation_.Observe(identity_manager);

  // Set up a timeout for extended account info.
  std::u16string fallback_profile_name =
      profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
          core_account_info_);
  CHECK(!fallback_profile_name.empty());
  extended_account_info_timeout_closure_.Reset(
      base::BindOnce(&ProfileNameResolver::OnProfileNameResolved,
                     weak_ptr_factory_.GetWeakPtr(), fallback_profile_name));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, extended_account_info_timeout_closure_.callback(),
      g_extended_account_info_timeout_for_testing.value_or(
          kDefaultExtendedAccountInfoTimeout));
}

ProfileNameResolver::~ProfileNameResolver() = default;

void ProfileNameResolver::RunWithProfileName(NameResolvedCallback callback) {
  CHECK(!on_profile_name_resolved_callback_);  // Multiple pending callbacks
                                               // not supported yet.
  if (resolved_profile_name_.empty()) {
    on_profile_name_resolved_callback_ = std::move(callback);
    return;
  }

  std::move(callback).Run(resolved_profile_name_);
}

void ProfileNameResolver::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (!account_info.IsValid() ||
      core_account_info_.account_id != account_info.account_id) {
    return;
  }

  std::u16string profile_name =
      profiles::GetDefaultNameForNewSignedInProfile(account_info);
  CHECK(!profile_name.empty());
  OnProfileNameResolved(profile_name);
}

void ProfileNameResolver::OnProfileNameResolved(
    const std::u16string& profile_name) {
  CHECK(!profile_name.empty());
  DCHECK(resolved_profile_name_.empty());  // Should be resolved only once.

  // Cancel timeout and stop listening to further changes.
  extended_account_info_timeout_closure_.Cancel();
  identity_manager_observation_.Reset();

  resolved_profile_name_ = profile_name;
  if (on_profile_name_resolved_callback_) {
    std::move(on_profile_name_resolved_callback_).Run(resolved_profile_name_);
  }
}
