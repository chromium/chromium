// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_CONCEPT_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_CONCEPT_H_

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_identifier.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search_result_icon.mojom.h"

namespace ash::settings {

// Represents a potential search result. In this context, "concept" refers to
// the fact that this search result represents an idea which may be described
// by more than just one phrase. For example, a concept of "Display settings"
// may also be described as "Monitor settings".
//
// Each concept has a canonical description search tag as well as up to
// |kMaxAltTagsPerConcept| alternate descriptions search tags.
struct SearchConcept {
  static constexpr size_t kMaxAltTagsPerConcept = 5;
  static constexpr int kAltTagEnd = 0;

  // Message ID (from os_settings_search_tag_strings.grdp) corresponding to the
  // canonical search tag for this concept.
  int canonical_message_id;

  // URL path corresponding to the settings subpage at which the user can
  // change a setting associated with the tag. This string can also contain
  // URL parameters.
  //
  // Example 1 - Display settings (chrome://os-settings/device/display):
  //             ==> "device/display".
  // Example 2 - Wi-Fi settings (chrome://os-settings/networks?type=WiFi):
  //             ==> "networks?type=WiFi"
  const char* url_path_with_parameters;

  // Icon to display for search results associated with this concept.
  mojom::SearchResultIcon icon;

  // Default ranking, which is used to break ties when searching for results.
  mojom::SearchResultDefaultRank default_rank;

  // The type and identifier for this search result. The value of the |type|
  // field indicates the union member used by |id|.
  mojom::SearchResultType type;
  OsSettingsIdentifier id;

  // Alternate message IDs (from os_settings_search_tag_strings.grdp)
  // corresponding to this concept. These IDs refer to messages which represent
  // an alternate way of describing the same concept (e.g., "Monitor settings"
  // is an alternate phrase for "Display settings").
  //
  // This field provides up to |kMaxAltTagsPerConcept| alternate tags, but not
  // all concepts will require this many. A value of kAltTagEnd is used to
  // indicate that there are no further tags.
  //
  // Example 1 - Five alternate tags: [1234, 1235, 1236, 1237, 1238]
  // Example 2 - Two alternate tags: [1234, 1235, kAltTagEnd, _, _]
  // Example 3 - Zero alternate tags: [kAltTagEnd, _, _, _, _]
  int alt_tag_ids[kMaxAltTagsPerConcept] = {kAltTagEnd};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_CONCEPT_H_
