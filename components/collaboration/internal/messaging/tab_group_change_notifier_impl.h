// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_

#include <set>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace collaboration::messaging {

// The concrete implementation of the `TabGroupChangeNotifier`.
class TabGroupChangeNotifierImpl : public TabGroupChangeNotifier {
 public:
  explicit TabGroupChangeNotifierImpl(
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      signin::IdentityManager* identity_manager);
  ~TabGroupChangeNotifierImpl() override;

  // TabGroupChangeNotifier.
  void AddObserver(TabGroupChangeNotifier::Observer* observer) override;
  void RemoveObserver(TabGroupChangeNotifier::Observer* observer) override;
  void Initialize() override;
  bool IsInitialized() override;

  void OnTabGroupOpenedOrClosed(
      const base::Uuid& sync_id,
      const std::optional<tab_groups::LocalTabGroupID>& local_id);

 private:
  // TabGroupSyncService::Observer.
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void BeforeTabGroupUpdateFromRemote(const base::Uuid& sync_group_id) override;
  void AfterTabGroupUpdateFromRemote(const base::Uuid& sync_group_id) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;
  void OnTabSelected(
      const std::set<tab_groups::LocalTabID>& selected_tabs) override;
  void OnTabLastSeenTimeChanged(const base::Uuid& tab_id,
                                tab_groups::TriggerSource source) override;
  void OnTabGroupLocalIdChanged(
      const base::Uuid& sync_id,
      const std::optional<tab_groups::LocalTabGroupID>& local_id) override;
  void OnSyncBridgeUpdateTypeChanged(
      tab_groups::SyncBridgeUpdateType sync_bridge_update_type) override;

  // Fetches the current state of the tab group model, and compares it to what
  // was previously known, publishing any changes that are found.
  void ProcessChangesSinceStartup();

  void NotifyTabGroupChangeNotifierInitializedAndProcessChanges();

  // Processes updates to group metadata and tabs within a group.
  void ProcessTabGroupUpdates(const tab_groups::SavedTabGroup& before,
                              const tab_groups::SavedTabGroup& after,
                              tab_groups::TriggerSource source);

  // Common code for processing OnTabGroupUpdated event (for both remote and
  // local events). For remote events, by this time the tab model UI already had
  // a chance to apply the incoming sync update so that TabGroupSyncService has
  // the most updated state (e.g. local tab IDs).
  void OnTabGroupUpdatedInner(const base::Uuid& sync_tab_group_id,
                              tab_groups::TriggerSource source);

  // Whether the sync bridge is currently undergoing an initial merge or disable
  // sync. Updates to the tab groups should be ignored during this period.
  bool IsInProgressInitialMergeOrDisableSync();

  std::unordered_map<base::Uuid, tab_groups::SavedTabGroup, base::UuidHash>
  ConvertToMapOfSharedTabGroup(
      const std::vector<tab_groups::SavedTabGroup>& groups);

  // The last known SavedTabGroup dataset.
  std::unordered_map<base::Uuid, tab_groups::SavedTabGroup, base::UuidHash>
      last_known_tab_groups_;

  // Whether the service has already been initialized.
  bool is_initialized_ = false;

  // The list of observers observing this particular class.
  base::ObserverList<TabGroupChangeNotifier::Observer> observers_;

  // The TabGroupSyncService that is the source of the updates.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // Service providing information about sign in.
  raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_observer_{this};

  // Whether shared tab group sync bridge is undergoing initial merge or disable
  // sync (which mostly happens during sign-in / sign-out). During this period,
  // the incoming tab group changes should be ignored which would otherwise
  // create an avalanche of false notifications. A value of std::nullopt means
  // that the service is not in the middle of an initial merge or disable sync
  // and the tab group updates should be processed as normal.
  std::optional<tab_groups::SyncBridgeUpdateType> sync_bridge_update_type_;

  // The last selected tabs across all browser windows.
  std::set<tab_groups::LocalTabID> last_selected_tabs_;

  // Whether sync is currently in the middle of applying an update to the tab
  // model during which we should ignore the tab selection events. We re-read
  // the selected tabs from tab model and notify the observers (messaging
  // backend) about any intermediate change in selection after the sync updated
  // has been applied.
  bool ignore_tab_selection_events_ = false;

  base::WeakPtrFactory<TabGroupChangeNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
