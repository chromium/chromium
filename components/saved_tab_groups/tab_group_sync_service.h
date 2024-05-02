// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {

// Whether the update was originated by a change in the local or remote
// client.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class TriggerSource {
  // The source is a remote chrome client.
  REMOTE = 0,

  // The source is the local chrome client.
  LOCAL = 1,
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
    // The data from sync ModelTypeStore has been loaded to memory.
    virtual void OnInitialized() = 0;

    // A new tab group was added at the given |source|.
    virtual void OnTabGroupAdded(const SavedTabGroup& group,
                                 TriggerSource source) = 0;

    // An existing tab group was updated at the given |source|.
    // Called whenever there are an update to a tab group, which can be title,
    // color, position, pinned state, or update to any of its tabs.
    virtual void OnTabGroupUpdated(const SavedTabGroup& group,
                                   TriggerSource source) = 0;

    // The local tab group corresponding to the |local_id| was removed.
    virtual void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                                   TriggerSource source) = 0;

    // Tab group corresponding to the |sync_id| was removed. Only used by the
    // revisit surface that needs to show both open and closed tab groups.
    // All other consumers should use the local ID variant of this method.
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id,
                                   TriggerSource source) = 0;
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

  // Mutator methods invoked to notify the service about the local changes.
  // The service will notify the observers accordingly, i.e. notify sync to
  // propagate the changes to server side, and notify any UI observers such
  // as revisit surface to update their UI accordingly.

  // Mutator methods that result in group metadata mutation.
  virtual void AddGroup(const SavedTabGroup& group) = 0;
  virtual void RemoveGroup(const LocalTabGroupID& local_id) = 0;
  virtual void RemoveGroup(const base::Uuid& sync_id) = 0;
  virtual void UpdateVisualData(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data) = 0;

  // Mutator methods that result in tab metadata mutation.
  virtual void AddTab(const LocalTabGroupID& group_id,
                      const LocalTabID& tab_id,
                      const std::u16string& title,
                      GURL url,
                      std::optional<size_t> position) = 0;
  virtual void UpdateTab(const LocalTabGroupID& group_id,
                         const LocalTabID& tab_id,
                         const std::u16string& title,
                         GURL url,
                         std::optional<size_t> position) = 0;
  virtual void RemoveTab(const LocalTabGroupID& group_id,
                         const LocalTabID& tab_id) = 0;
  virtual void MoveTab(const LocalTabGroupID& group_id,
                       const LocalTabID& tab_id,
                       int new_group_index) = 0;

  // Accessor methods.
  virtual std::vector<SavedTabGroup> GetAllGroups() = 0;
  virtual std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) = 0;
  virtual std::optional<SavedTabGroup> GetGroup(LocalTabGroupID& local_id) = 0;
  virtual std::vector<LocalTabGroupID> GetDeletedGroupIds() = 0;

  // Book-keeping methods to maintain in-memory mapping of sync and local IDs.
  virtual void UpdateLocalTabGroupMapping(const base::Uuid& sync_id,
                                          const LocalTabGroupID& local_id) = 0;
  virtual void RemoveLocalTabGroupMapping(const LocalTabGroupID& local_id) = 0;
  virtual void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                                const base::Uuid& sync_tab_id,
                                const LocalTabID& local_tab_id) = 0;

  // For connecting to sync engine.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() = 0;
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_H_
