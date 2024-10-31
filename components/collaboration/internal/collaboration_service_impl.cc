// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace collaboration {

using data_sharing::GroupData;
using data_sharing::GroupId;
using data_sharing::GroupMember;
using data_sharing::MemberRole;

CollaborationServiceImpl::CollaborationServiceImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  // Initialize ServiceStatus.
  current_status_.collaboration_status = CollaborationStatus::kDisabled;
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    current_status_.collaboration_status =
        CollaborationStatus::kEnabledCreateAndJoin;
  } else if (base::FeatureList::IsEnabled(
                 data_sharing::features::kDataSharingJoinOnly)) {
    current_status_.collaboration_status = CollaborationStatus::kAllowedToJoin;
  }

  // TODO(b/360184707): Add identity manager and sync service to observe state
  // changes.
}

CollaborationServiceImpl::~CollaborationServiceImpl() = default;

bool CollaborationServiceImpl::IsEmptyService() {
  return false;
}

void CollaborationServiceImpl::StartJoinFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GURL& url) {}

void CollaborationServiceImpl::StartShareFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    tab_groups::EitherGroupID group_id) {}

ServiceStatus CollaborationServiceImpl::GetServiceStatus() {
  return current_status_;
}

MemberRole CollaborationServiceImpl::GetCurrentUserRoleForGroup(
    const GroupId& group_id) {
  std::optional<GroupData> group_data =
      data_sharing_service_->ReadGroup(group_id);
  if (!group_data.has_value() || group_data.value().members.empty()) {
    // Group do not exist or is empty.
    return MemberRole::kUnknown;
  }

  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (account.IsEmpty()) {
    // No current logged in user.
    return MemberRole::kUnknown;
  }

  for (const GroupMember& member : group_data.value().members) {
    if (member.gaia_id == account.gaia) {
      return member.role;
    }
  }

  // Current user is not found in group.
  return MemberRole::kUnknown;
}

}  // namespace collaboration
