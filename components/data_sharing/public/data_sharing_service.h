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
#include "components/data_sharing/migration/public/context_id.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/share_url_interception_context.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace data_sharing {
class DataSharingNetworkLoader;
class DataSharingSDKDelegate;
class Logger;
class PreviewServerProxy;

// The core class for managing data sharing.
class DataSharingService : public KeyedService, public base::SupportsUserData {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.components.data_sharing)
  enum class DataPreviewActionFailure {
    kUnknown = 0,
    kPermissionDenied = 1,
    kGroupFull = 2,
    kGroupClosedByOrganizationPolicy = 3,
    kOtherFailure = 4
  };

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

  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Called when the group data model has been loaded. Use
    // DataSharingService::IsGroupDataModelLoaded() to check if the model has
    // been already loaded before starting to observe the service.
    virtual void OnGroupDataModelLoaded() {}

    // Called when the group data model has been changed.
    virtual void OnGroupChanged(const GroupData& group_data,
                                const base::Time& event_time) {}
    // User either created a new group or has been invited to the existing one.
    virtual void OnGroupAdded(const GroupData& group_data,
                              const base::Time& event_time) {}
    // Either group has been deleted or user has been removed from the group.
    virtual void OnGroupRemoved(const GroupId& group_id,
                                const base::Time& event_time) {}

    // Two methods below are called in addition to OnGroupChanged().
    // Called when a new member has been added to the group.
    virtual void OnGroupMemberAdded(const GroupId& group_id,
                                    const GaiaId& member_gaia_id,
                                    const base::Time& event_time) {}
    // Called when a member has been removed from the group.
    virtual void OnGroupMemberRemoved(const GroupId& group_id,
                                      const GaiaId& member_gaia_id,
                                      const base::Time& event_time) {}

    // Called to notify of the sync bridge state changes, e.g. whether initial
    // merge or disable sync are in progress. Interested consumers can choose
    // to ignore incoming sync events during this duration.
    virtual void OnSyncBridgeUpdateTypeChanged(
        SyncBridgeUpdateType sync_bridge_update_type) {}

    // Invoked when the DataSharingService is being destroyed. Give the subclass
    // a chance to cleanup.
    virtual void OnDataSharingServiceDestroyed() {}
  };

  using GroupDataOrFailureOutcome =
      base::expected<GroupData, PeopleGroupActionFailure>;
  using GroupsDataSetOrFailureOutcome =
      base::expected<std::set<GroupData>, PeopleGroupActionFailure>;
  using SharedDataPreviewOrFailureOutcome =
      base::expected<SharedDataPreview, DataPreviewActionFailure>;

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
  // Returns true if the group data model has been loaded. Read APIs will return
  // empty results if the model is not loaded.
  virtual bool IsGroupDataModelLoaded() = 0;

  // Synchronously reads a group from the local storage. Returns nullopt if the
  // group doesn't exist, it has not been fetched from the server yet, or the
  // model is not loaded yet.
  virtual std::optional<GroupData> ReadGroup(const GroupId& group_id) = 0;

  // Synchronously reads all groups from the local storage. Returns empty set
  // if the groups haven't been fetched from the server yet, or the model is not
  // loaded yet.
  virtual std::set<GroupData> ReadAllGroups() = 0;

  // Synchronously reads partial group member data either from the group store
  // or from the special database that stores partial data of removed members.
  // Returns nullopt if no data is found.
  virtual std::optional<GroupMemberPartialData> GetPossiblyRemovedGroupMember(
      const GroupId& group_id,
      const GaiaId& member_gaia_id) = 0;

  // Provides lookup functionality for groups that were known at some point
  // during the current session, but have been deleted. This does not look at
  // currently available groups, for that you should use `ReadGroup`.
  virtual std::optional<GroupData> GetPossiblyRemovedGroup(
      const GroupId& group_id) = 0;

  // Refreshes data if necessary and passes the GroupData to `callback`.
  // Deprecated: use synchronous ReadGroup() above instead.
  virtual void ReadGroupDeprecated(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Attempt to read a group that the user is not member of. This does not
  // refresh the cached data. Returns the group data on success.
  virtual void ReadNewGroup(
      const GroupToken& token,
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

  // Attempts to leave a group the current user has joined before.
  virtual void LeaveGroup(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Returns whether the current user has attempted to leave or delete a group
  // in the current session that they had joined or created before. This is
  // different than if the member is removed from the group by someone else.
  // Returns true for the entire current session even after leave / delete
  // attempt has been committed.
  virtual bool IsLeavingOrDeletingGroup(const GroupId& group_id) = 0;

  // Returns group events since the DataSharingService was started. This is
  // similar to events exposed to Observers, but allows to collect changes by
  // observer that were created after DataSharingService was started.
  virtual std::vector<GroupEvent> GetGroupEventsSinceStartup() = 0;

  // DEPRECATED: Called when a data sharing type URL has been intercepted.
  // Called when a data sharing type URL has been intercepted.
  virtual void HandleShareURLNavigationIntercepted(
      const GURL& url,
      std::unique_ptr<ShareURLInterceptionContext> context) = 0;

  // Create a data sharing URL used for sharing. This does not validate if the
  // group is still active nor guarantee that the URL is not expired. The caller
  // needs to get the valid group info from the other APIs above. Make sure
  // EnsureGroupVisibility API is called before getting the URL for the group.
  virtual std::unique_ptr<GURL> GetDataSharingUrl(
      const GroupData& group_data) = 0;

  // This ensures that the group is open for new members to join. Only owner can
  // call this API. The owner must always call this API before
  // GetDataSharingUrl().
  virtual void EnsureGroupVisibility(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Gets a preview of the shared entities. The returned result may contain
  // all types of shared entities for the group.
  virtual void GetSharedEntitiesPreview(
      const GroupToken& group_token,
      base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
          callback) = 0;

  // Gets avatar image for the given `avatar_url. It's by default cropped into a
  // circle where the diameter will be set to the `size`.
  // TODO(crbug.com/382127659): Shouldn't force UI to pass in an image_fetcher.
  virtual void GetAvatarImageForURL(
      const GURL& avatar_url,
      int size,
      base::OnceCallback<void(const gfx::Image&)> callback,
      image_fetcher::ImageFetcher* image_fetcher) = 0;

  // Sets the current DataSharingSDKDelegate instance.
  virtual void SetSDKDelegate(
      std::unique_ptr<DataSharingSDKDelegate> sdk_delegate) = 0;

  // Get the current SDK Delegate instance.
  virtual DataSharingSDKDelegate* GetSDKDelegate() = 0;

  // Sets the current DataSharingUIDelegate instance.
  virtual void SetUIDelegate(
      std::unique_ptr<DataSharingUIDelegate> ui_delegate) = 0;

  // Get the current DataSharingUIDelegate instance.
  virtual DataSharingUIDelegate* GetUiDelegate() = 0;

  virtual Logger* GetLogger() = 0;

  // Sets a group for testing. When ReadGroup is called, the GroupData that
  // matches GroupId will be returned. This function does not notify observers
  // of the group being added. Settings 2 groups with the same GroupId will
  // replace the existing GroupData.
  virtual void AddGroupDataForTesting(GroupData group_data) = 0;

  // Getter/setter for the preview proxy to allow override in tests.
  virtual void SetPreviewServerProxyForTesting(
      std::unique_ptr<PreviewServerProxy> preview_server_proxy) = 0;
  virtual PreviewServerProxy* GetPreviewServerProxyForTesting() = 0;

  // Called when a collaboration group is removed by the user locally. This
  // happens when user leaves or deletes a group.
  virtual void OnCollaborationGroupRemoved(const GroupId& group_id) = 0;

  // Returns synchronous whether the `context_id` is shared based on local data.
  // If a migration is ongoing, return the last stable state (e.g. If a
  // migration is ongoing from private to shared, then it would return false).
  virtual bool IsContextIdShared(const ContextId& context_id) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_
