// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

#include "base/metrics/histogram_functions.h"

namespace {

using signin_metrics::SigninOrSyncStatus;

// Returns the appropriate status if there are |opened_profiles| total,
// of which |profiles_in_state| are in the state.
SigninOrSyncStatus GetStatus(size_t opened_profiles, size_t profiles_in_state) {
  if (opened_profiles == 0)
    return SigninOrSyncStatus::kUnknown;
  if (opened_profiles == profiles_in_state)
    return SigninOrSyncStatus::kAllProfiles;
  if (profiles_in_state == 0)
    return SigninOrSyncStatus::kNoProfiles;
  return SigninOrSyncStatus::kMixedProfiles;
}

}  // anonymous namespace

namespace signin_metrics {

void EmitHistograms(const ProfilesStatus& profiles_status) {
  base::UmaHistogramEnumeration(
      "UMA.ProfileSignInStatusV2",
      GetStatus(profiles_status.num_opened_profiles,
                profiles_status.num_signed_in_profiles));
  base::UmaHistogramEnumeration(
      "UMA.ProfileSyncStatusV2",
      GetStatus(profiles_status.num_opened_profiles,
                profiles_status.num_syncing_profiles));
}

void UpdateProfilesStatusBasedOnSignInAndSyncStatus(
    ProfilesStatus& profiles_status,
    bool signed_in,
    bool syncing) {
  profiles_status.num_opened_profiles++;
  if (signed_in)
    profiles_status.num_signed_in_profiles++;
  if (syncing)
    profiles_status.num_syncing_profiles++;
}

}  // namespace signin_metrics
