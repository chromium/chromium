// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/collaboration/internal/collaboration_controller.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

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

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/424385780): Clean this duplicate.
// Values for the BrowserSignin policy.
// VALUES MUST COINCIDE WITH THE BrowserSignin POLICY DEFINITION.
// LINT.IfChange(BrowserSigninMode)
enum class BrowserSigninMode {
  kDisabled = 0,
  kEnabled = 1,
  kForced = 2,
};
// LINT.ThenChange(//ios/chrome/browser/policy/model/policy_util.h:BrowserSigninMode)
#endif

// The internal implementation of the CollaborationService.
class CollaborationServiceImpl : public CollaborationService,
                                 public syncer::SyncServiceObserver,
                                 public signin::IdentityManager::Observer {
 public:
  // `local_prefs` is specific to iOS, providing access to device-level
  // preferences. It will be `nullptr` on other platforms where these
  // preferences are managed differently.
  CollaborationServiceImpl(
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      data_sharing::DataSharingService* data_sharing_service,
      signin::IdentityManager* identity_manager,
      PrefService* profile_prefs,
      PrefService* local_prefs);
  ~CollaborationServiceImpl() override;

  // CollaborationService implementation.
  bool IsEmptyService() override;
  void AddObserver(CollaborationService::Observer* observer) override;
  void RemoveObserver(CollaborationService::Observer* observer) override;
  void StartJoinFlow(std::unique_ptr<CollaborationControllerDelegate> delegate,
                     const GURL& url) override;
  void StartShareOrManageFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const tab_groups::EitherGroupID& either_id,
      CollaborationServiceShareOrManageEntryPoint entry) override;
  void StartLeaveOrDeleteFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const tab_groups::EitherGroupID& either_id,
      CollaborationServiceLeaveOrDeleteEntryPoint entry) override;
  void CancelAllFlows() override;
  ServiceStatus GetServiceStatus() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  data_sharing::MemberRole GetCurrentUserRoleForGroup(
      const data_sharing::GroupId& group_id) override;
  std::optional<data_sharing::GroupData> GetGroupData(
      const data_sharing::GroupId& group_id) override;
  void DeleteGroup(const data_sharing::GroupId& group_id,
                   base::OnceCallback<void(bool)> callback) override;
  void LeaveGroup(const data_sharing::GroupId& group_id,
                  base::OnceCallback<void(bool)> callback) override;
  bool ShouldInterceptNavigationForShareURL(const GURL& url) override;
  void HandleShareURLNavigationIntercepted(
      const GURL& url,
      std::unique_ptr<data_sharing::ShareURLInterceptionContext> context,
      CollaborationServiceJoinEntryPoint entry) override;

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
  int GetDeletingControllersCountForTesting();

  // Called to clean up a flow given a GroupToken.
  void FinishCollaborationFlow(const void* controller);

 private:
  SyncStatus GetSyncStatus();
  SigninStatus GetSigninStatus();
  CollaborationStatus GetCollaborationStatus();
  void RefreshServiceStatus();
  void OnCollaborationGroupRemoved(
      const data_sharing::GroupId& group_id,
      base::OnceCallback<void(bool)> callback,
      data_sharing::DataSharingService::PeopleGroupActionOutcome result);
  std::unique_ptr<CollaborationController> CreateCollaborationController(
      CollaborationController::Flow flow,
      std::unique_ptr<CollaborationControllerDelegate> delegate);

  THREAD_CHECKER(thread_checker_);

  ServiceStatus current_status_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  base::ObserverList<CollaborationService::Observer> observers_;
  std::unique_ptr<signin::AccountManagedStatusFinder>
      account_managed_status_finder_;

  // Service providing information about tabs and tab groups.
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about people groups.
  const raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // Service providing information about sign in.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Service providing information about sync.
  raw_ptr<syncer::SyncService> sync_service_;

  // Used to listen for sharing policy pref change notification.
  PrefChangeRegistrar registrar_;

  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<PrefService> local_prefs_;

  // Started flows.
  // Join controllers: <GroupId, CollaborationController>
  std::map<data_sharing::GroupToken, std::unique_ptr<CollaborationController>>
      join_controllers_;
  std::map<tab_groups::EitherGroupID, std::unique_ptr<CollaborationController>>
      collaboration_controllers_;

  // List of pointers that are cleaning up asynchronously.
  std::set<std::unique_ptr<CollaborationController>> cancelled_controllers_;

  base::WeakPtrFactory<CollaborationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_SERVICE_IMPL_H_
