// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

namespace collaboration {

CollaborationServiceImpl::CollaborationServiceImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service) {}

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

}  // namespace collaboration
