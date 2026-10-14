// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_PREF_NAMES_H_
#define COMPONENTS_NTP_TILES_PREF_NAMES_H_

namespace ntp_tiles::prefs {

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
