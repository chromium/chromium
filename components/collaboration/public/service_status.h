// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_SERVICE_STATUS_H_
#define COMPONENTS_COLLABORATION_PUBLIC_SERVICE_STATUS_H_

namespace collaboration {

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class SigninStatus {
  kNotSignedIn = 0,
  // Signin is disabled either in user setting or by enterprise policy.
  kSigninDisabled = 1,
  kSignedInPaused = 2,
  kSignedIn = 3
};

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class SyncStatus {
  // Sync-the-feature is disabled but required, or the Sync machinery is fully
  // disabled (e.g. by command-line switch).
  kNotSyncing = 0,
  // Syncing is available in principle, but the tab groups type is not enabled.
  // This may also be returned for signed-out users.
  kSyncWithoutTabGroup = 1,
  // Syncing of tab groups is enabled.
  kSyncEnabled = 2,
  // Syncing is disabled due to Enterprise policy.
  kSyncDisabledByEnterprise = 3,
};

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class CollaborationStatus {
  // Users are not allowed to either join or create.
  kDisabled = 0,
  // Disabled while loading some mandatory information.
  kDisabledPending = 1,
  // The Chrome policy disables this feature, eg: enterprise policies.
  kDisabledForPolicy = 2,
  // Users are allowed to join only but have not joined a shared tab group
  // yet.
  kAllowedToJoin = 3,
  // Users are allowed to join only and have already joined at least 1 shared
  // tab group.
  kEnabledJoinOnly = 4,
  // Users are allowed to join and create shared tab groups.
  kEnabledCreateAndJoin = 5,
  // Due to version out of date disable shared date types.
  kVersionOutOfDate = 6,
  // Due to version out of date disable shared date types and show update ui.
  kVersionOutOfDateShowUpdateChromeUi = 7
};

struct ServiceStatus {
  SigninStatus signin_status = SigninStatus::kNotSignedIn;
  SyncStatus sync_status = SyncStatus::kNotSyncing;
  CollaborationStatus collaboration_status = CollaborationStatus::kDisabled;

  // Helper functions for checking DataSharingService's status.

  // Whether the current user is allowed to join a collaboration.
  bool IsAllowedToJoin();

  // Whether the current user is allowed to both join and create a new
  // collaboration.
  bool IsAllowedToCreate();

  // Whether the current user is logged in and sync enabled. This will fail if
  // the current user's account is paused.
  bool IsAuthenticationValid() const;
};

bool operator==(const ServiceStatus& lhs, const ServiceStatus& rhs);

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_SERVICE_STATUS_H_
