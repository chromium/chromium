// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kEvDetailsInPageInfo;

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const base::Feature kExtensionSettingsOverriddenDialogs;
#endif

extern const base::Feature kExtensionsToolbarMenu;

extern const base::Feature kForceEnableDevicesPage;

extern const base::Feature kNewProfilePicker;

extern const base::Feature kNewTabstripAnimation;

extern const base::Feature kPermissionChip;

extern const base::Feature kProminentDarkModeActiveTabTitle;

extern const base::Feature kReadLater;

extern const base::Feature kScrollableTabStrip;

extern const base::Feature kProfilesUIRevamp;

extern const base::Feature kTabGroups;

extern const base::Feature kTabGroupsAutoCreate;

extern const base::Feature kTabGroupsCollapse;

extern const base::Feature kTabGroupsCollapseFreezing;

extern const base::Feature kTabGroupsFeedback;

extern const base::Feature kTabHoverCards;
extern const char kTabHoverCardsFeatureParameterName[];

extern const base::Feature kTabHoverCardImages;

extern const base::Feature kTabOutlinesInLowContrastThemes;

extern const base::Feature kTabSearch;

extern const base::Feature kTabSearchFeedback;

extern const base::Feature kTabSearchFixedEntrypoint;

// Setting this to true will ignore the distance parameter when finding matches.
// This means that it will not matter where in the string the pattern occurs.
extern const base::FeatureParam<bool> kTabSearchSearchIgnoreLocation;

// Determines how close the match must be to the beginning of the string. Eg a
// distance of 100 and threshold of 0.8 would require a perfect match to be
// within 80 characters of the beginning of the string.
extern const base::FeatureParam<int> kTabSearchSearchDistance;

// This determines how strong the match should be for the item to be included in
// the result set. Eg a threshold of 0.0 requires a perfect match, 1.0 would
// match anything. Permissible values are [0.0, 1.0].
extern const base::FeatureParam<double> kTabSearchSearchThreshold;

// These are the hardcoded minimum and maximum search threshold values for
// |kTabSearchSearchThreshold|.
constexpr double kTabSearchSearchThresholdMin = 0.0;
constexpr double kTabSearchSearchThresholdMax = 1.0;

// Controls how heavily weighted the tab's title is relative to the hostname.
extern const base::FeatureParam<double> kTabSearchTitleToHostnameWeightRatio;

extern const base::Feature kUseTextForUpdateButton;

extern const base::Feature kWebFooterExperiment;

extern const base::Feature kWebUITabStrip;

#if defined(OS_CHROMEOS)
extern const base::Feature kHiddenNetworkWarning;
extern const base::Feature kSeparatePointingStickSettings;
#endif  // defined(OS_CHROMEOS)
}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
