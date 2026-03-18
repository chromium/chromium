// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ntp_tiles {

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
BASE_DECLARE_FEATURE(kPopularSitesBakedInContentFeature);

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
BASE_DECLARE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature);

// Feature to refresh popular sites for the US region.
BASE_DECLARE_FEATURE(kPopularSitesRefreshUs);

// Parameter determining the experimental arm. 0 means off, 1, 2, 3 are valid
// arms.
extern const base::FeatureParam<int> kPopularSitesRefreshUsArm;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
