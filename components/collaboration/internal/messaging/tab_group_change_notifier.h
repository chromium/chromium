// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_H_

#include "base/observer_list_types.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace collaboration::messaging {

// The TabGroupChangeNotifier is a class that listens to changes from
// TabGroupSyncService and passes them on to its own observers as delta updates
// of the tab group and tabs.
class TabGroupChangeNotifier
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  // An delta observer that is invoked asynchronously based on updates from the
  // TabGroupSyncService.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the service has completed initialization. It is expected
    // that this class is controlled by whoever is acting as the observer, and
    // as such, it is expected that the class has already added its own
    // observer before calling Initialize().
    virtual void OnTabGroupChangeNotifierInitialized() = 0;

    // A new tab group was added.
    virtual void OnTabGroupAdded(
        const tab_groups::SavedTabGroup& added_group) = 0;
    // Tab group was removed, and the old group is provided.
    virtual void OnTabGroupRemoved(tab_groups::SavedTabGroup removed_group) = 0;
    // Called whenever there is a relevant update to tab group name.
    // Note: This is not invoked on tab changes within a tab group.
    virtual void OnTabGroupNameUpdated(
        const tab_groups::SavedTabGroup& updated_group) = 0;
    // Called whenever there is a relevant update to tab group color.
    // Note: This is not invoked on tab changes within a tab group.
    virtual void OnTabGroupColorUpdated(
        const tab_groups::SavedTabGroup& updated_group) = 0;

    // A new tab was added to the given tab group.
    virtual void OnTabAdded(const tab_groups::SavedTabGroupTab& added_tab) = 0;
    // A tab was removed from the given tab group. The last known information
    // about the tab is provided.
    virtual void OnTabRemoved(tab_groups::SavedTabGroupTab removed_tab) = 0;
    // A tab has been updated.
    virtual void OnTabUpdated(
        const tab_groups::SavedTabGroupTab& updated_tab) = 0;
    // A tab has been selected. The parameter is empty if the selected tab could
    // not be found, i.e. it is not part of a shared tab group.
    virtual void OnTabSelected(
        std::optional<tab_groups::SavedTabGroupTab> selected_tab) = 0;
  };

  ~TabGroupChangeNotifier() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Kicks off the initialization of this component.
  virtual void Initialize() = 0;

  // Whether this instance has finished initialization.
  virtual bool IsInitialized() = 0;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_H_
