// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/proto/url_restriction.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {
class CollaborationFinder;
class TabGroupSyncDelegate;
class TabGroupSyncMetricsLogger;
class VersioningMessageController;

// A RAII class that pauses local tab model observers when required.
class ScopedLocalObservationPauser {
 public:
  ScopedLocalObservationPauser() = default;
  virtual ~ScopedLocalObservationPauser() = default;
};

// The core service class for handling tab group sync across devices. Provides
// mutation methods to propagate local changes to remote and observer interface
// to propagate remote changes to the local client.
class TabGroupSyncService : public KeyedService, public base::SupportsUserData {
 public:
  // Observers observing updates to the sync data which can be originated by
  // either the local or remote clients.
  class Observer : public base::CheckedObserver {
   public:
    // The data from sync DataTypeStore has been loaded to memory.
    virtual void OnInitialized() {}

    // The service is about to be destroyed. Ensures observers have a chance to
    // remove references before service destruction.
    virtual void OnWillBeDestroyed() {}

    // A new tab group was added at the given |source|.
    virtual void OnTabGroupAdded(const SavedTabGroup& group,
                                 TriggerSource source) {}

    // An existing tab group was updated at the given |source|.
    // Called whenever there are an update to a tab group, which can be title,
    // color, position, pinned state, update to any of its tabs, or when a group
    // was shared and migrated from the originating saved tab group.
    virtual void OnTabGroupUpdated(const SavedTabGroup& group,
                                   TriggerSource source) {}

    // Observer methods that notify before and after applying a sync change.
    // After Android java-to-native migration is complete, we will merge
    // OnTabGroupUpdated with AfterTabGroupUpdateFromRemote since they are
    // essentially the same.

    // Invoked before applying a remote update to the local tab model so that
    // the observers have a chance to cache the previous state of the world.
    // Only invoked for remote updates.
    virtual void BeforeTabGroupUpdateFromRemote(
        const base::Uuid& sync_group_id) {}

    // Invoked after applying a remote update to the local tab model.
    // Only invoked for remote updates.
    virtual void AfterTabGroupUpdateFromRemote(
        const base::Uuid& sync_group_id) {}

    // The local tab group corresponding to the |local_id| was removed.
    virtual void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                                   TriggerSource source) {}

    // Tab group corresponding to the |sync_id| was removed. Only used by the
    // revisit surface that needs to show both open and closed tab groups.
    // All other consumers should use the local ID variant of this method.
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id,
                                   TriggerSource source) {}

    // Invoked whenever there is a change in the set of active tabs across all
    // browser windows. Can include the same set of tabs across two invocations.
    // It's the responsibility of the observer to figure out the diff between
    // two updates.
    virtual void OnTabSelected(const std::set<LocalTabID>& selected_tabs) {}

    // Invoked when the last_seen_time for a shared tab has been updated.
    // This happens either when the user activates a tab locally or the
    // model is updated from the account data sync bridge.
    virtual void OnTabLastSeenTimeChanged(const base::Uuid& tab_id,
                                          TriggerSource source) {}

    // The existing SavedTabGroup has been replaced by a new one. This happens
    // when the originating SavedTabGroup was transitioned to a shared one. The
    // old group is not accessible from the service anymore. This method is
    // useful when observers store group's sync ID to update to a new one. Note
    // that OnTabGroupUpdated() is called afterwards, so the observers don't
    // have to always handle this event.
    virtual void OnTabGroupMigrated(const SavedTabGroup& new_group,
                                    const base::Uuid& old_sync_id,
                                    TriggerSource source) {}

    // The local ID for a tab group was changed. This is usually fired when the
    // group is opened, closed (not always), or (desktop only) restored from
    // session restore. Not all the cases where the tab group is closed is
    // covered.
    virtual void OnTabGroupLocalIdChanged(
        const base::Uuid& sync_id,
        const std::optional<LocalTabGroupID>& local_id) {}

    // (desktop only) The ordering of tab groups in the bookmarks bar UI has
    // changed. Update the UI to reflect the new ordering.
    virtual void OnTabGroupsReordered(TriggerSource source) {}

    // Called to notify of the sync bridge state changes, e.g. whether initial
    // merge or disable sync are in progress. Invoked only for shared tab group
    // bridge.
    virtual void OnSyncBridgeUpdateTypeChanged(
        SyncBridgeUpdateType sync_bridge_update_type) {}
  };

  enum class TabGroupSharingResult {
    kSuccess,
    kTimedOut,
  };

  using TabGroupSharingCallback =
      base::OnceCallback<void(TabGroupSharingResult)>;

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type TabGroupSyncService for the given
  // TabGroupSyncService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      TabGroupSyncService* tab_group_sync_service);
