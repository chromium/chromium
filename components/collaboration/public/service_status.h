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
  kSignedInPaused = 1,
  kSignedIn = 2
};

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class SyncStatus {
  kNotSyncing = 0,
  kSyncWithoutTabGroup = 1,
  kSyncEnabled = 2
};

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class CollaborationStatus {
  // Users are not allowed to either join or create.
  kDisabled = 0,
  // The Chrome policy disables this feature, eg: enterprise policies.
  kDisabledForPolicy = 1,
  // Users are allowed to join only but have not joined a shared tab group
  // yet.
  kAllowedToJoin = 2,
  // Users are allowed to join only and have already joined at least 1 shared
  // tab group.
  kEnabledJoinOnly = 3,
  // Users are allowed to join and create shared tab groups.
  kEnabledCreateAndJoin = 4
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
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_SERVICE_STATUS_H_
