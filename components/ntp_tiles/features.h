// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

#include "base/feature_list.h"

namespace ntp_tiles {

// (Chrome IOS only) Experiment behaviors for the improved default popular sites
// experiment.
enum class IOSDefaultPopularSitesExperimentBehavior {
  // Sites with popular, native iOS apps are included in the default popular
  // sites suggestions.
  kIncludePopularApps = 0,
  // Sites with popular, native iOS apps are excluded from the default popular
  // sites suggestions.
  kExcludePopularApps = 1,
  // Default popular sites.
  kDefault = 2,
};

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// Feature param under `kIOSPopularSitesImprovedSuggestions` to enable
// excluding sites from popular sites (on IOS only) that have popular, native
// iOS apps.
extern const char kIOSPopularSitesExcludePopularAppsParam[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
BASE_DECLARE_FEATURE(kPopularSitesBakedInContentFeature);

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
BASE_DECLARE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature);

// If this feature is enabled, we enable popular sites in the suggestions UI.
BASE_DECLARE_FEATURE(kUsePopularSitesSuggestions);

// Feature flag to enable improved default popular sites suggestions on IOS.
// Use `GetDefaultPopularSitesExperimentType()` instead of this
// constant directly.
BASE_DECLARE_FEATURE(kIOSPopularSitesImprovedSuggestions);

// (Chrome IOS only) Returns the experiment type for the improved default
// popular sites suggestions.
IOSDefaultPopularSitesExperimentBehavior GetDefaultPopularSitesExperimentType();

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
