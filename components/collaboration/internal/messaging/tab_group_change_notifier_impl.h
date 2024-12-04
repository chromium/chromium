// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_

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

namespace collaboration::messaging {

// The TabGroupChangeNotifier is a class that listens to changes from
// TabGroupSyncService and passes them on to its own observers as delta updates
// of the tab group and tabs.
class TabGroupChangeNotifierImpl : public TabGroupChangeNotifier {
 public:
  explicit TabGroupChangeNotifierImpl(
      tab_groups::TabGroupSyncService* tab_group_sync_service);
  ~TabGroupChangeNotifierImpl() override;

  // TabGroupChangeNotifier.
  void AddObserver(TabGroupChangeNotifier::Observer* observer) override;
  void RemoveObserver(TabGroupChangeNotifier::Observer* observer) override;
  void Initialize() override;
  bool IsInitialized() override;

 private:
  // TabGroupSyncService::Observer.
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;
  void OnTabSelected(const std::optional<base::Uuid>& sync_tab_group_id,
                     const std::optional<base::Uuid>& sync_tab_id) override;

  // Fetches the current state of the tab group model, and compares it to what
  // was previously known, publishing any changes that are found.
  void ProcessChangesSinceStartup();

  void NotifyTabGroupChangeNotifierInitializedAndProcessChanges();

  // Processes updates to group metadata and tabs within a group.
  void ProcessTabGroupUpdates(const tab_groups::SavedTabGroup& before,
                              const tab_groups::SavedTabGroup& after);

  // Looks for the selected tab within our last known shared tab groups and
  // returns it if found, else returns std::nullopt.
  std::optional<tab_groups::SavedTabGroupTab> GetSelectedSharedTabForPublishing(
      const std::optional<base::Uuid>& sync_tab_group_id,
      const std::optional<base::Uuid>& sync_tab_id);

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

  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_observer_{this};

  base::WeakPtrFactory<TabGroupChangeNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
