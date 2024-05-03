// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PREF_NAMES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PREF_NAMES_H_

#include "build/build_config.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

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

// Registers the Clear Browsing Data UI prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace tab_groups::prefs

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PREF_NAMES_H_
