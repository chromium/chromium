// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_METRICS_STATE_H_
#define COMPONENTS_PROFILE_METRICS_STATE_H_

namespace profile_metrics {

// Type of the unconsented primary account in a profile.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnconsentedPrimaryAccountType {
  kConsumer = 0,
  kEnterprise = 1,
  kChild = 2,
  kSignedOut = 3,
  kMaxValue = kSignedOut
};

// Classification of what gaia names appear or appeared in this profile since
// the last time gaia cookies got deleted. Thus, this also includes signed-out
// accounts. In order to protect privacy, only classifies whether multiple
// distinct gaia names appeared in this profile and if so, whether sync is
// enabled for one of them. Furthermore, this classification uses a low-entropy
// hash to detect distinct names. In case of a rare hash collision (less than
// 0.1% of cases), multiple names get recorded as a single name. Entries should
// not be renumbered and numeric values should never be reused.
enum class AllAccountsNames {
  kLikelySingleName = 0,  // Gets also rare false records due to hash collision.
  kMultipleNamesWithoutSync = 1,
  kMultipleNamesWithSync = 2,
  kMaxValue = kMultipleNamesWithSync
};

// Different types of reporting for profile state. This is used as a histogram
// suffix.
enum class StateSuffix {
  kAll,                 // Recorded for all clients and all their profiles.
  kAllManagedDevice,    // Recorded for all clients on a managed device and all
                        // their profiles.
  kAllUnmanagedDevice,  // Recorded for all clients on an unmanaged device and
                        // all their profiles.
  kActiveMultiProfile,  // Recorded for multi-profile users with >=2 active
                        // profiles, for all their profiles.
  kLatentMultiProfile,  // Recorded for multi-profile users with one active
                        // profile, for all their profiles.
  kLatentMultiProfileActive,  // Recorded for multi-profile users with one
                              // active profile, only for the active profile.
  kLatentMultiProfileOthers,  // Recorded for multi-profile users with one
                              // active profile, only for the non-active
                              // profiles.
  kSingleProfile,  // Recorded for single-profile users for their single
                   // profile.
  kUponDeletion,   // Recorded whenever a profile gets deleted.
};

// Records the state of profile's UPA.
void LogProfileAccountType(UnconsentedPrimaryAccountType account_type,
                           StateSuffix suffix);

// Records the state of profile's sync.
void LogProfileSyncEnabled(bool sync_enabled, StateSuffix suffix);

// Records the days since last use of a profile.
void LogProfileDaysSinceLastUse(int days_since_last_use, StateSuffix suffix);

// Records the context of a profile deletion, whether it is the last profile and
// whether it happens while no browser windows are opened.
void LogProfileDeletionContext(bool is_last_profile, bool no_browser_windows);

// Records the state of account names used in multi-login.
void LogProfileAllAccountsNames(AllAccountsNames names);

}  // namespace profile_metrics

#endif  // COMPONENTS_PROFILE_METRICS_STATE_H_
