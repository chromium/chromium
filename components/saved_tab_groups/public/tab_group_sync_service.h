// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_

#include <memory>
#include <optional>
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
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {
class TabGroupSyncDelegate;
class TabGroupSyncMetricsLogger;

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
    // color, position, pinned state, or update to any of its tabs.
    virtual void OnTabGroupUpdated(const SavedTabGroup& group,
                                   TriggerSource source) {}

    // The local tab group corresponding to the |local_id| was removed.
    virtual void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                                   TriggerSource source) {}

    // Tab group corresponding to the |sync_id| was removed. Only used by the
    // revisit surface that needs to show both open and closed tab groups.
    // All other consumers should use the local ID variant of this method.
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id,
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
  };

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

  // Mutator methods that result in tab metadata mutation.
  virtual void AddTab(const LocalTabGroupID& group_id,
                      const LocalTabID& tab_id,
                      const std::u16string& title,
                      GURL url,
                      std::optional<size_t> position) = 0;
  virtual void UpdateTab(const LocalTabGroupID& group_id,
                         const LocalTabID& tab_id,
                         const SavedTabGroupTabBuilder& tab_builder) = 0;
  virtual void RemoveTab(const LocalTabGroupID& group_id,
                         const LocalTabID& tab_id) = 0;
  virtual void MoveTab(const LocalTabGroupID& group_id,
                       const LocalTabID& tab_id,
                       int new_group_index) = 0;

  // For metrics only.
  virtual void OnTabSelected(const LocalTabGroupID& group_id,
                             const LocalTabID& tab_id) = 0;

  // SaveGroup / UnsaveGroup are temporary solutions used during desktop's
  // migration. Other clients should use AddGroup / RemoveGroup.
  virtual void SaveGroup(SavedTabGroup group) = 0;
  virtual void UnsaveGroup(const LocalTabGroupID& local_id) = 0;

  // Mutator methods for shared tab groups.
  // Converts the saved tab group to shared tab group and associates it with the
  // given `collaboration_id` (this is the same as data_sharing::GroupId). The
  // tab group must not be shared.
  // TODO(crbug.com/351022699): consider using data_sharing::GroupId.
  virtual void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                                  std::string_view collaboration_id) = 0;

  // Accessor methods.
  virtual std::vector<SavedTabGroup> GetAllGroups() = 0;
  virtual std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) = 0;
  virtual std::optional<SavedTabGroup> GetGroup(
      const LocalTabGroupID& local_id) = 0;
  virtual std::vector<LocalTabGroupID> GetDeletedGroupIds() = 0;

  // Method invoked from UI to open a remote tab group in the local tab model.
  virtual void OpenTabGroup(const base::Uuid& sync_group_id,
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

  // Called from the UI layer such as tab group restore from recent tabs or undo
  // tab group closure to reconnect a local tab group to a saved tab group.
  // `opening_source` refers to the callsite that results in invoking this
  // method.
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

  // Helper method to record metrics for certain tab group events.
  // While metrics are implicitly recorded in the native for most of the tab
  // group events, there are certain events that don't have a clean way of
  // passing additional information from the event source call site. That's
  // where this method comes in handy that can be directly invoked from the
  // event source call site i.e. UI layer. Currently required to record open and
  // close tab group events only, but see implementation for more details.
  virtual void RecordTabGroupEvent(const EventDetails& event_details) = 0;

  // For accessing the centralized metrics logger.
  virtual TabGroupSyncMetricsLogger* GetTabGroupSyncMetricsLogger() = 0;

  // For connecting to sync engine.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() = 0;
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() = 0;

  // Helper method to pause / resume local observer.
  virtual std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() = 0;

  using UrlRestrictionCallback =
      base::OnceCallback<void(std::optional<proto::UrlRestriction>)>;
  // Get the restrictions on a given URL.
  virtual void GetURLRestriction(const GURL& url,
                                 UrlRestrictionCallback callback) = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // For testing only. This is needed to test the API calls received before
  // service init as we need to explicitly un-initialize the service for these
  // scenarios.
  virtual void SetIsInitializedForTesting(bool initialized) {}
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TAB_GROUP_SYNC_SERVICE_H_
