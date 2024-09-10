// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_

#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/service_status.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_sync_bridge.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace data_sharing {
class DataSharingNetworkLoader;

// The core class for managing data sharing.
class DataSharingService : public KeyedService, public base::SupportsUserData {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.components.data_sharing)
  enum class PeopleGroupActionFailure {
    kUnknown = 0,
    kTransientFailure = 1,
    kPersistentFailure = 2
  };

  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.components.data_sharing)
  enum class PeopleGroupActionOutcome {
    kUnknown = 0,
    kSuccess = 1,
    kTransientFailure = 2,
    kPersistentFailure = 3
  };

  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.components.data_sharing)
  enum class ParseURLStatus {
    kUnknown = 0,
    kSuccess = 1,
    kHostOrPathMismatchFailure = 2,
    kQueryMissingFailure = 3
  };

  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    virtual void OnGroupChanged(const GroupData& group_data) {}
    // User either created a new group or has been invited to the existing one.
    virtual void OnGroupAdded(const GroupData& group_data) {}
    // Either group has been deleted or user has been removed from the group.
    virtual void OnGroupRemoved(const GroupId& group_id) {}

    // The update details of a service's collaboration status.
    struct ServiceStatusUpdate {
      ServiceStatus old_status;
      ServiceStatus new_status;

      // Add helper functions as needed here.
    };

    // The service status has been changed.
    virtual void OnServiceStatusChanged(
        const ServiceStatusUpdate& status_update) {}
  };

  using GroupDataOrFailureOutcome =
      base::expected<GroupData, PeopleGroupActionFailure>;
  using GroupsDataSetOrFailureOutcome =
      base::expected<std::set<GroupData>, PeopleGroupActionFailure>;
  using SharedDataPreviewOrFailureOutcome =
      base::expected<SharedDataPreview, PeopleGroupActionFailure>;
  using ParseURLResult = base::expected<GroupToken, ParseURLStatus>;

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type DataSharingService for the given
  // DataSharingService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      DataSharingService* data_sharing_service);
#endif  // BUILDFLAG(IS_ANDROID)

  DataSharingService() = default;
  ~DataSharingService() override = default;

  // Disallow copy/assign.
  DataSharingService(const DataSharingService&) = delete;
  DataSharingService& operator=(const DataSharingService&) = delete;

  // Whether the service is an empty implementation. This is here because the
  // Chromium build disables RTTI, and we need to be able to verify that we are
  // using an empty service from the Chrome embedder.
  virtual bool IsEmptyService() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the network loader for fetching data.
  virtual DataSharingNetworkLoader* GetDataSharingNetworkLoader() = 0;

  // Returns DataTypeControllerDelegate for the collaboration group datatype.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetCollaborationGroupControllerDelegate() = 0;

  // People Group API.
  // Refreshes data if necessary. On success passes to the `callback` a set of
  // all groups known to the client (ordered by id).
  virtual void ReadAllGroups(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)>
          callback) = 0;

  // Refreshes data if necessary and passes the GroupData to `callback`.
  virtual void ReadGroup(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Attempts to create a new group. Returns a created group on success.
  virtual void CreateGroup(
      const std::string& group_name,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Attempts to delete a group.
  virtual void DeleteGroup(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Attempts to invite a new user to the group.
  virtual void InviteMember(
      const GroupId& group_id,
      const std::string& invitee_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Attempts to add the primary account associated with the current profile to
  // the group.
  virtual void AddMember(
      const GroupId& group_id,
      const std::string& access_token,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Attempts to remove a user from the group.
  virtual void RemoveMember(
      const GroupId& group_id,
      const std::string& member_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Check if the given URL should be intercepted.
  virtual bool ShouldInterceptNavigationForShareURL(const GURL& url) = 0;

  // Called when a data sharing type URL has been intercepted.
  virtual void HandleShareURLNavigationIntercepted(const GURL& url) = 0;

  // Create a data sharing URL used for sharing. This does not validate if the
  // group is still active nor guarantee that the URL is not expired. The caller
  // needs to get the valid group info from the other APIs above. Make sure
  // EnsureGroupVisibility API is called before getting the URL for the group.
  virtual std::unique_ptr<GURL> GetDataSharingURL(
      const GroupData& group_data) = 0;

  // Parse and validate a data sharing URL. This simply parses the url. The
  // returned group may not be valid, the caller needs to check ReadGroup or
  // other apis to validate the group.
  virtual ParseURLResult ParseDataSharingURL(const GURL& url) = 0;

  // This ensures that the group is open for new members to join. Only owner can
  // call this API. The owner must always call this API before
  // GetDataSharingURL().
  virtual void EnsureGroupVisibility(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Gets a preview of the shared entities. The returned result may contain
  // all types of shared entities for the group.
  virtual void GetSharedEntitiesPreview(
      const GroupToken& group_token,
      base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
          callback) = 0;

  // Get the current DataSharingUIDelegate instance.
  virtual DataSharingUIDelegate* GetUIDelegate() = 0;

  // Get the current ServiceStatus.
  virtual ServiceStatus GetServiceStatus() = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_
