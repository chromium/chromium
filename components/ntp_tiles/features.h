// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace ntp_tiles {

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
extern const base::Feature kPopularSitesBakedInContentFeature;

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
extern const base::Feature kNtpMostLikelyFaviconsFromServerFeature;

// If this feature is enabled, we enable popular sites in the suggestions UI.
extern const base::Feature kUsePopularSitesSuggestions;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
