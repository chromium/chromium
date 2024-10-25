// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include "components/data_sharing/public/features.h"

namespace collaboration {

CollaborationServiceImpl::CollaborationServiceImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service) {
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

}  // namespace collaboration
