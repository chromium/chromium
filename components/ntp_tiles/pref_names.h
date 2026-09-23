// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_PREF_NAMES_H_
#define COMPONENTS_NTP_TILES_PREF_NAMES_H_

#include "build/build_config.h"

namespace ntp_tiles::prefs {

// The number of personal tiles we had previously. Used to figure out
// whether we need popular sites.
inline constexpr char kNumPersonalTiles[] = "ntp.num_personal_suggestions";

// If set, overrides the URL for popular sites, including the individual
// overrides for country and version below.
inline constexpr char kPopularSitesOverrideURL[] = "popular_sites.override_url";

// If set, this will override the URL path directory for popular sites.
inline constexpr char kPopularSitesOverrideDirectory[] =
    "popular_sites.override_directory";

// If set, this will override the country detection for popular sites.
inline constexpr char kPopularSitesOverrideCountry[] =
    "popular_sites.override_country";

// If set, this will override the default file version for popular sites.
inline constexpr char kPopularSitesOverrideVersion[] =
    "popular_sites.override_version";

// Prefs used to cache suggested sites and store caching meta data.
inline constexpr char kPopularSitesLastDownloadPref[] =
    "popular_sites_last_download";
inline constexpr char kPopularSitesURLPref[] = "popular_sites_url";
inline constexpr char kPopularSitesJsonPref[] = "suggested_sites_json";
inline constexpr char kPopularSitesVersionPref[] = "suggested_sites_version";

// Prefs used to cache custom links. Each CustomLinksScope gets its own keys so
// that links from one scope are never synced over links from the other.
// CustomLinksScope::kDesktop keys. These keep their historical, unsuffixed
// names so that existing profiles and sync records stay valid.
inline constexpr char kCustomLinksList[] = "custom_links.list";
inline constexpr char kCustomLinksInitialized[] = "custom_links.initialized";
// CustomLinksScope::kMobile keys.
inline constexpr char kCustomLinksListMobile[] = "custom_links_mobile.list";
inline constexpr char kCustomLinksInitializedMobile[] =
    "custom_links_mobile.initialized";

// Prefs used to cache enterprise shortcuts.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
inline constexpr char kEnterpriseShortcutsPolicyList[] =
    "enterprise_shortcuts.policy_list";
inline constexpr char kEnterpriseShortcutsUserList[] =
    "enterprise_shortcuts.user_list";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS)

// Pref used to verify whether custom links have been removed
// for preinstalled default chrome apps
inline constexpr char kCustomLinksForPreinstalledAppsRemoved[] =
    "custom_links.preinstalledremoved";

// Pref that stores the index of the virtual AI Mode link in custom links.
// Defaults to 0; set to -1 if deleted/unpinned by the user.
inline constexpr char kCustomLinksAiModeTileIndex[] =
    "custom_links.ai_mode_tile_index";

// The pref that stores if the Tab Resumption Home Module is enabled.
inline constexpr char kTabResumptionHomeModuleEnabled[] =
    "home.module.tab_resumption.enabled";

// The pref that stores if the Tips Home Module is enabled.
inline constexpr char kTipsHomeModuleEnabled[] = "home.module.tips.enabled";

// Stores if the Magic Stack Home Module (i.e. cards on the NTP) is enabled.
inline constexpr char kMagicStackHomeModuleEnabled[] =
    "home.module.magic_stack.enabled";

// The pref that stores if the Most Visited Tiles Home Module is enabled.
inline constexpr char kMostVisitedHomeModuleEnabled[] =
    "home.module.most_visited.enabled";

}  // namespace ntp_tiles::prefs

#endif  // COMPONENTS_NTP_TILES_PREF_NAMES_H_
