// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/collaboration/public/collaboration_flow_entry_point.h"
#include "components/data_sharing/public/logger.h"
#include "components/data_sharing/public/logger_common.mojom.h"
#include "components/data_sharing/public/logger_utils.h"
#include "ui/base/page_transition_types.h"

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
    case CollaborationServiceJoinEvent::kReadNewGroupUserIsAlreadyMember:
      return "ReadNewGroupUserIsAlreadyMember";
    case CollaborationServiceJoinEvent::kFailedAddingUserToGroup:
      return "FailedAddingUserToGroup";
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
    case CollaborationServiceShareOrManageEvent::kCollaborationGroupCreated:
      return "CollaborationGroupCreated";
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
    case CollaborationServiceShareOrManageEvent::
        kCollaborationIdEmptyGroupToken:
      return "CollaborationIdEmptyGroupToken";
    case CollaborationServiceShareOrManageEvent::kCollaborationIdShareCanceled:
      return "CollaborationIdShareCanceled";
    case CollaborationServiceShareOrManageEvent::kTabGroupShared:
      return "TabGroupShared";
  }
}

std::string_view CollaborationServiceJoinEntryPointToString(
    CollaborationServiceJoinEntryPoint entry) {
  switch (entry) {
    case CollaborationServiceJoinEntryPoint::kUnknown:
      return "Unknown";
    case CollaborationServiceJoinEntryPoint::kLinkClick:
      return "LinkClick";
    case CollaborationServiceJoinEntryPoint::kUserTyped:
      return "UserTyped";
    case CollaborationServiceJoinEntryPoint::kExternalApp:
      return "ExternalApp";
    case CollaborationServiceJoinEntryPoint::kForwardBackButton:
      return "ForwardBackButton";
    case CollaborationServiceJoinEntryPoint::kRedirect:
      return "Redirect";
  }
}

std::string_view CollaborationServiceShareOrManageEntryPointToString(
    CollaborationServiceShareOrManageEntryPoint entry) {
  switch (entry) {
    case CollaborationServiceShareOrManageEntryPoint::kUnknown:
      return "Unknown";
    case CollaborationServiceShareOrManageEntryPoint::
        kAndroidTabGridDialogShare:
      return "AndroidTabGridDialogShare";
    case CollaborationServiceShareOrManageEntryPoint::
        kAndroidTabGridDialogManage:
      return "AndroidTabGridDialogManage";
    case CollaborationServiceShareOrManageEntryPoint::kRecentActivity:
      return "RecentActivity";
    case CollaborationServiceShareOrManageEntryPoint::
        kAndroidTabGroupContextMenuShare:
      return "AndroidTabGroupContextMenuShare";
    case CollaborationServiceShareOrManageEntryPoint::
        kAndroidTabGroupContextMenuManage:
      return "AndroidTabGroupContextMenuManage";
    case CollaborationServiceShareOrManageEntryPoint::kNotification:
      return "Notification";
    case CollaborationServiceShareOrManageEntryPoint::kAndroidMessage:
      return "AndroidMessage";
    case CollaborationServiceShareOrManageEntryPoint::kTabGroupItemMenuShare:
      return "TabGroupItemMenuShare";
    case CollaborationServiceShareOrManageEntryPoint::kAndroidShareSheetExtra:
      return "AndroidShareSheetExtra";
    case CollaborationServiceShareOrManageEntryPoint::kDialogToolbarButton:
      return "DialogToolbarButton";
    case CollaborationServiceShareOrManageEntryPoint::
        kiOSTabGroupIndicatorShare:
      return "iOSTabGroupIndicatorShare";
    case CollaborationServiceShareOrManageEntryPoint::
        kiOSTabGroupIndicatorManage:
      return "iOSTabGroupIndicatorManage";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabGridShare:
      return "iOSTabGridShare";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabGridManage:
      return "iOSTabGridManage";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabStripShare:
      return "iOSTabStripShare";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabStripManage:
      return "iOSTabStripManage";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabGroupViewShare:
      return "iOSTabGroupViewShare";
    case CollaborationServiceShareOrManageEntryPoint::kiOSTabGroupViewManage:
      return "iOSTabGroupViewManage";
    case CollaborationServiceShareOrManageEntryPoint::kiOSMessage:
      return "iOSMessage";
    case CollaborationServiceShareOrManageEntryPoint::
        kDesktopGroupEditorShareOrManageButton:
      return "DesktopGroupEditorShareOrManageButton";
    case CollaborationServiceShareOrManageEntryPoint::kDesktopNotification:
      return "DesktopNotification";
    case CollaborationServiceShareOrManageEntryPoint::kDesktopRecentActivity:
      return "DesktopRecentActivity";
  }
}

