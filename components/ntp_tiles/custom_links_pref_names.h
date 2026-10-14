// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CUSTOM_LINKS_PREF_NAMES_H_
#define COMPONENTS_NTP_TILES_CUSTOM_LINKS_PREF_NAMES_H_

// These pref names are an implementation detail of //components/ntp_tiles and
// their visibility is restricted in GN. Code outside of //components/ntp_tiles
// should use the ntp_tiles::MostVisitedSites API instead of reading or
// observing these prefs directly.

namespace ntp_tiles::prefs {

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

// Pref used to verify whether custom links have been removed
// for preinstalled default chrome apps
inline constexpr char kCustomLinksForPreinstalledAppsRemoved[] =
    "custom_links.preinstalledremoved";

// Pref that stores the index of the virtual AI Mode link in custom links.
// Defaults to 0; set to -1 if deleted/unpinned by the user.
inline constexpr char kCustomLinksAiModeTileIndex[] =
    "custom_links.ai_mode_tile_index";

}  // namespace ntp_tiles::prefs

#endif  // COMPONENTS_NTP_TILES_CUSTOM_LINKS_PREF_NAMES_H_
