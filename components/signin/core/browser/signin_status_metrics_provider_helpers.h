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
  kAllProfiles = 0,
  kNoProfiles = 1,
  kMixedProfiles = 2,
  kUnknown = 3,
  kMaxValue = kUnknown,
};

// Provides information relating to the status of profiles in the embedder:
// how many are open, how many are signed in, and how many are syncing.
struct ProfilesStatus {
  size_t num_opened_profiles = 0;
  size_t num_signed_in_profiles = 0;
  size_t num_syncing_profiles = 0;
};

// Uses |profiles_status| to identify whether all, some, or no profiles are
// signed-in and emits that to the appropriate histogram.  Does the same with
// sync status to the sync histogram.
void EmitHistograms(const ProfilesStatus& profiles_status);

// Updates |profiles_status| to incorporate another opened profiles that is
// |signed_in| and |syncing|.
void UpdateProfilesStatusBasedOnSignInAndSyncStatus(
    ProfilesStatus& profiles_status,
    bool signed_in,
    bool syncing);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_HELPERS_H_
