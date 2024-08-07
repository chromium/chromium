// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PREF_NAMES_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PREF_NAMES_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace prefs {

// A preference that tracks actions pinned to the toolbar. This is a list.
// The actions are stored by ID.
inline constexpr char kPinnedActions[] = "toolbar.pinned_actions";
// Indicates whether the search companion pin state pref has been migrated to
// the new toolbar container.
inline constexpr char kPinnedSearchCompanionMigrationComplete[] =
    "toolbar.pinned_search_companion_migration_complete";

// Indicates whether the Chrome Labs pin state pref has been migrated to
// the new toolbar container.
inline constexpr char kPinnedChromeLabsMigrationComplete[] =
    "toolbar.pinned_chrome_labs_migration_complete";

}  // namespace prefs

namespace toolbar {

// Registers user preferences related to the toolbar.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace toolbar

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PREF_NAMES_H_
