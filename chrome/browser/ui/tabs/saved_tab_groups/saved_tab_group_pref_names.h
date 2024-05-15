// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_PREF_NAMES_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_PREF_NAMES_H_

#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Contains all of the SavedTabGroup specific prefs that do are not tied to
// components/saved_tab_groups/.
namespace tab_groups::saved_tab_groups::prefs {

// Boolean pref denoting if we have performed the ui update migration from the
// previous version of SavedTabGroups.
inline constexpr char kTabGroupSavesUIUpdateMigrated[] =
    "tab_group_saves_ui_update_migrated";

// Booleans indicating whether the user had dismissed the dialog with "Dont ask
// again". This value is assumed false, if true the dialog should not show.
inline constexpr char kTabGroupsDeletionSkipDialogOnDelete[] =
    "tab_groups.deletion.skip_dialog_on_delete";
inline constexpr char kTabGroupsDeletionSkipDialogOnUngroup[] =
    "tab_groups.deletion.skip_dialog_on_ungroup";
inline constexpr char kTabGroupsDeletionSkipDialogOnRemoveTab[] =
    "tab_groups.deletion.skip_dialog_on_remove_tab";
inline constexpr char kTabGroupsDeletionSkipDialogOnCloseTab[] =
    "tab_groups.deletion.skip_dialog_on_close_tab";

// Integer that keep track of how many times the learn more footer in the
// TabGroupEditorBubbleView has been seen by the user. Once this value reaches
// 5, stop showing the footer.
inline constexpr char kTabGroupLearnMoreFooterShownCount[] =
    "tab_groups.editor_bubble.learn_more_footer_shown_count";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Helps manage the state of the kTabGroupSavesUIUpdateMigrated migration pref.
bool IsTabGroupSavesUIUpdateMigrated(PrefService* pref_service);
void SetTabGroupSavesUIUpdateMigrated(PrefService* pref_service);

int GetLearnMoreFooterShownCount(PrefService* pref_service);
void IncrementLearnMoreFooterShownCountPref(PrefService* pref_service);

}  // namespace tab_groups::saved_tab_groups::prefs

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_PREF_NAMES_H_
