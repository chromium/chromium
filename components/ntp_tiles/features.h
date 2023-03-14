// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

#include "base/feature_list.h"

namespace ntp_tiles {

// Experiment groups for the new tab page retention experiments.
enum class NewTabPageRetentionExperimentBehavior {
  // Default popular sites.
  kDefault = 0,
  // Sites with popular, native iOS apps are included in the default popular
  // sites suggestions.
  kPopularSitesIncludePopularApps = 1,
  // Sites with popular, native iOS apps are excluded from the default popular
  // sites suggestions.
  kPopularSitesExcludePopularApps = 2,
  // Control population for popular apps experiment.
  kPopularSitesControl = 3,
  // Hides all NTP tiles for new users.
  kTileAblationHideAll = 4,
  // Hides most visited tiles for new users.
  kTileAblationHideMVTOnly = 5,
  // Control group for tile ablation.
  kTileAblationControl = 6,
};

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
BASE_DECLARE_FEATURE(kPopularSitesBakedInContentFeature);

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
BASE_DECLARE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature);

// If this feature is enabled, we enable popular sites in the suggestions UI.
BASE_DECLARE_FEATURE(kUsePopularSitesSuggestions);

// Feature flag to enable new tab page retention experiment on IOS.
// Use `GetDefaultNTPRetentionExperimentType()` instead of this
// constant directly.
BASE_DECLARE_FEATURE(kNewTabPageRetention);

// Feature name for the NTP retention field trial.
extern const char kNewTabPageRetentionName[];

// Feature parameters for the new tab page retention experiment.
extern const char kNewTabPageRetentionParam[];

// Returns the currently enabled NTP retention experiment type. If none are
// enabled, returns the default value.
NewTabPageRetentionExperimentBehavior GetNewTabPageRetentionExperimentType();

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
