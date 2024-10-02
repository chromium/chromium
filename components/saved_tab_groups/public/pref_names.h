// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_PREF_NAMES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_PREF_NAMES_H_

#include <vector>

#include "build/build_config.h"

namespace signin {
class GaiaIdHash;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace tab_groups::prefs {

// Whether tab groups are syncable across devices.
inline constexpr char kSyncableTabGroups[] = "tabgroup.sync_enabled";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kAutoOpenSyncedTabGroups[] =
    "auto_open_synced_tab_groups";
inline constexpr char kStopShowingTabGroupConfirmationOnClose[] =
    "stop_showing_tab_group_confirmation_on_close";
inline constexpr char kStopShowingTabGroupConfirmationOnUngroup[] =
    "stop_showing_tab_group_confirmation_on_ungroup";
inline constexpr char kStopShowingTabGroupConfirmationOnTabRemove[] =
    "stop_showing_tab_group_confirmation_on_tab_remove";
inline constexpr char kStopShowingTabGroupConfirmationOnTabClose[] =
    "stop_showing_tab_group_confirmation_on_tab_close";
#endif  // BUILDFLAG(IS_ANDROID)

// Boolean which specifies whether the tab group is automatically pinned when
// it's created.
inline constexpr char kAutoPinNewTabGroups[] = "auto_pin_new_tab_groups";

// Whether the DataTypeStore has been migrated from storing
// SavedTabGroupSpecifics to SavedTabGroupData.
inline constexpr char kSavedTabGroupSpecificsToDataMigration[] =
    "saved_tab_groups.specifics_to_data_migration";

// The pref that stores the deleted group IDs that needs to be closed in the
// local tab model. Stores a dictionary of local ID -> sync ID. Local ID is the
// main thing that the tab model needs. On startup, UI queries the deleted IDs
// and closes them up. Sync ID isn't really needed since it's already removed
// from sync and the model, but kept for use in case we find a future use for
// it. When the group is closed, the ID will be deleted from this pref.
inline constexpr char kDeletedTabGroupIds[] =
    "saved_tab_groups.deleted_group_ids";

// Stores the `saved_guid` of remote tab groups that have been closed locally,
// keyed by a hash of the owning user's Gaia ID. If the user signs out and back
// in again, these groups won't automatically be reopened.
inline constexpr char kLocallyClosedRemoteTabGroupIds[] =
    "saved_tab_groups.closed_remote_group_ids";

// Registers the Clear Browsing Data UI prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Drops all account-keyed pref entries for accounts that are *not* listed in
// `available_gaia_ids`.
void KeepAccountSettingsPrefsOnlyForUsers(
    PrefService* pref_service,
    const std::vector<signin::GaiaIdHash>& available_gaia_ids);

}  // namespace tab_groups::prefs

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_PREF_NAMES_H_
