// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_TYPE_OBSERVER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_TYPE_OBSERVER_H_

#include "base/scoped_observation.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
class SyntheticFieldTrialHelper;

// Class for observing types (e.g. shared or saved) of tab groups that has
// been owned by the TabGroupSyncService, and report them to the
// SyntheticFieldTrialHelper if anything changes. Once the TabGroupSyncService
// has owned a saved or shared tab group in the past, removing the saved
// or shared tab group won't have any effects.
class TabGroupTypeObserver : public base::SupportsUserData::Data,
                             public TabGroupSyncService::Observer {
 public:
  TabGroupTypeObserver(TabGroupSyncService* service,
                       SyntheticFieldTrialHelper* synthetic_field_trial_helper);
  ~TabGroupTypeObserver() override;
  TabGroupTypeObserver(const TabGroupTypeObserver&) = delete;
  TabGroupTypeObserver& operator=(const TabGroupTypeObserver&) = delete;

  // TabGroupSyncService::Observer impl.
  void OnInitialized() override;
  void OnWillBeDestroyed() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupMigrated(const SavedTabGroup& new_group,
                          const base::Uuid& old_sync_id,
                          TriggerSource source) override;

 private:
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;
  raw_ptr<SyntheticFieldTrialHelper> synthetic_field_trial_helper_;
  base::ScopedObservation<TabGroupSyncService, TabGroupSyncService::Observer>
      obs_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_TYPE_OBSERVER_H_
