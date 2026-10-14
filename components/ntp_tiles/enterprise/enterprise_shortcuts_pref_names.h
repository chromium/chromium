// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_PREF_NAMES_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_PREF_NAMES_H_

#include "build/build_config.h"

// These pref names are an implementation detail of //components/ntp_tiles and
// their visibility is restricted in GN. Code outside of //components/ntp_tiles
// should use the ntp_tiles::MostVisitedSites API instead of reading or
// observing these prefs directly.

namespace ntp_tiles::prefs {

// Prefs used to cache enterprise shortcuts.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
inline constexpr char kEnterpriseShortcutsPolicyList[] =
    "enterprise_shortcuts.policy_list";
inline constexpr char kEnterpriseShortcutsUserList[] =
    "enterprise_shortcuts.user_list";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace ntp_tiles::prefs

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_PREF_NAMES_H_