std::string_view CollaborationServiceLeaveOrDeleteEntryPointToString(
    CollaborationServiceLeaveOrDeleteEntryPoint entry) {
  switch (entry) {
    case CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown:
      return "Unknown";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGridDialogLeave:
      return "AndroidTabGridDialogLeave";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGridDialogDelete:
      return "AndroidTabGridDialogDelete";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGroupContextMenuLeave:
      return "AndroidTabGroupContextMenuLeave";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGroupContextMenuDelete:
      return "AndroidTabGroupContextMenuDelete";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGroupItemMenuLeave:
      return "AndroidTabGroupItemMenuLeave";
    case CollaborationServiceLeaveOrDeleteEntryPoint::
        kAndroidTabGroupItemMenuDelete:
      return "AndroidTabGroupItemMenuDelete";
    case CollaborationServiceLeaveOrDeleteEntryPoint::kAndroidTabGroupRow:
      return "AndroidTabGroupRow";
  }
}

std::string_view CollaborationServiceStepToString(
    CollaborationServiceStep step) {
  switch (step) {
    case CollaborationServiceStep::kUnknown:
      return "Unknown";
    case CollaborationServiceStep::kAuthenticationInitToSuccess:
      return "AuthenticationInitToSuccess";
    case CollaborationServiceStep::kWaitingForServicesInitialization:
      return "WaitingForServicesInitialization";
    case CollaborationServiceStep::kLinkReadyAfterGroupCreation:
      return "LinkReadyAfterGroupCreation";
    case CollaborationServiceStep::kTabGroupFetchedAfterPeopleGroupJoined:
      return "TabGroupFetchedAfterPeopleGroupJoined";
    case CollaborationServiceStep::kFullJoinFlowSuccess:
      return "FullJoinFlowSuccess";
  }
}

