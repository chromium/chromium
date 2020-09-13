// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
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

extern const base::Feature kSignInProfileCreationFlow;

extern const base::Feature kTabGroups;

extern const base::Feature kTabGroupsCollapse;

extern const base::Feature kTabGroupsCollapseFreezing;

extern const base::Feature kTabGroupsFeedback;

extern const base::Feature kTabHoverCards;
extern const char kTabHoverCardsFeatureParameterName[];

extern const base::Feature kTabHoverCardImages;

extern const base::Feature kTabOutlinesInLowContrastThemes;

extern const base::Feature kTabSearch;

extern const base::Feature kTabSearchFixedEntrypoint;

extern const base::Feature kUseTextForUpdateButton;

extern const base::Feature kWebFooterExperiment;

extern const base::Feature kWebUITabStrip;

extern const base::Feature kSyncSetupFriendlySettings;

#if defined(OS_CHROMEOS)
extern const base::Feature kHiddenNetworkWarning;
#endif  // defined(OS_CHROMEOS)
}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
