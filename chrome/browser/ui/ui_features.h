// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// TODO(https://crbug.com/896640): Remove this when the tab dragging
// interactive_ui_tests pass on Wayland.
extern const base::Feature kAllowWindowDragUsingSystemDragDrop;

extern const base::Feature kChromeLabs;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const base::Feature kChromeTipsInMainMenu;

extern const base::Feature kChromeTipsInMainMenuNewBadge;
#endif

extern const base::Feature kChromeWhatsNewUI;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const base::Feature kChromeWhatsNewInMainMenuNewBadge;
#endif

#if !defined(ANDROID)
extern const base::Feature kAccessCodeCastUI;
#endif

extern const base::Feature kDisplayOpenLinkAsProfile;

extern const base::Feature kEvDetailsInPageInfo;

extern const base::Feature kForceSignInReauth;

extern const base::Feature kProminentDarkModeActiveTabTitle;

extern const base::Feature kQuickCommands;

extern const base::Feature kScrollableTabStrip;
extern const char kMinimumTabWidthFeatureParameterName[];

// TODO(pbos): Once kReadLater is cleaned up on Desktop, move definition into
// ui_features.cc. This is currently temporarily in reading_list_switches.h.
extern const base::Feature kSidePanelImprovedClobbering;

extern const base::Feature kSidePanelJourneys;

extern const base::Feature kSideSearch;
extern const base::Feature kSideSearchFeedback;
extern const base::Feature kSideSearchDSESupport;
extern const base::Feature kClobberAllSideSearchSidePanels;

extern const base::Feature kSideSearchPageActionLabelAnimation;

enum class kSideSearchLabelAnimationTypeOption {
  kProfile,
  kWindow,
  kTab,
};
extern const base::FeatureParam<kSideSearchLabelAnimationTypeOption>
    kSideSearchPageActionLabelAnimationType;

extern const base::FeatureParam<int>
    kSideSearchPageActionLabelAnimationMaxCount;

extern const base::Feature kTabGroupsNewBadgePromo;

extern const base::Feature kTabGroupsSave;

extern const base::Feature kTabHoverCardImages;

// These parameters control how long the hover card system waits before
// requesting a preview image from a tab where no preview image is available.
// Values are in ms.
extern const char kTabHoverCardImagesNotReadyDelayParameterName[];
extern const char kTabHoverCardImagesLoadingDelayParameterName[];
extern const char kTabHoverCardImagesLoadedDelayParameterName[];

// Determines how long to wait during a hover card slide transition before a
// placeholder image is displayed via crossfade.
// -1: disable crossfade entirely
//  0: show placeholder immediately
//  1: show placeholder when the card lands on the new tab
//  between 0 and 1: show at a percentage of transition
//
// Note: crossfade is automatically disabled if animations are disabled at the
// OS level (e.g. for accessibility).
extern const char kTabHoverCardImagesCrossfadePreviewAtParameterName[];

// Adds an amount of time (in ms) to the show delay when tabs are max width -
// typically when there are less than 5 or 6 tabs in a browser window.
extern const char kTabHoverCardAdditionalMaxWidthDelay[];

// When set to 1, reverses the order of elements in the hover card, so that
// the title and site are on bottom and the tab status and preview image are
// on top. 0 is the default layout.
extern const char kTabHoverCardAlternateFormat[];

extern const base::Feature kTabOutlinesInLowContrastThemes;

extern const base::Feature kTabSearchChevronIcon;

extern const base::Feature kTabSearchFeedback;

extern const base::Feature kTabSearchFuzzySearch;

extern const char kTabSearchSearchThresholdName[];

// Setting this to true will ignore the distance parameter when finding matches.
// This means that it will not matter where in the string the pattern occurs.
extern const base::FeatureParam<bool> kTabSearchSearchIgnoreLocation;

extern const base::Feature kTabSearchMediaTabs;

extern const char kTabSearchAlsoShowMediaTabsinOpenTabsSectionParameterName[];

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

// Controls the weight associated with a tab's title for filtering and ordering
// list items.
extern const base::FeatureParam<double> kTabSearchTitleWeight;

// Controls the weight associated with a tab's hostname when filering and
// odering list items.
extern const base::FeatureParam<double> kTabSearchHostnameWeight;

// Controls the weight associated with a tab's group title filering and
// odering list items
extern const base::FeatureParam<double> kTabSearchGroupTitleWeight;

// Whether to move the active tab to the bottom of the list.
extern const base::FeatureParam<bool> kTabSearchMoveActiveTabToBottom;

extern const base::Feature kTabSearchRecentlyClosed;

// Default number of recently closed entries to display by default when no
// search text is provided.
extern const base::FeatureParam<int>
    kTabSearchRecentlyClosedDefaultItemDisplayCount;

// A threshold of recently closed tabs after which to stop adding recently
// closed item data to the profile data payload should the minimum display
// count have been met.
extern const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold;

extern const base::Feature kTabSearchUseMetricsReporter;

// Determines how screenshots of the toolbar uses Software or Hardware drawing.
// Works on Android 10+.
extern const base::Feature kToolbarUseHardwareBitmapDraw;

extern const base::Feature kUnifiedSidePanel;

extern const base::Feature kWebUIBubblePerProfilePersistence;

extern const base::Feature kWebUITabStrip;

// Controls whether the context menu is shown on a touch press or a touch
// tap gesture on the WebUI Tab Strip.
extern const base::Feature kWebUITabStripContextMenuAfterTap;

#if BUILDFLAG(IS_CHROMEOS)
extern const base::Feature kChromeOSTabSearchCaptionButton;
#endif

// Cocoa to views migration.
#if BUILDFLAG(IS_MAC)
extern const base::Feature kLocationPermissionsExperiment;

extern const base::Feature kViewsFirstRunDialog;
extern const base::Feature kViewsTaskManager;
extern const base::Feature kViewsJSAppModalDialog;

int GetLocationPermissionsExperimentBubblePromptLimit();
int GetLocationPermissionsExperimentLabelPromptLimit();
#endif

#if BUILDFLAG(IS_WIN)
extern const base::Feature kWin10TabSearchCaptionButton;
#endif

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