std::string_view CollaborationServiceFlowEventToString(
    CollaborationServiceFlowEvent event) {
  switch (event) {
    case CollaborationServiceFlowEvent::kUnknown:
      return "Unknown";
    case CollaborationServiceFlowEvent::kStarted:
      return "Started";
    case CollaborationServiceFlowEvent::kNotSignedIn:
      return "NotSignedIn";
    case CollaborationServiceFlowEvent::kCanceledNotSignedIn:
      return "CanceledNotSignedIn";
    case CollaborationServiceFlowEvent::kFlowRequirementsMet:
      return "FlowRequirementsMet";
    case CollaborationServiceFlowEvent::kSigninVerificationFailed:
      return "SigninVerificationFailed";
    case CollaborationServiceFlowEvent::kSigninVerified:
      return "SigninVerified";
    case CollaborationServiceFlowEvent::kSigninVerifiedInObserver:
      return "SigninVerifiedInObserver";
    case CollaborationServiceFlowEvent::kDataSharingReadyWhenStarted:
      return "DataSharingReadyWhenStarted";
    case CollaborationServiceFlowEvent::kDataSharingServiceReadyObserved:
      return "DataSharingServiceReadyObserved";
    case CollaborationServiceFlowEvent::kTabGroupServiceReady:
      return "TabGroupServiceReady";
    case CollaborationServiceFlowEvent::kAllServicesReadyForFlow:
      return "AllServicesReadyForFlow";
    case CollaborationServiceFlowEvent::kDevicePolicyDisableSignin:
      return "DevicePolicyDisableSignin";
    case CollaborationServiceFlowEvent::kManagedAccountSignin:
      return "ManagedAccountSignin";
    case CollaborationServiceFlowEvent::kAccountInfoNotReadyOnSignin:
      return "AccountInfoNotReadyOnSignin";
    // Join flow metrics
    case CollaborationServiceFlowEvent::kJoinCanceled:
      return "JoinCanceled";
    case CollaborationServiceFlowEvent::kJoinAccepted:
      return "JoinAccepted";
    case CollaborationServiceFlowEvent::kJoinOpenedNewGroup:
      return "JoinOpenedNewGroup";
    case CollaborationServiceFlowEvent::kJoinOpenedExistingGroup:
      return "JoinOpenedExistingGroup";
    case CollaborationServiceFlowEvent::kJoinParsingFailure:
      return "JoinParsingFailure";
    case CollaborationServiceFlowEvent::kJoinFoundCollaborationWithoutTabGroup:
      return "JoinFoundCollaborationWithoutTabGroup";
    case CollaborationServiceFlowEvent::kJoinReadNewGroupFailed:
      return "JoinReadNewGroupFailed";
    case CollaborationServiceFlowEvent::kJoinReadNewGroupSuccess:
      return "JoinReadNewGroupSuccess";
    case CollaborationServiceFlowEvent::kJoinAddedUserToGroup:
      return "JoinAddedUserToGroup";
    case CollaborationServiceFlowEvent::kJoinPreviewGroupFullError:
      return "JoinPreviewGroupFullError";
    case CollaborationServiceFlowEvent::kJoinPreviewFailure:
      return "JoinPreviewFailure";
    case CollaborationServiceFlowEvent::kJoinPreviewSuccess:
      return "JoinPreviewSuccess";
    case CollaborationServiceFlowEvent::kJoinGroupExistsWhenJoined:
      return "JoinGroupExistsWhenJoined";
    case CollaborationServiceFlowEvent::kJoinTabGroupFetched:
      return "JoinTabGroupFetched";
    case CollaborationServiceFlowEvent::kJoinPeopleGroupFetched:
      return "JoinPeopleGroupFetched";
    case CollaborationServiceFlowEvent::kJoinPromoteTabGroup:
      return "JoinPromoteTabGroup";
    case CollaborationServiceFlowEvent::kJoinTimeoutWaitingForServicesReady:
      return "JoinTimeoutWaitingForServicesReady";
    case CollaborationServiceFlowEvent::
        kJoinTimeoutWaitingForSyncAndDataSharingGroup:
      return "JoinTimeoutWaitingForSyncAndDataSharingGroup";
    case CollaborationServiceFlowEvent::kJoinReadNewGroupUserIsAlreadyMember:
      return "JoinReadNewGroupUserIsAlreadyMember";
    case CollaborationServiceFlowEvent::kJoinFailedAddingUserToGroup:
      return "JoinFailedAddingUserToGroup";
    // Share or manage flow metrics
    case CollaborationServiceFlowEvent::kShareDialogShown:
      return "ShareDialogShown";
    case CollaborationServiceFlowEvent::kManageDialogShown:
      return "ManageDialogShown";
    case CollaborationServiceFlowEvent::kCollaborationGroupCreated:
      return "CollaborationGroupCreated";
    case CollaborationServiceFlowEvent::kUrlReadyToShare:
      return "UrlReadyToShare";
    case CollaborationServiceFlowEvent::kSyncedTabGroupNotFound:
      return "SyncedTabGroupNotFound";
    case CollaborationServiceFlowEvent::kCollaborationIdMissing:
      return "CollaborationIdMissing";
    case CollaborationServiceFlowEvent::kCollaborationIdInvalid:
      return "CollaborationIdInvalid";
    case CollaborationServiceFlowEvent::kTabGroupMissingBeforeMigration:
      return "TabGroupMissingBeforeMigration";
    case CollaborationServiceFlowEvent::kMigrationFailure:
      return "MigrationFailure";
    case CollaborationServiceFlowEvent::kReadGroupFailed:
      return "ReadGroupFailed";
    case CollaborationServiceFlowEvent::kUrlCreationFailed:
      return "UrlCreationFailed";
    case CollaborationServiceFlowEvent::kCollaborationIdEmptyGroupToken:
      return "CollaborationIdEmptyGroupToken";
    case CollaborationServiceFlowEvent::kCollaborationIdShareCanceled:
      return "CollaborationIdShareCanceled";
    case CollaborationServiceFlowEvent::kTabGroupShared:
      return "TabGroupShared";
  }
}

