// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_HELPERS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_HELPERS_H_

#include <cstddef>

// Structures and common code used to regularly emit metrics about sign-in and
// sync status of all opened profiles.
namespace signin_metrics {

// Possible sign-in or sync status of all profiles open during one snapshot.
// For kMixedProfiles, at least one signed-in profile and at least one
// signed-out profile were open. Some statuses are not applicable to all
// platforms.
//
// This is only made visible for testing.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninOrSyncStatus {
  // All profiles signed in / syncing, with no error.
  kAllProfiles = 0,
  // All profiles signed out / not syncing (no errors are possible).
  kNoProfiles = 1,
  // Mix of different states across profiles. For example if a profile is signed
  // in without error and another signed in with error.
  kMixedProfiles = 2,
  // No loaded profiles.
  kUnknown = 3,
  // All profiles with signin/sync error.
  // Note: this is only supported for signin. It is not supported for Sync, and
  // this value is not currently used for Sync. Profiles with Sync error are
  // reported as not syncing.
  kAllProfilesInError = 4,

  kMaxValue = kAllProfilesInError,
};

// Provides information relating to the status of profiles in the embedder:
// how many are open, how many are signed in, and how many are syncing.
struct ProfilesStatus {
  size_t num_opened_profiles = 0;

  // A profile with error is counted in `num_signed_in_profiles_with_error` but
  // not in `num_signed_in_profiles`.
  size_t num_signed_in_profiles = 0;
  size_t num_signed_in_profiles_with_error = 0;

  // Profiles with sync active (no sync error).
  size_t num_syncing_profiles = 0;
};

enum class SingleProfileSigninStatus {
  kSignedIn,
  kSignedInWithError,
  kSignedOut
};

// Uses |profiles_status| to identify whether all, some, or no profiles are
// signed-in and emits that to the appropriate histogram.  Does the same with
// sync status to the sync histogram.
void EmitHistograms(const ProfilesStatus& profiles_status);

// Updates `profiles_status` to incorporate another opened profiles that is
// `signin_status` and `syncing`.
void UpdateProfilesStatusBasedOnSignInAndSyncStatus(
    ProfilesStatus& profiles_status,
    SingleProfileSigninStatus signin_status,
    bool syncing);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_HELPERS_H_
