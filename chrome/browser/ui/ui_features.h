// Copyright 2019 The Chromium Authors
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
#include "extensions/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// TODO(https://crbug.com/896640): Remove this when the tab dragging
// interactive_ui_tests pass on Wayland.
BASE_DECLARE_FEATURE(kAllowWindowDragUsingSystemDragDrop);

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

BASE_DECLARE_FEATURE(kChromeLabs);
extern const char kChromeLabsActivationParameterName[];
extern const base::FeatureParam<int> kChromeLabsActivationPercentage;

BASE_DECLARE_FEATURE(kChromeWhatsNewUI);

BASE_DECLARE_FEATURE(kExtensionsMenuInAppMenu);
bool IsExtensionMenuInRootAppMenu();

#if !defined(ANDROID)
BASE_DECLARE_FEATURE(kAccessCodeCastUI);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kCameraMicPreview);
#endif

BASE_DECLARE_FEATURE(kDisplayOpenLinkAsProfile);

BASE_DECLARE_FEATURE(kEvDetailsInPageInfo);

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_DECLARE_FEATURE(kGetTheMostOutOfChrome);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kHaTSWebUI);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_DECLARE_FEATURE(kLightweightExtensionOverrideConfirmations);
#endif

BASE_DECLARE_FEATURE(kPowerBookmarksSidePanel);

BASE_DECLARE_FEATURE(kQuickCommands);

BASE_DECLARE_FEATURE(kResponsiveToolbar);

BASE_DECLARE_FEATURE(kScrollableTabStrip);
extern const char kMinimumTabWidthFeatureParameterName[];

BASE_DECLARE_FEATURE(kScrollableTabStripWithDragging);
extern const char kTabScrollingWithDraggingModeName[];

BASE_DECLARE_FEATURE(kSplitTabStrip);

BASE_DECLARE_FEATURE(kTabScrollingButtonPosition);
extern const char kTabScrollingButtonPositionParameterName[];

BASE_DECLARE_FEATURE(kScrollableTabStripOverflow);
extern const char kScrollableTabStripOverflowModeName[];

BASE_DECLARE_FEATURE(kSidePanelWebView);

#if !defined(ANDROID)
BASE_DECLARE_FEATURE(kSidePanelCompanionDefaultPinned);

BASE_DECLARE_FEATURE(kSidePanelPinning);
#endif

BASE_DECLARE_FEATURE(kSidePanelJourneysQueryless);
BASE_DECLARE_FEATURE(kSidePanelSearchCompanion);

BASE_DECLARE_FEATURE(kSideSearch);
BASE_DECLARE_FEATURE(kSideSearchFeedback);
BASE_DECLARE_FEATURE(kSearchWebInSidePanel);

BASE_DECLARE_FEATURE(kSideSearchAutoTriggering);
extern const base::FeatureParam<int> kSideSearchAutoTriggeringReturnCount;

BASE_DECLARE_FEATURE(kTabGroupsCollapseFreezing);

BASE_DECLARE_FEATURE(kTabGroupsSave);

BASE_DECLARE_FEATURE(kTabHoverCardImageSettings);

BASE_DECLARE_FEATURE(kTabHoverCardImages);

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

BASE_DECLARE_FEATURE(kTabOrganization);
bool IsTabOrganization();

BASE_DECLARE_FEATURE(kTabSearchChevronIcon);

BASE_DECLARE_FEATURE(kTabSearchFeedback);

BASE_DECLARE_FEATURE(kTabSearchFuzzySearch);

extern const char kTabSearchSearchThresholdName[];

// Setting this to true will ignore the distance parameter when finding matches.
// This means that it will not matter where in the string the pattern occurs.
extern const base::FeatureParam<bool> kTabSearchSearchIgnoreLocation;

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

BASE_DECLARE_FEATURE(kTabSearchRecentlyClosed);

// Default number of recently closed entries to display by default when no
// search text is provided.
extern const base::FeatureParam<int>
    kTabSearchRecentlyClosedDefaultItemDisplayCount;

// A threshold of recently closed tabs after which to stop adding recently
// closed item data to the profile data payload should the minimum display
// count have been met.
extern const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold;

BASE_DECLARE_FEATURE(kTabSearchUseMetricsReporter);

// Determines how screenshots of the toolbar uses Software or Hardware drawing.
// Works on Android 10+.
BASE_DECLARE_FEATURE(kToolbarUseHardwareBitmapDraw);

BASE_DECLARE_FEATURE(kTopChromeWebUIUsesSpareRenderer);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kUpdateTextOptions);
extern const base::FeatureParam<int> kUpdateTextOptionNumber;
#endif

BASE_DECLARE_FEATURE(kWebUIBubblePerProfilePersistence);

BASE_DECLARE_FEATURE(kWebUITabStrip);

// Controls whether the context menu is shown on a touch press or a touch
// tap gesture on the WebUI Tab Strip.
BASE_DECLARE_FEATURE(kWebUITabStripContextMenuAfterTap);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kChromeOSTabSearchCaptionButton);
#endif

// Cocoa to views migration.
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kLocationPermissionsExperiment);

BASE_DECLARE_FEATURE(kViewsFirstRunDialog);
BASE_DECLARE_FEATURE(kViewsTaskManager);
BASE_DECLARE_FEATURE(kViewsJSAppModalDialog);

int GetLocationPermissionsExperimentBubblePromptLimit();
int GetLocationPermissionsExperimentLabelPromptLimit();
#endif

BASE_DECLARE_FEATURE(kStopLoadingAnimationForHiddenWindow);

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
