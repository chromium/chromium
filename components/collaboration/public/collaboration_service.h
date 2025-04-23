// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_

#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_entry_point.h"
#include "components/collaboration/public/service_status.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace syncer {
class SyncService;
}  // namespace syncer

namespace collaboration {

// The core class for managing collaboration group flows.
class CollaborationService : public KeyedService,
                             public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    struct ServiceStatusUpdate {
      ServiceStatus old_status;
      ServiceStatus new_status;

      // Helper methods.
    };
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Called when service status has changed.
    virtual void OnServiceStatusChanged(const ServiceStatusUpdate& update) {}
  };

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type CollaborationService for the given
  // CollaborationService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      CollaborationService* collaboration_service);
#endif  // BUILDFLAG(IS_ANDROID)

  CollaborationService() = default;
  ~CollaborationService() override = default;

  // Disallow copy/assign.
  CollaborationService(const CollaborationService&) = delete;
  CollaborationService& operator=(const CollaborationService&) = delete;

  // Whether the service is an empty implementation. This is here because the
  // Chromium build disables RTTI, and we need to be able to verify that we are
  // using an empty service from the Chrome embedder.
  virtual bool IsEmptyService() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts a new join flow. This will cancel all existing ongoing join and
  // share flows in the same browser instance.
  virtual void StartJoinFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const GURL& url) = 0;

  // Starts a new share or manage flow. This will cancel all existing ongoing
  // flows in the same browser instance. Note: EitherGroupID is either a local
  // tab group id or a sync id.
  virtual void StartShareOrManageFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const tab_groups::EitherGroupID& either_id,
      CollaborationServiceShareOrManageEntryPoint entry) = 0;

  // Starts a new leave or delete flow. This will cancel all existing ongoing
  // flows in the same browser instance.
  virtual void StartLeaveOrDeleteFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const tab_groups::EitherGroupID& either_id,
      CollaborationServiceLeaveOrDeleteEntryPoint entry) = 0;

  // Cancels all the flows currently displayed.
  virtual void CancelAllFlows(base::OnceCallback<void()> finish_callback) = 0;

  // Get the current ServiceStatus.
  virtual ServiceStatus GetServiceStatus() = 0;

  // Invoked on startup to start observing sync service for sync status changes.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service) = 0;

  // Get the group member information of the current user.
  virtual data_sharing::MemberRole GetCurrentUserRoleForGroup(
      const data_sharing::GroupId& group_id) = 0;

  // Synchronously get the group data for the given group id. Returns nullopt if
  // the group doesn't exist, it has not been fetched from the server yet, or
  // the model is not loaded yet.
  virtual std::optional<data_sharing::GroupData> GetGroupData(
      const data_sharing::GroupId& group_id) = 0;

  // Attempts to delete a group. `callback` will be invoked on completion with
  // param indicating whether group is successfully deleted.
  virtual void DeleteGroup(const data_sharing::GroupId& group_id,
                           base::OnceCallback<void(bool success)> callback) = 0;

  // Attempts to leave a group the current user has joined before. `callback`
  // will be invoked on completion with param indicating whether group is
  // successfully deleted.
  virtual void LeaveGroup(const data_sharing::GroupId& group_id,
                          base::OnceCallback<void(bool success)> callback) = 0;

  // Check if the given URL should be intercepted.
  virtual bool ShouldInterceptNavigationForShareURL(const GURL& url) = 0;

  // Called when a data sharing type URL has been intercepted.
  virtual void HandleShareURLNavigationIntercepted(
      const GURL& url,
      std::unique_ptr<data_sharing::ShareURLInterceptionContext> context,
      CollaborationServiceJoinEntryPoint entry) = 0;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_
