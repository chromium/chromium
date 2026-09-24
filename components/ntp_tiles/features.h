// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

#include "base/feature_list.h"

namespace ntp_tiles {

// Defines the arms for the AIM Refactor Experiment.
enum class AimButtonRefactorArm {
  kDisabled = 0,
  // Focuses the composebox with AI Mode enabled when the AIM Quick Action
  // button is tapped.
  kFocusComposeboxAimQuickAction = 1,
  // Present AIM button in the Quick Actions row alongside an image generation
  // mode chip.
  kImageGenerationQuickAction = 2,
  // Present AIM button in the Quick Actions row alongside an attach images
  // chip.
  kAttachImageQuickAction = 3,
  // Present the AIM button as a standalone module beside the Most Visited
  // Tiles. Remove the Quick Actions row from the NTP.
  kAimAsModule = 4,
  // Present the AIM button as a Most Visited Tile. Remove the Quick Actions row
  // from the NTP.
  kAimAsMvt = 5,
  // Remove the AIM button and the Quick Actions row from the NTP.
  kNoChips = 6,
};

// Parameter to indicate which arm of the feature kAimButtonRefactor is enabled.
inline constexpr char kAimButtonRefactorArmParam[] = "aim-button-refactor-arm";

// Enables the AimButtonRefactor feature.
BASE_DECLARE_FEATURE(kAimButtonRefactor);

// Returns the active arm for the AimButtonRefactor feature.
AimButtonRefactorArm GetAimButtonRefactorArm();

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
BASE_DECLARE_FEATURE(kPopularSitesBakedInContentFeature);

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
BASE_DECLARE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature);

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
