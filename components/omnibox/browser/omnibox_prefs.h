// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_

#include <string>

#include "components/omnibox/browser/omnibox_pref_names.h"

class PrefRegistrySimple;
class PrefService;

namespace omnibox {

// These values are persisted to prefs. They cannot be freely changed.
enum SuggestionGroupVisibility {
  // When DEFAULT is returned, the group's visibility should be controlled by
  // the server-provided hint.
  DEFAULT = 0,

  // HIDDEN means the user has manually hidden the group before, and so this
  // group should be hidden regardless of the server-provided hint.
  HIDDEN = 1,

  // SHOWN means the user has manually shown the group before, and so this
  // group should be shown regardless of the server-provided hint.
  SHOWN = 2,
};

// Histograms being recorded when visibility of suggestion group IDs change.
inline constexpr char kGroupIdToggledOffHistogram[] =
    "Omnibox.GroupId.ToggledOff";
inline constexpr char kGroupIdToggledOnHistogram[] =
    "Omnibox.GroupId.ToggledOn";

// Many of the prefs defined above are registered locally where they're used.
// New prefs should be added here and ordered the same as they're defined above.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Registers the omnibox prefs that are stored in Local State. These prefs are
// not tied to a specific profile.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Updates the ZPS dictionary preference to cache the given |response| value
// using the |page_url| as the cache key.
void SetUserPreferenceForZeroSuggestCachedResponse(PrefService* prefs,
                                                   const std::string& page_url,
                                                   const std::string& response);

// Returns the cached response from the ZPS dictionary preference associated
// with the given |page_url|.
std::string GetUserPreferenceForZeroSuggestCachedResponse(
    PrefService* prefs,
    const std::string& page_url);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
