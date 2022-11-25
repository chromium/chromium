// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_UTILS_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/types/strong_alias.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Browser;
class Profile;

// Type of the callbacks that are called to be notified that the switch to a
// given step by `ProfileManagementFlowController` is completed. `success` is
// is set to false if some sort of error is detected, and `true` otherwise.
// This type is intended for documentation purposes, there is no plan to treat
// it like an opaque type.
using StepSwitchFinishedCallback = base::OnceCallback<void(bool success)>;

// Callback executed when the flow finishes, after the host was cleared and
// we opened a browser for the newly set up profile.
// This callback should not rely on profile management flow instances, as we
// assume that they are deleted when the host is cleared.
// The provided `Browser*` should not be `nullptr`. (This assumption is expected
// to change in the future, see crbug.com/1374315.)
using PostHostClearedCallback =
    base::StrongAlias<class PostHostClearedCallbackTag,
                      base::OnceCallback<void(Browser*)>>;

// Callback to run to finish the flow. If a `PostHostClearedCallback` is
// provided, it will be executed after the host is cleared, and will be given
// a browser window for the newly set up profile.
using FinishFlowCallback =
    base::StrongAlias<class FinishFlowCallbackTag,
                      base::OnceCallback<void(PostHostClearedCallback)>>;

// Updates prefs and entries for `profile` to make it ready to be used
// normally by the user.
void FinalizeNewProfileSetup(Profile* profile,
                             const std::u16string& profile_name);

// Helper to obtain a profile name derived from the user's identity.
//
// Obtains the identity from `identity_manager` and caches the computed name,
// which can be obtained by calling `resolved_profile_name()`. If a callback
// is provided through `set_on_profile_name_available_callback()`, it will be
// executed when the name is resolved.
class ProfileNameResolver : public signin::IdentityManager::Observer {
 public:
  explicit ProfileNameResolver(signin::IdentityManager* identity_manager);

  ProfileNameResolver(const ProfileNameResolver&) = delete;
  ProfileNameResolver& operator=(const ProfileNameResolver&) = delete;

  ~ProfileNameResolver() override;

  using ScopedInfoFetchTimeoutOverride =
      base::AutoReset<absl::optional<base::TimeDelta>>;
  // Overrides the timeout allowed for the profile name resolution, before we
  // default to a fallback value.
  static ScopedInfoFetchTimeoutOverride
  CreateScopedInfoFetchTimeoutOverrideForTesting(base::TimeDelta timeout);

  const std::u16string& resolved_profile_name() const {
    return resolved_profile_name_;
  }

  void set_on_profile_name_resolved_callback(base::OnceClosure callback) {
    on_profile_name_resolved_callback_ = std::move(callback);
  }

 private:
  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  void OnProfileNameResolved(const std::u16string& profile_name);

  std::u16string resolved_profile_name_;
  base::CancelableOnceClosure extended_account_info_timeout_closure_;

  base::OnceClosure on_profile_name_resolved_callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfileNameResolver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_UTILS_H_
