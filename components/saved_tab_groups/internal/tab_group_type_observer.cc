// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_type_observer.h"

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace tab_groups {
TabGroupTypeObserver::TabGroupTypeObserver(
    TabGroupSyncService* service,
    SyntheticFieldTrialHelper* synthetic_field_trial_helper)
    : tab_group_sync_service_(service),
      synthetic_field_trial_helper_(synthetic_field_trial_helper) {
  obs_.Observe(service);
}

TabGroupTypeObserver::~TabGroupTypeObserver() = default;

void TabGroupTypeObserver::OnInitialized() {
  const std::vector<const SavedTabGroup*> tab_groups =
      tab_group_sync_service_->ReadAllGroups();
  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(
      !tab_groups.empty());
  for (const SavedTabGroup* group : tab_groups) {
    if (group->is_shared_tab_group()) {
      synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(true);
      return;
    }
  }
  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(false);
}

void TabGroupTypeObserver::OnWillBeDestroyed() {
  obs_.Reset();
}

void TabGroupTypeObserver::OnTabGroupAdded(const SavedTabGroup& group,
                                           TriggerSource source) {
  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(true);

  if (group.is_shared_tab_group()) {
    synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(true);
  }
}

void TabGroupTypeObserver::OnTabGroupMigrated(const SavedTabGroup& new_group,
                                              const base::Uuid& old_sync_id,
                                              TriggerSource source) {
  if (new_group.is_shared_tab_group()) {
    synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(true);
  }
}

}  // namespace tab_groups
