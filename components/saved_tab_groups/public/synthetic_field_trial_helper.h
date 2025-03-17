// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SYNTHETIC_FIELD_TRIAL_HELPER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SYNTHETIC_FIELD_TRIAL_HELPER_H_

#include <optional>

#include "base/functional/callback.h"

namespace tab_groups {
// Synthetic field trial name for synced tab group.
extern const char kSyncedTabGroupFieldTrialName[];

// Synthetic field trial name for shared tab group.
extern const char kSharedTabGroupFieldTrialName[];

// Group name indicating this client has owned synced or shared tab groups
// during the session.
extern const char kHasOwnedTabGroupTypeName[];

// Group name indicating this client has not owned synced or shared tab groups
// during the session.
extern const char kHasNotOwnedTabGroupTypeName[];

// Class for updating the synthetic field trial depending on whether the
// client has used saved or shared tab groups. If user has already used
// shared or saved tab groups, removing the groups subsequentlly won't
// have any effects on the syntheric field trial.
class SyntheticFieldTrialHelper {
 public:
  SyntheticFieldTrialHelper(base::RepeatingCallback<void(bool)>
                                on_had_saved_tab_group_changed_callback,
                            base::RepeatingCallback<void(bool)>
                                on_had_shared_tab_group_changed_callback);
  virtual ~SyntheticFieldTrialHelper();
  SyntheticFieldTrialHelper(const SyntheticFieldTrialHelper&) = delete;
  SyntheticFieldTrialHelper& operator=(const SyntheticFieldTrialHelper&) =
      delete;

  // Called to update had_`saved_tab_group_` or `had_shared_tab_group_` if they
  // are unset or are not true. Marked as virtual for testing purposes.
  virtual void UpdateHadSavedTabGroupIfNeeded(bool had_saved_tab_group);
  virtual void UpdateHadSharedTabGroupIfNeeded(bool had_shared_tab_group);

 private:
  // Indicates whether the service had a saved group during the browser session.
  // This will not change once it is set to true.
  std::optional<bool> had_saved_tab_group_;
  // Indicates whether the service had a shared group during the browser
  // session. This will not change once it is set to true.
  std::optional<bool> had_shared_tab_group_;

  base::RepeatingCallback<void(bool)> on_had_saved_tab_group_changed_callback_;
  base::RepeatingCallback<void(bool)> on_had_shared_tab_group_changed_callback_;
};
}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SYNTHETIC_FIELD_TRIAL_HELPER_H_
