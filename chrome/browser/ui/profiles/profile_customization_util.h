// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_UTIL_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_UTIL_H_

#include <string>

#include "base/cancelable_callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

// Updates prefs and entries for `profile` to make it ready to be used
// normally by the user.
// `is_default_name` should be set to true if `profile_name` is not selected by
// the app itself but instead was chosen by the user.
void FinalizeNewProfileSetup(Profile* profile,
                             const std::u16string& profile_name,
                             bool is_default_name);

// Helper to obtain a profile name derived from the user's identity.
//
// Obtains the identity associated with `core_account_info` from
// `identity_manager`.
// Calling `RunWithProfileName()` allows providing a callback that will be
// executed when the name is resolved. The name is never empty.
class ProfileNameResolver : public signin::IdentityManager::Observer {
 public:
  explicit ProfileNameResolver(signin::IdentityManager* identity_manager,
                               const CoreAccountInfo& core_account_info);

  ProfileNameResolver(const ProfileNameResolver&) = delete;
  ProfileNameResolver& operator=(const ProfileNameResolver&) = delete;

  ~ProfileNameResolver() override;

  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  using ScopedInfoFetchTimeoutOverride =
      base::AutoReset<std::optional<base::TimeDelta>>;
  // Overrides the timeout allowed for the profile name resolution, before we
  // default to a fallback value.
  static ScopedInfoFetchTimeoutOverride
  CreateScopedInfoFetchTimeoutOverrideForTesting(base::TimeDelta timeout);

  // The profile name is never empty.
  // Note: We are passing the resolved name by copy to protect against the
  // `ProfileNameResolver` being destroyed during the callback, causing the name
  // reference to become invalid.
  using NameResolvedCallback = base::OnceCallback<void(std::u16string)>;
  void RunWithProfileName(NameResolvedCallback callback);

 private:
  void OnProfileNameResolved(const std::u16string& profile_name);

  const CoreAccountInfo core_account_info_;

  std::u16string resolved_profile_name_;
  base::CancelableOnceClosure extended_account_info_timeout_closure_;

  NameResolvedCallback on_profile_name_resolved_callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfileNameResolver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_UTIL_H_
