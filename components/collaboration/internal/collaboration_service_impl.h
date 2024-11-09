// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_

#include <string>

#include "components/collaboration/public/collaboration_service.h"

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace collaboration {
class CollaborationController;

// The internal implementation of the CollborationService.
class CollaborationServiceImpl : public CollaborationService {
 public:
  CollaborationServiceImpl(
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      data_sharing::DataSharingService* data_sharing_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);
  ~CollaborationServiceImpl() override;

  // CollaborationService implementation.
  bool IsEmptyService() override;
  void StartJoinFlow(std::unique_ptr<CollaborationControllerDelegate> delegate,
                     const GURL& url) override;
  void StartShareFlow(std::unique_ptr<CollaborationControllerDelegate> delegate,
                      tab_groups::EitherGroupID group_id) override;
  ServiceStatus GetServiceStatus() override;
  data_sharing::MemberRole GetCurrentUserRoleForGroup(
      const data_sharing::GroupId& group_id) override;

  // For testing.
  const std::map<data_sharing::GroupToken,
                 std::unique_ptr<CollaborationController>>&
  GetJoinControllersForTesting();

 private:
  ServiceStatus current_status_;

  // Service providing information about tabs and tab groups.
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about people groups.
  const raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // Service providing information about sign in.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Service providing information about sync.
  const raw_ptr<syncer::SyncService> sync_service_;

  // Started flows.
  // Join controllers: <GroupId, CollaborationController>
  std::map<data_sharing::GroupToken, std::unique_ptr<CollaborationController>>
      join_controllers_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
