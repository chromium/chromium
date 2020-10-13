// Copyright 2020 The Chromium Authors. All rights reserved.
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
  }
}

}  // namespace

void LogProfileAvatar(AvatarState avatar_state, StateSuffix suffix) {
  base::UmaHistogramEnumeration("Profile.State.Avatar" + GetStateSuffix(suffix),
                                avatar_state);
}

void LogProfileName(NameState name_state, StateSuffix suffix) {
  base::UmaHistogramEnumeration("Profile.State.Name" + GetStateSuffix(suffix),
                                name_state);
}

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

void LogProfileAllAccountsNames(AllAccountsNames names) {
  base::UmaHistogramEnumeration("Profile.AllAccounts.Names", names);
}

void LogProfileAllAccountsCategories(AllAccountsCategories categories) {
  base::UmaHistogramEnumeration("Profile.AllAccounts.Categories", categories);
}

}  // namespace profile_metrics