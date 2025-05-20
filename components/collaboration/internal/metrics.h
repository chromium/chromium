// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
#define COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_

#include "base/time/time.h"
#include "components/collaboration/public/collaboration_flow_entry_point.h"
#include "components/collaboration/public/collaboration_flow_type.h"

namespace data_sharing {
class Logger;
}  // namespace data_sharing

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
  kTimeoutWaitingForServicesReady = 28,
  kTimeoutWaitingForSyncAndDataSharingGroup = 29,
  kDevicePolicyDisableSignin = 30,
  kManagedAccountSignin = 31,
  kAccountInfoNotReadyOnSignin = 32,
  kReadNewGroupUserIsAlreadyMember = 33,
  kFailedAddingUserToGroup = 34,
  kMaxValue = kFailedAddingUserToGroup,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceJoinEvent)

// Types of share or manage events that occur in the collaboration service.
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
  kCollaborationGroupCreated = 6,
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
  kDevicePolicyDisableSignin = 23,
  kManagedAccountSignin = 24,
  kAccountInfoNotReadyOnSignin = 25,
  kCollaborationIdEmptyGroupToken = 26,
  kCollaborationIdShareCanceled = 27,
  kTabGroupShared = 28,
  kMaxValue = kTabGroupShared,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceShareOrManageEvent)

enum class CollaborationServiceStep {
  kUnknown = 0,
  kAuthenticationInitToSuccess = 1,
  kWaitingForServicesInitialization = 2,
  kLinkReadyAfterGroupCreation = 3,
  kTabGroupFetchedAfterPeopleGroupJoined = 4,
  kMaxValue = kTabGroupFetchedAfterPeopleGroupJoined,
};

void RecordJoinEvent(data_sharing::Logger* logger,
                     CollaborationServiceJoinEvent event);
void RecordShareOrManageEvent(data_sharing::Logger* logger,
                              CollaborationServiceShareOrManageEvent event);
void RecordLeaveOrDeleteEntryPoint(
    data_sharing::Logger* logger,
    CollaborationServiceLeaveOrDeleteEntryPoint event);
void RecordJoinOrShareOrManageEvent(
    data_sharing::Logger* logger,
    FlowType type,
    CollaborationServiceJoinEvent join_event,
    CollaborationServiceShareOrManageEvent share_or_manage_event);
void RecordJoinEntryPoint(data_sharing::Logger* logger,
                          CollaborationServiceJoinEntryPoint entry);
void RecordShareOrManageEntryPoint(
    data_sharing::Logger* logger,
    CollaborationServiceShareOrManageEntryPoint entry);
void RecordLatency(data_sharing::Logger* logger,
                   CollaborationServiceStep step,
                   base::TimeDelta duration);
}  // namespace collaboration::metrics

#endif  // COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