#endif  // BUILDFLAG(IS_ANDROID)

  TabGroupSyncService() = default;
  ~TabGroupSyncService() override = default;

  // Disallow copy/assign.
  TabGroupSyncService(const TabGroupSyncService&) = delete;
  TabGroupSyncService& operator=(const TabGroupSyncService&) = delete;

  // Called to set a delegate that will manage all interactions with the tab
  // model UI layer.
  virtual void SetTabGroupSyncDelegate(
      std::unique_ptr<TabGroupSyncDelegate> delegate) = 0;

  // Mutator methods invoked to notify the service about the local changes.
  // The service will notify the observers accordingly, i.e. notify sync to
  // propagate the changes to server side, and notify any UI observers such
  // as revisit surface to update their UI accordingly.

  // Mutator methods that result in group metadata mutation.
  virtual void AddGroup(SavedTabGroup group) = 0;
  virtual void RemoveGroup(const LocalTabGroupID& local_id) = 0;
  virtual void RemoveGroup(const base::Uuid& sync_id) = 0;
  virtual void UpdateVisualData(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data) = 0;
  // Updates the pinned state of the group when `is_pinned` is provided.
  // Updates the index of the group when `new_index` is provided.
  virtual void UpdateGroupPosition(const base::Uuid& sync_id,
                                   std::optional<bool> is_pinned,
                                   std::optional<int> new_index) = 0;

  // Update bookmark node id of the tab group.
  // This is used to connect/disconnect bookmark folder with a saved tab group.
  virtual void UpdateBookmarkNodeId(
      const base::Uuid& sync_id,
      std::optional<base::Uuid> bookmark_node_id) = 0;

  // Mutator methods that result in tab metadata mutation.
  virtual void AddTab(const LocalTabGroupID& group_id,
                      const LocalTabID& tab_id,
                      const std::u16string& title,
                      const GURL& url,
                      std::optional<size_t> position) = 0;

  // Method to add a tab with the specified url and title
  // to a saved tab group that is not open.
  virtual void AddUrl(const base::Uuid& sync_id,
                      const std::u16string& title,
                      const GURL& url) = 0;

  virtual void RemoveTab(const LocalTabGroupID& group_id,
                         const LocalTabID& tab_id) = 0;
  virtual void MoveTab(const LocalTabGroupID& group_id,
                       const LocalTabID& tab_id,
                       int new_group_index) = 0;

  // Methods to update an existing tab. The primary method is `NavigateTab`
  // which is invoked for local navigations which normally result in an update
  // event sent to sync. On the other hand, `UpdateTabProperties` is reserved to
  // be used for notifying non-navigation changes such as updating the URL
  // redirect chain etc, which doesn't result in an update event sent to sync.
  virtual void NavigateTab(const LocalTabGroupID& group_id,
                           const LocalTabID& tab_id,
                           const GURL& url,
                           const std::u16string& title) = 0;
  virtual void UpdateTabProperties(
      const LocalTabGroupID& group_id,
      const LocalTabID& tab_id,
      const SavedTabGroupTabBuilder& tab_builder) = 0;

  // Invoked to keep track of the currently selected tab info which is used by
  // messaging backend.
  // TODO(crbug.com/362092886): Currently this is not invoked on desktop and
  // also not invoked for non-grouped tabs. This needs to be fixed.
  virtual void OnTabSelected(const std::optional<LocalTabGroupID>& group_id,
                             const LocalTabID& tab_id,
                             const std::u16string& title) = 0;

  // SaveGroup / UnsaveGroup are temporary solutions used during desktop's
  // migration. Other clients should use AddGroup / RemoveGroup.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  virtual void SaveGroup(SavedTabGroup group) = 0;
  virtual void UnsaveGroup(const LocalTabGroupID& local_id) = 0;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Mutator methods for shared tab groups.
  // Converts the saved tab group to shared tab group and associates it with the
  // given `collaboration_id` (this is the same as data_sharing::GroupId). The
  // tab group must not be shared. `callback` will be called with the result if
  // provided.
  virtual void MakeTabGroupShared(
      const LocalTabGroupID& local_group_id,
      const syncer::CollaborationId& collaboration_id,
      TabGroupSharingCallback callback) = 0;
  // For testing only. This is needed to test shared tab groups flow without
  // depending on real people groups from data sharing service backend.
  virtual void MakeTabGroupSharedForTesting(
      const LocalTabGroupID& local_group_id,
      const syncer::CollaborationId& collaboration_id) = 0;
  virtual void MakeTabGroupUnsharedForTesting(
      const LocalTabGroupID& local_group_id) = 0;

  // Mutator methods for shared tab groups.
  // Starts the process of converting a shared tab group to saved tab group. Due
  // to network, Chrome will need to wait for server confirmation before the
  // conversion completes successfully. The tab group must be shared when
  // calling this. `on_complete_callback` will be called on completion.
  virtual void AboutToUnShareTabGroup(
      const LocalTabGroupID& local_group_id,
      base::OnceClosure on_complete_callback) = 0;

  // Called when server confirms that the shared tab group has become private
  // or when unshare fails due to some errors.
  virtual void OnTabGroupUnShareComplete(const LocalTabGroupID& local_group_id,
                                         bool success) = 0;

  // Called when a collaboration group is removed. This call will mark the
  // shared group associated with the collaboration as hidden. The actual group
  // deletion happens in the server in response to the collaboration group
  // deletion. This trickles back to the sync bridge thereby removing the tab
  // group from the model.
  virtual void OnCollaborationRemoved(
      const syncer::CollaborationId& collaboration_id) = 0;

  // Accessor methods.
  // ReadAllGroups and GetAllGroups both return the same list of groups,
  // filtered by whether they should be exposed to external callers.
  // ReadAllGroups should be used by default since it doesnt require copying
  // unless there is a specific reason for using GetAllGroups.
  // Note that the pointers returned by ReadAllGroups are affected by any
  // insertion or deletion operations on the tab group, so don't hold the this
  // vector while doing any insertion deletion, or use this pointer across
  // multiple calls to ReadAllGroups.
  virtual std::vector<const SavedTabGroup*> ReadAllGroups() const = 0;
  virtual std::vector<SavedTabGroup> GetAllGroups() const = 0;

  // Returns groups (even if they would be filtered out in Get/ReadAllGroups).
  virtual std::optional<SavedTabGroup> GetGroup(
      const base::Uuid& guid) const = 0;
  virtual std::optional<SavedTabGroup> GetGroup(
      const LocalTabGroupID& local_id) const = 0;
  virtual std::optional<SavedTabGroup> GetGroup(
      const EitherGroupID& either_id) const = 0;
  virtual std::vector<LocalTabGroupID> GetDeletedGroupIds() const = 0;
  virtual std::optional<std::u16string>
  GetTitleForPreviouslyExistingSharedTabGroup(
      const syncer::CollaborationId& collaboration_id) const = 0;

  // Method invoked from UI to open a remote tab group in the local tab model.
  virtual std::optional<LocalTabGroupID> OpenTabGroup(
      const base::Uuid& sync_group_id,
      std::unique_ptr<TabGroupActionContext> context) = 0;

  // Book-keeping methods to maintain in-memory mapping of sync and local IDs.
  // `opening_source` and `closing_source` refer to the user actions and
  // callsites that result in invoking these methods.
  virtual void UpdateLocalTabGroupMapping(const base::Uuid& sync_id,
                                          const LocalTabGroupID& local_id,
                                          OpeningSource opening_source) = 0;
  virtual void RemoveLocalTabGroupMapping(const LocalTabGroupID& local_id,
                                          ClosingSource closing_source) = 0;
  virtual void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                                const base::Uuid& sync_tab_id,
                                const LocalTabID& local_tab_id) = 0;

  // Only under certain circumstances. Called from the UI layer to reestablish
  // the connection between a local tab group and saved tab group. Don't call
  // this method if you can get what you want via `UpdateLocalTabGroupMapping`,
  // `AddGroup`, or `OpenTabGroup`. Currently invoked from the following places:
  // 1. Session restore in desktop.
  // 2. Undo tab group closure on iOS.
  // 3. Saved to Shared tab group conversion.
  // Invoking this method would update the mapping for tab group, individual
  // tabs, and (on desktop) recreate the tab group listeners. `opening_source`
  // refers to the callsite that results in invoking this method.
  // Note that this method does not update the local tab group, and must be
  // invoked only if the number of local tabs is more or equal to saved tabs.
  virtual void ConnectLocalTabGroup(const base::Uuid& sync_id,
                                    const LocalTabGroupID& local_id,
                                    OpeningSource opening_source) = 0;

  // Attribution related methods.
  // Helper method to determine whether a given cache guid corresponds to a
  // remote device. Empty value or string is considered local device.
  virtual bool IsRemoteDevice(
      const std::optional<std::string>& cache_guid) const = 0;

  // Returns whether a tab group with the given `sync_tab_group_id` was
  // previously closed on this device. Reset to false whenever the user opens
  // the group intentionally.
  virtual bool WasTabGroupClosedLocally(
      const base::Uuid& sync_tab_group_id) const = 0;

  // Method to find out the currently selected tabs from the tab model.
  // Result contains the set of selected tabs from all open browser windows.
  virtual std::set<LocalTabID> GetSelectedTabs();

  // Method to find out the current title of a live tab in the tab model.
  virtual std::u16string GetTabTitle(const LocalTabID& local_tab_id);

  // Helper method to record metrics for certain tab group events.
  // While metrics are implicitly recorded in the native for most of the tab
  // group events, there are certain events that don't have a clean way of
  // passing additional information from the event source call site. That's
  // where this method comes in handy that can be directly invoked from the
  // event source call site i.e. UI layer. Currently required to record open and
  // close tab group events only, but see implementation for more details.
  virtual void RecordTabGroupEvent(const EventDetails& event_details) = 0;

  // Method to update the archival status via timestamp of the local tab group.
  // No timestamp indicates that the tab group is not currently archived.
  virtual void UpdateArchivalStatus(const base::Uuid& sync_id,
                                    bool archival_status) = 0;

  // Method to update the last seen timestamp for a tab. This method exists for
  // external callers such as messaging card dismiss button to be able to clear
  // the dots of all unseen tabs without actually switching to the tabs.
  virtual void UpdateTabLastSeenTime(const base::Uuid& group_id,
                                     const base::Uuid& tab_id,
                                     TriggerSource source) = 0;

  // For accessing the centralized metrics logger.
  virtual TabGroupSyncMetricsLogger* GetTabGroupSyncMetricsLogger() = 0;

  // For connecting to sync engine.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() = 0;
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() = 0;
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupAccountControllerDelegate() = 0;

  // Helper method to pause / resume local observer.
  virtual std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() = 0;

  using UrlRestrictionCallback =
      base::OnceCallback<void(const std::optional<proto::UrlRestriction>&)>;
  // Get the restrictions on a given URL.
  virtual void GetURLRestriction(const GURL& url,
                                 UrlRestrictionCallback callback) = 0;

  // The list of shared tab groups is stored on startup before any local changes
  // have been applied, which enables the messaging system to safely calculate
  // deltas for changes to groups without keeping its own persistence layer.
  // For now there is only a single user of this, so we give away ownership.
  // If there are more users in the future, we should keep the data around in
  // this service.
  virtual std::unique_ptr<std::vector<SavedTabGroup>>
  TakeSharedTabGroupsAvailableAtStartupForMessaging() = 0;

  // Returns if shared tab group existed during startup. If
  // `open_shared_tab_groups` is true, returns whether there were open shared
  // tab groups during startup.
  virtual bool HadSharedTabGroupsLastSession(bool open_shared_tab_groups) = 0;

  // Called when the last tab in a group is closed.
  virtual void OnLastTabClosed(const SavedTabGroup& saved_tab_group) = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the versioning message controller which is responsible for business
  // logic related to shared tab groups versioning related messages.
  virtual VersioningMessageController* GetVersioningMessageController() = 0;

  // For testing only. This is needed to test the API calls received before
  // service init as we need to explicitly un-initialize the service for these
  // scenarios. When calling this method the MessagingBackendService will need
  // to be faked or have its store callbacks set first. (see
  // EmptyMessagingBackendService)
  virtual void SetIsInitializedForTesting(bool initialized) {}

  // For testing only. This is needed to test shared tab groups flow without
  // depending on real people groups from data sharing service backend.
  virtual CollaborationFinder* GetCollaborationFinderForTesting();
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_
