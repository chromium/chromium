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
#include "ui/gfx/range/range.h"

namespace tab_groups {

// The core service class for handling tab group sync across devices. Provides
// mutation methods to propagate local changes to remote and observer interface
// to propagate remote changes to the local client.
class TabGroupSyncService : public KeyedService, public base::SupportsUserData {
 public:
  // Whether the update was originated by a change in the local or remote
  // client.
  enum TriggerSource {
    // The source is a remote chrome client.
    REMOTE = 0,

    // The source is the local chrome client.
    LOCAL = 1,
  };

  // Observers observing updates to the sync data which can be originated by
  // either the local or remote clients.
  class Observer : public base::CheckedObserver {
   public:
    // A new tab group was added, or an existing tab group was updated at the
    // given |source|.
    virtual void OnTabGroupAddedOrUpdated(const SavedTabGroup& group,
                                          TriggerSource source) = 0;

    // Tab group corresponding to the |sync_id| was removed.
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id) = 0;
  };

  TabGroupSyncService() = default;
  ~TabGroupSyncService() override = default;

  // Disallow copy/assign.
  TabGroupSyncService(const TabGroupSyncService&) = delete;
  TabGroupSyncService& operator=(const TabGroupSyncService&) = delete;

  // Mutator methods invoked to notify the service about the local changes.
  // The service will notify the observers accordingly, i.e. notify sync to
  // propagate the changes to server side, and notify any UI observers such
  // as revisit surface to update their UI accordingly.
  virtual void AddOrUpdateGroup(SavedTabGroup group) = 0;
  virtual void RemoveGroup(const LocalTabGroupID& local_id) = 0;

  // Get methods.
  virtual std::vector<SavedTabGroup> GetAllGroups() = 0;
  virtual std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) = 0;
  virtual std::optional<SavedTabGroup> GetGroup(LocalTabGroupID& local_id) = 0;

  // Book-keeping methods to map the IDs.
  virtual void SetLocalTabGroupIdForSyncId(const base::Uuid& sync_id,
                                           LocalTabGroupID& local_id) = 0;
  virtual base::Uuid GetSyncIdForLocalTabGroupId(LocalTabGroupID& local_id) = 0;
  virtual base::Uuid GetLocalIdForSyncId(const base::Uuid& sync_id) = 0;

  // For connecting to sync engine.
  virtual syncer::ModelTypeSyncBridge* bridge() = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_H_