std::string_view CreateFlowTypeToString(FlowType type) {
  switch (type) {
    case FlowType::kJoin:
      return "JoinFlow";
    case FlowType::kShareOrManage:
      return "ShareOrManageFlow";
    case FlowType::kLeaveOrDelete:
      return "LeaveOrDeleteFlow";
  }
}

std::string CreateJoinEventLogString(CollaborationServiceJoinEvent event) {
  return base::StringPrintf("Join Flow Event: %s",
                            CollaborationServiceJoinEventToString(event));
}

std::string CreateShareOrManageEventLogString(
    CollaborationServiceShareOrManageEvent event) {
  return base::StringPrintf(
      "Share or Manage Flow: %s",
      CollaborationServiceShareOrManageEventToString(event));
}

std::string CreateJoinEntryLogToString(
    CollaborationServiceJoinEntryPoint entry) {
  return base::StringPrintf("Join Flow Started\n  From: %s\n",
                            CollaborationServiceJoinEntryPointToString(entry));
}

std::string CreateShareOrManageEntryLogToString(
    CollaborationServiceShareOrManageEntryPoint entry) {
  return base::StringPrintf(
      "Share or Manage Flow Started\n  From: %s\n",
      CollaborationServiceShareOrManageEntryPointToString(entry));
}

std::string CreateLeaveOrDeleteEntryLogToString(
    CollaborationServiceLeaveOrDeleteEntryPoint entry) {
  return base::StringPrintf(
      "Leave or Delete Flow Started\n  From: %s\n",
      CollaborationServiceLeaveOrDeleteEntryPointToString(entry));
}

std::string CreateLatencyLogToString(CollaborationServiceStep step,
                                     base::TimeDelta duration) {
  return base::StringPrintf("Step %s took %dms to complete.",
                            CollaborationServiceStepToString(step),
                            duration.InMillisecondsRoundedUp());
}

std::string CreateFlowEventLogString(CollaborationServiceFlowEvent event) {
  return base::StringPrintf("Flow Event: %s",
                            CollaborationServiceFlowEventToString(event));
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
  } else if (type == FlowType::kShareOrManage) {
    RecordShareOrManageEvent(logger, share_or_manage_event);
  }
}

void RecordJoinEntryPoint(data_sharing::Logger* logger,
                          CollaborationServiceJoinEntryPoint entry) {
  base::UmaHistogramEnumeration("CollaborationService.JoinFlow.EntryPoint",
                                entry);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateJoinEntryLogToString(entry));
}

void RecordShareOrManageEntryPoint(
    data_sharing::Logger* logger,
    CollaborationServiceShareOrManageEntryPoint entry) {
  base::UmaHistogramEnumeration(
      "CollaborationService.ShareOrManageFlow.EntryPoint", entry);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateShareOrManageEntryLogToString(entry));
}

void RecordLeaveOrDeleteEntryPoint(
    data_sharing::Logger* logger,
    CollaborationServiceLeaveOrDeleteEntryPoint entry) {
  base::UmaHistogramEnumeration(
      "CollaborationService.LeaveOrDeleteFlow.EntryPoint", entry);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateLeaveOrDeleteEntryLogToString(entry));
}

void RecordLatency(data_sharing::Logger* logger,
                   CollaborationServiceStep step,
                   base::TimeDelta duration) {
  std::string histogram_name =
      base::StrCat({"CollaborationService.Latency.",
                    CollaborationServiceStepToString(step)});

  base::UmaHistogramMediumTimes(histogram_name, duration);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateLatencyLogToString(step, duration));
}

void RecordCollaborationFlowEvent(data_sharing::Logger* logger,
                                  FlowType type,
                                  CollaborationServiceFlowEvent event) {
  std::string histogram_name = base::StrCat(
      {"CollaborationService.", CreateFlowTypeToString(type), ".Events"});

  base::UmaHistogramEnumeration(histogram_name, event);
  DATA_SHARING_LOG(logger_common::mojom::LogSource::CollaborationService,
                   logger, CreateFlowEventLogString(event));
}

}  // namespace collaboration::metrics
