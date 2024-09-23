// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_metrics/state.h"

#include "base/metrics/histogram_functions.h"

namespace profile_metrics {

namespace {

std::string GetStateSuffix(StateSuffix suffix) {
  switch (suffix) {
    case StateSuffix::kAll:
      return "_All";
    case StateSuffix::kAllManagedDevice:
      return "_AllManagedDevice";
    case StateSuffix::kAllUnmanagedDevice:
      return "_AllUnmanagedDevice";
    case StateSuffix::kActiveMultiProfile:
      return "_ActiveMultiProfile";
    case StateSuffix::kLatentMultiProfile:
      return "_LatentMultiProfile";
    case StateSuffix::kLatentMultiProfileActive:
      return "_LatentMultiProfileActive";
    case StateSuffix::kLatentMultiProfileOthers:
      return "_LatentMultiProfileOthers";
    case StateSuffix::kSingleProfile:
      return "_SingleProfile";
    case StateSuffix::kUponDeletion:
      return "_UponDeletion";
  }
}

// Context for profile deletion.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DeleteProfileContext {
  kWithoutBrowserLastProfile = 0,
  kWithoutBrowserAdditionalProfile = 1,
  kWithBrowserLastProfile = 2,
  kWithBrowserAdditionalProfile = 3,
  kMaxValue = kWithBrowserAdditionalProfile
};

}  // namespace

void LogProfileAccountType(UnconsentedPrimaryAccountType account_type,
                           StateSuffix suffix) {
  base::UmaHistogramEnumeration(
      "Profile.State.UnconsentedPrimaryAccountType" + GetStateSuffix(suffix),
      account_type);
}

void LogProfileSyncEnabled(bool sync_enabled, StateSuffix suffix) {
  base::UmaHistogramBoolean(
      "Profile.State.SyncEnabled" + GetStateSuffix(suffix), sync_enabled);
}

void LogProfileDaysSinceLastUse(int days_since_last_use, StateSuffix suffix) {
  base::UmaHistogramCounts1000(
      "Profile.State.LastUsed" + GetStateSuffix(suffix), days_since_last_use);
}

void LogProfileDeletionContext(bool is_last_profile, bool no_browser_windows) {
  DeleteProfileContext context;
  if (no_browser_windows) {
    if (is_last_profile) {
      context = DeleteProfileContext::kWithoutBrowserLastProfile;
    } else {
      context = DeleteProfileContext::kWithoutBrowserAdditionalProfile;
    }
  } else {
    if (is_last_profile) {
      context = DeleteProfileContext::kWithBrowserLastProfile;
    } else {
      context = DeleteProfileContext::kWithBrowserAdditionalProfile;
    }
  }
  base::UmaHistogramEnumeration("Profile.DeleteProfileContext", context);
}

void LogProfileAllAccountsNames(AllAccountsNames names) {
  base::UmaHistogramEnumeration("Profile.AllAccounts.Names", names);
}

}  // namespace profile_metrics
