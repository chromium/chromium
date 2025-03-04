// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/data_sharing/public/logger.h"
#include "components/data_sharing/public/logger_common.mojom.h"
#include "components/data_sharing/public/logger_utils.h"

namespace collaboration::metrics {

namespace {
std::string_view CollaborationServiceJoinEventToString(
    CollaborationServiceJoinEvent event) {
  switch (event) {
    case CollaborationServiceJoinEvent::kUnknown:
      return "Unknown";
    case CollaborationServiceJoinEvent::kStarted:
      return "Started";
    case CollaborationServiceJoinEvent::kCanceled:
      return "Canceled";
    case CollaborationServiceJoinEvent::kCanceledNotSignedIn:
      return "CanceledNotSignedIn";
    case CollaborationServiceJoinEvent::kNotSignedIn:
      return "NotSignedIn";
    case CollaborationServiceJoinEvent::kAccepted:
      return "Accepted";
    case CollaborationServiceJoinEvent::kOpenedNewGroup:
      return "OpenedNewGroup";
    case CollaborationServiceJoinEvent::kOpenedExistingGroup:
      return "OpenedExistingGroup";
    case CollaborationServiceJoinEvent::kFlowRequirementsMet:
      return "FlowRequirementsMet";
    case CollaborationServiceJoinEvent::kParsingFailure:
      return "ParsingFailure";
    case CollaborationServiceJoinEvent::kSigninVerificationFailed:
      return "SigninVerificationFailed";
    case CollaborationServiceJoinEvent::kSigninVerified:
      return "SigninVerified";
    case CollaborationServiceJoinEvent::kSigninVerifiedInObserver:
      return "SigninVerifiedInObserver";
    case CollaborationServiceJoinEvent::kFoundCollaborationWithoutTabGroup:
      return "FoundCollaborationWithoutTabGroup";
    case CollaborationServiceJoinEvent::kReadNewGroupFailed:
      return "ReadNewGroupFailed";
    case CollaborationServiceJoinEvent::kReadNewGroupSuccess:
      return "ReadNewGroupSuccess";
    case CollaborationServiceJoinEvent::kAddedUserToGroup:
      return "AddedUserToGroup";
    case CollaborationServiceJoinEvent::kPreviewGroupFullError:
      return "PreviewGroupFullError";
    case CollaborationServiceJoinEvent::kPreviewFailure:
      return "PreviewFailure";
    case CollaborationServiceJoinEvent::kPreviewSuccess:
      return "PreviewSuccess";
    case CollaborationServiceJoinEvent::kGroupExistsWhenJoined:
      return "GroupExistsWhenJoined";
    case CollaborationServiceJoinEvent::kTabGroupFetched:
      return "TabGroupFetched";
    case CollaborationServiceJoinEvent::kPeopleGroupFetched:
      return "PeopleGroupFetched";
    case CollaborationServiceJoinEvent::kPromoteTabGroup:
      return "PromoteTabGroup";
    case CollaborationServiceJoinEvent::kDataSharingReadyWhenStarted:
      return "DataSharingReadyWhenStarted";
    case CollaborationServiceJoinEvent::kDataSharingServiceReadyObserved:
      return "DataSharingServiceReadyObserved";
    case CollaborationServiceJoinEvent::kTabGroupServiceReady:
      return "TabGroupServiceReady";
    case CollaborationServiceJoinEvent::kAllServicesReadyForFlow:
      return "AllServicesReadyForFlow";
    case CollaborationServiceJoinEvent::kTimeoutWaitingForServicesReady:
      return "TimeoutWaitingForServicesReady";
    case CollaborationServiceJoinEvent::
        kTimeoutWaitingForSyncAndDataSharingGroup:
      return "TimeoutWaitingForSyncAndDataSharingGroup";
    case CollaborationServiceJoinEvent::kDevicePolicyDisableSignin:
      return "DevicePolicyDisableSignin";
    case CollaborationServiceJoinEvent::kManagedAccountSignin:
      return "ManagedAccountSignin";
    case CollaborationServiceJoinEvent::kAccountInfoNotReadyOnSignin:
      return "AccountInfoNotReadyOnSignin";
  }
}

std::string_view CollaborationServiceShareOrManageEventToString(
    CollaborationServiceShareOrManageEvent event) {
  switch (event) {
    case CollaborationServiceShareOrManageEvent::kUnknown:
      return "Unknown";
    case CollaborationServiceShareOrManageEvent::kStarted:
      return "Started";
    case CollaborationServiceShareOrManageEvent::kNotSignedIn:
      return "NotSignedIn";
    case CollaborationServiceShareOrManageEvent::kCanceledNotSignedIn:
      return "CanceledNotSignedIn";
    case CollaborationServiceShareOrManageEvent::kShareDialogShown:
      return "ShareDialogShown";
    case CollaborationServiceShareOrManageEvent::kManageDialogShown:
      return "ManageDialogShown";
    case CollaborationServiceShareOrManageEvent::kTabGroupShared:
      return "TabGroupShared";
    case CollaborationServiceShareOrManageEvent::kUrlReadyToShare:
      return "UrlReadyToShare";
    case CollaborationServiceShareOrManageEvent::kFlowRequirementsMet:
      return "FlowRequirementsMet";
    case CollaborationServiceShareOrManageEvent::kSigninVerificationFailed:
      return "SigninVerificationFailed";
    case CollaborationServiceShareOrManageEvent::kSigninVerified:
      return "SigninVerified";
    case CollaborationServiceShareOrManageEvent::kSigninVerifiedInObserver:
      return "SigninVerifiedInObserver";
    case CollaborationServiceShareOrManageEvent::kSyncedTabGroupNotFound:
      return "SyncedTabGroupNotFound";
    case CollaborationServiceShareOrManageEvent::kCollaborationIdMissing:
      return "CollaborationIdMissing";
    case CollaborationServiceShareOrManageEvent::kCollaborationIdInvalid:
      return "CollaborationIdInvalid";
    case CollaborationServiceShareOrManageEvent::
        kTabGroupMissingBeforeMigration:
      return "TabGroupMissingBeforeMigration";
    case CollaborationServiceShareOrManageEvent::kMigrationFailure:
      return "MigrationFailure";
    case CollaborationServiceShareOrManageEvent::kReadGroupFailed:
      return "ReadGroupFailed";
    case CollaborationServiceShareOrManageEvent::kUrlCreationFailed:
      return "UrlCreationFailed";
    case CollaborationServiceShareOrManageEvent::kDataSharingReadyWhenStarted:
      return "DataSharingReadyWhenStarted";
    case CollaborationServiceShareOrManageEvent::
        kDataSharingServiceReadyObserved:
      return "DataSharingServiceReadObserved";
    case CollaborationServiceShareOrManageEvent::kTabGroupServiceReady:
      return "TabGroupServiceReady";
    case CollaborationServiceShareOrManageEvent::kAllServicesReadyForFlow:
      return "AllServicesReadyForFlow";
    case CollaborationServiceShareOrManageEvent::kDevicePolicyDisableSignin:
      return "DevicePolicyDisableSignin";
    case CollaborationServiceShareOrManageEvent::kManagedAccountSignin:
      return "ManagedAccountSignin";
    case CollaborationServiceShareOrManageEvent::kAccountInfoNotReadyOnSignin:
      return "AccountInfoNotReadyOnSignin";
  }
}

std::string CreateJoinEventLogString(CollaborationServiceJoinEvent event) {
  std::string log = "Join Flow Event [";
  log += CollaborationServiceJoinEventToString(event);
  log += "]";
  return log;
}

std::string CreateShareOrManageEventLogString(
    CollaborationServiceShareOrManageEvent event) {
  std::string log = "Share or Manage Flow Event [";
  log += CollaborationServiceShareOrManageEventToString(event);
  log += "]";
  return log;
}

}  // namespace

void RecordJoinEvent(data_sharing::Logger* logger,
                     CollaborationServiceJoinEvent event) {
  base::UmaHistogramEnumeration("CollaborationService.JoinFlow", event);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateJoinEventLogString(event));
}

void RecordShareOrManageEvent(data_sharing::Logger* logger,
                              CollaborationServiceShareOrManageEvent event) {
  base::UmaHistogramEnumeration("CollaborationService.ShareOrManageFlow",
                                event);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateShareOrManageEventLogString(event));
}

void RecordJoinOrShareOrManageEvent(
    data_sharing::Logger* logger,
    FlowType type,
    CollaborationServiceJoinEvent join_event,
    CollaborationServiceShareOrManageEvent share_or_manage_event) {
  if (type == FlowType::kJoin) {
    RecordJoinEvent(logger, join_event);
  } else {
    RecordShareOrManageEvent(logger, share_or_manage_event);
  }
}

}  // namespace collaboration::metrics
