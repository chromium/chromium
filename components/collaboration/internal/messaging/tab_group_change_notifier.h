// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_H_

#include "base/observer_list_types.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace collaboration::messaging {

// The `TabGroupChangeNotifier` is an interface that observes the
// `TabGroupSyncService` and translates its events into a simplified,
// delta-based format for the messaging backend.
//
// This class is responsible for:
// - Observing tab group and tab changes from the `TabGroupSyncService`.
// - Maintaining the state of observed tab groups and tabs to compute deltas
//   (e.g., added, removed, updated).
// - Notifying its observers of these deltas asynchronously.
//
// The observers of this class are expected to handle the simplified change
// events to update their own state and the UI accordingly.
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

    // Invoked when sync is disabled which includes sign out or disabling sync
    // for tabs. On receiving this event, the messaging service is expected to
    // clear the database, in-memory model, and hide any outstanding messages.
    virtual void OnSyncDisabled() = 0;

    // A new tab group was added.
    virtual void OnTabGroupAdded(const tab_groups::SavedTabGroup& added_group,
                                 tab_groups::TriggerSource source) = 0;
    // Tab group was removed, and the old group is provided.
    virtual void OnTabGroupRemoved(tab_groups::SavedTabGroup removed_group,
                                   tab_groups::TriggerSource source) = 0;
    // Called whenever there is a relevant update to tab group name.
    // Note: This is not invoked on tab changes within a tab group.
    virtual void OnTabGroupNameUpdated(
        const tab_groups::SavedTabGroup& updated_group,
        tab_groups::TriggerSource source) = 0;
    // Called whenever there is a relevant update to tab group color.
    // Note: This is not invoked on tab changes within a tab group.
    virtual void OnTabGroupColorUpdated(
        const tab_groups::SavedTabGroup& updated_group,
        tab_groups::TriggerSource source) = 0;

    // A saved tab group was opened, e.g. from tab group revisit surface.
    virtual void OnTabGroupOpened(
        const tab_groups::SavedTabGroup& tab_group) = 0;

    // A saved tab group was closed.
    virtual void OnTabGroupClosed(
        const tab_groups::SavedTabGroup& tab_group) = 0;

    // A new tab was added to the given tab group.
    virtual void OnTabAdded(const tab_groups::SavedTabGroupTab& added_tab,
                            tab_groups::TriggerSource source) = 0;
    // A tab was removed from the given tab group. The last known information
    // about the tab is provided.
    virtual void OnTabRemoved(tab_groups::SavedTabGroupTab removed_tab,
                              tab_groups::TriggerSource source,
                              bool is_selected) = 0;
    // A tab has been updated.
    virtual void OnTabUpdated(const tab_groups::SavedTabGroupTab& before,
                              const tab_groups::SavedTabGroupTab& after,
                              tab_groups::TriggerSource source,
                              bool is_selected) = 0;
    // A tab was selected or deselected.
    virtual void OnTabSelectionChanged(const tab_groups::LocalTabID& tab_id,
                                       bool is_selected) = 0;

    // A tab is "seen" by a local device or other device. On desktop
    // platforms this is called when the tab enters the active state.
    virtual void OnTabLastSeenTimeChanged(const base::Uuid& tab_id,
                                          tab_groups::TriggerSource source) = 0;
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
