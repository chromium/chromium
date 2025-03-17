// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"

namespace tab_groups {
const char kSyncedTabGroupFieldTrialName[] = "TabGroup_SyncedTabGroup";
const char kSharedTabGroupFieldTrialName[] = "TabGroup_SharedTabGroup";
const char kHasOwnedTabGroupTypeName[] = "Owned";
const char kHasNotOwnedTabGroupTypeName[] = "Unowned";

SyntheticFieldTrialHelper::SyntheticFieldTrialHelper(
    base::RepeatingCallback<void(bool)> on_had_saved_tab_group_changed_callback,
    base::RepeatingCallback<void(bool)>
        on_had_shared_tab_group_changed_callback)
    : on_had_saved_tab_group_changed_callback_(
          on_had_saved_tab_group_changed_callback),
      on_had_shared_tab_group_changed_callback_(
          on_had_shared_tab_group_changed_callback) {}

SyntheticFieldTrialHelper::~SyntheticFieldTrialHelper() = default;

void SyntheticFieldTrialHelper::UpdateHadSavedTabGroupIfNeeded(
    bool had_saved_tab_group) {
  if (had_saved_tab_group_.has_value()) {
    if (had_saved_tab_group_.value() || !had_saved_tab_group) {
      return;
    }
  }

  had_saved_tab_group_ = had_saved_tab_group;
  on_had_saved_tab_group_changed_callback_.Run(had_saved_tab_group);
}

void SyntheticFieldTrialHelper::UpdateHadSharedTabGroupIfNeeded(
    bool had_shared_tab_group) {
  if (had_shared_tab_group_.has_value()) {
    if (had_shared_tab_group_.value() || !had_shared_tab_group) {
      return;
    }
  }

  had_shared_tab_group_ = had_shared_tab_group;
  on_had_shared_tab_group_changed_callback_.Run(had_shared_tab_group);
}

}  // namespace tab_groups
