// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/pref_names.h"

namespace ntp_tiles {
namespace prefs {

// The number of personal tiles we had previously. Used to figure out
// whether we need popular sites.
const char kNumPersonalTiles[] = "ntp.num_personal_suggestions";

// If set, overrides the URL for popular sites, including the individual
// overrides for country and version below.
const char kPopularSitesOverrideURL[] = "popular_sites.override_url";

// If set, this will override the URL path directory for popular sites.
const char kPopularSitesOverrideDirectory[] =
    "popular_sites.override_directory";

// If set, this will override the country detection for popular sites.
const char kPopularSitesOverrideCountry[] = "popular_sites.override_country";

// If set, this will override the default file version for popular sites.
const char kPopularSitesOverrideVersion[] = "popular_sites.override_version";

// Prefs used to cache suggested sites and store caching meta data.
const char kPopularSitesLastDownloadPref[] = "popular_sites_last_download";
const char kPopularSitesURLPref[] = "popular_sites_url";
const char kPopularSitesJsonPref[] = "suggested_sites_json";
const char kPopularSitesVersionPref[] = "suggested_sites_version";

// Prefs used to cache custom links.
const char kCustomLinksList[] = "custom_links.list";
const char kCustomLinksInitialized[] = "custom_links.initialized";

}  // namespace prefs
}  // namespace ntp_tiles
