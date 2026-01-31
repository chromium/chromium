// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"

namespace ntp_prefs {
// Tracks whether the user has chosen to hide the shortcuts tiles on the NTP.
const char kNtpShortcutsVisible[] = "ntp.shortcust_visible";
// Tracks whether the user has chosen to use custom links or most visited sites
// for the shortcut tiles on the NTP. This pref is migrated to
// `kNtpShortcutsType`.
const char kNtpUseMostVisitedTiles[] = "ntp.use_most_visited_tiles";
// Tracks what type of shortcuts tiles to show. Values must stay in sync with
// `TileType` enum (0 = TopSites, 1 = CustomLinks). This pref is migrated to
// `kNtpCustomLinksVisible` and `kNtpEnterpriseShortcutsVisible`.
const char kNtpShortcutsType[] = "ntp.shortcuts_type";
// Tracks whether the user has chosen to show custom links on the NTP.
// When false, the user has chosen to show top sites.
const char kNtpCustomLinksVisible[] = "ntp.custom_links_visible";
// Tracks whether the user has chosen to hide the enterprise shortcuts tiles on
// the NTP.
const char kNtpEnterpriseShortcutsVisible[] =
    "ntp.enterprise_shortcuts_visible";
// Tracks whether the user has chosen to hide the personal tiles on the NTP.
// Used when enterprise shortcuts are enabled and mixed with personal tiles.
const char kNtpPersonalShortcutsVisible[] = "ntp.personal_shortcuts_visible";
// Tracks whether the user has chosen to show all most visited tiles on the NTP.
const char kNtpShowAllMostVisitedTiles[] = "ntp.show_all_most_visited_tiles";
// Tracks the last time the staleness counter was updated for shortcuts.
const char kNtpLastShortcutsStalenessUpdate[] =
    "ntp.last_shortcuts_staleness_update";
// Tracks the staleness counter for shortcuts.
const char kNtpShortcutsStalenessCount[] = "ntp.shortcuts_staleness_count";
// Tracks whether shortcuts auto-removal for inactivity is disabled.
const char kNtpShortcutsAutoRemovalDisabled[] =
    "ntp.shortcuts_auto_removal_disabled";
// Tracks the last time the staleness counter was updated for modules.
const char kNtpLastModuleStalenessUpdate[] = "ntp.last_module_staleness_update";
// Tracks the staleness counter for each module id.
const char kNtpModuleStalenessCountDict[] = "ntp.module_staleness_count_dict";
// Tracks whether module auto-removal for inactivity is disabled for each
// module. A special key of "all_modules" indicates that the auto-removal is
// disabled for all modules.
const char kNtpModulesAutoRemovalDisabledDict[] =
    "ntp.modules_auto_removal_disabled";
// Tracks the number of times the user has clicked on the context menu
// entrypoint when the context menu description is displayed.
const char kNtpContextMenuClickCount[] = "ntp.context_menu_click_count";
}  // namespace ntp_prefs
