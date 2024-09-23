// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

#include "base/metrics/histogram_functions.h"

namespace {

using signin_metrics::SigninOrSyncStatus;

// Returns the appropriate status if there are |opened_profiles| total,
// of which |profiles_in_state| are in the state.
SigninOrSyncStatus GetStatus(size_t opened_profiles,
                             size_t profiles_in_state,
                             size_t profiles_in_error) {
  CHECK_LE(profiles_in_state + profiles_in_error, opened_profiles);
  if (opened_profiles == 0)
    return SigninOrSyncStatus::kUnknown;
  if (opened_profiles == profiles_in_state) {
    return SigninOrSyncStatus::kAllProfiles;
  }
  if (opened_profiles == profiles_in_error) {
    return SigninOrSyncStatus::kAllProfilesInError;
  }
  if (profiles_in_state == 0 && profiles_in_error == 0) {
    return SigninOrSyncStatus::kNoProfiles;
  }
  return SigninOrSyncStatus::kMixedProfiles;
}

}  // anonymous namespace

namespace signin_metrics {

void EmitHistograms(const ProfilesStatus& profiles_status) {
  base::UmaHistogramEnumeration(
      "UMA.ProfileSignInStatusV2",
      GetStatus(profiles_status.num_opened_profiles,
                profiles_status.num_signed_in_profiles,
                profiles_status.num_signed_in_profiles_with_error));

  // Sync paused users are lumped with syncing users for this histogram.
  SigninOrSyncStatus sync_status = GetStatus(
      profiles_status.num_opened_profiles, profiles_status.num_syncing_profiles,
      // Sync paused users are lumped with non-syncing users for this histogram.
      /*profiles_in_error=*/0);
  CHECK_NE(sync_status, SigninOrSyncStatus::kAllProfilesInError);
  base::UmaHistogramEnumeration("UMA.ProfileSyncStatusV2", sync_status);
}

void UpdateProfilesStatusBasedOnSignInAndSyncStatus(
    ProfilesStatus& profiles_status,
    SingleProfileSigninStatus signin_status,
    bool syncing) {
  ++profiles_status.num_opened_profiles;
  switch (signin_status) {
    case SingleProfileSigninStatus::kSignedIn:
      ++profiles_status.num_signed_in_profiles;
      break;
    case SingleProfileSigninStatus::kSignedInWithError:
      ++profiles_status.num_signed_in_profiles_with_error;
      break;
    case SingleProfileSigninStatus::kSignedOut:
      break;
  }

  if (syncing) {
    ++profiles_status.num_syncing_profiles;
  }
}

}  // namespace signin_metrics
