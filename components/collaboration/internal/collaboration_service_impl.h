// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/service/sync_service_observer.h"

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace syncer {
class SyncService;
}  // namespace syncer

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace collaboration {
class CollaborationController;

// The internal implementation of the CollborationService.
class CollaborationServiceImpl : public CollaborationService,
                                 public syncer::SyncServiceObserver,
                                 public signin::IdentityManager::Observer {
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

  // SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // For testing.
  const std::map<data_sharing::GroupToken,
                 std::unique_ptr<CollaborationController>>&
  GetJoinControllersForTesting();

  // Called to clean up a flow given a GroupToken.
  void FinishFlow(const data_sharing::GroupToken& token);

 private:
  SyncStatus GetSyncStatus();
  SigninStatus GetSigninStatus();
  void RefreshSigninStatus();

  ServiceStatus current_status_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

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

  base::WeakPtrFactory<CollaborationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
