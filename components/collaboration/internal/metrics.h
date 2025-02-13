// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
#define COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_

namespace collaboration::metrics {

// Types of join events that occur in the collaboration service.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceJoinEvent)
enum class CollaborationServiceJoinEvent {
  kUnknown = 0,
  kStarted = 1,
  kCanceled = 2,
  kCanceledNotSignedIn = 3,
  kNotSignedIn = 4,
  kAccepted = 5,
  kOpenedNewGroup = 6,
  kOpenedExistingGroup = 7,
  kFlowRequirementsMet = 8,
  kParsingFailure = 9,
  kSigninVerificationFailed = 10,
  kSigninVerified = 11,
  kSigninVerifiedInObserver = 12,
  kFoundCollaborationWithoutTabGroup = 13,
  kReadNewGroupFailed = 14,
  kReadNewGroupSuccess = 15,
  kAddedUserToGroup = 16,
  kPreviewGroupFullError = 17,
  kPreviewFailure = 18,
  kPreviewSuccess = 19,
  kGroupExistsWhenJoined = 20,
  kTabGroupFetched = 21,
  kPeopleGroupFetched = 22,
  kPromoteTabGroup = 23,
  kDataSharingReadyWhenStarted = 24,
  kDataSharingServiceReadyObserved = 25,
  kTabGroupServiceReady = 26,
  kAllServicesReadyForFlow = 27,
  kMaxValue = kAllServicesReadyForFlow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceJoinEvent)

// Types of join events that occur in the collaboration service.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceShareOrManageEvent)
enum class CollaborationServiceShareOrManageEvent {
  kUnknown = 0,
  kStarted = 1,
  kNotSignedIn = 2,
  kCanceledNotSignedIn = 3,
  kShareDialogShown = 4,
  kManageDialogShown = 5,
  kTabGroupShared = 6,
  kUrlReadyToShare = 7,
  kFlowRequirementsMet = 8,
  kSigninVerificationFailed = 9,
  kSigninVerified = 10,
  kSigninVerifiedInObserver = 11,
  kSyncedTabGroupNotFound = 12,
  kCollaborationIdMissing = 13,
  kCollaborationIdInvalid = 14,
  kTabGroupMissingBeforeMigration = 15,
  kMigrationFailure = 16,
  kReadGroupFailed = 17,
  kUrlCreationFailed = 18,
  kDataSharingReadyWhenStarted = 19,
  kDataSharingServiceReadyObserved = 20,
  kTabGroupServiceReady = 21,
  kAllServicesReadyForFlow = 22,
  kMaxValue = kAllServicesReadyForFlow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceShareOrManageEvent)

void RecordJoinEvent(CollaborationServiceJoinEvent event);
void RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent event);
}  // namespace collaboration::metrics

#endif  // COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
