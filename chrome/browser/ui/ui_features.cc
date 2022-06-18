// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui_features.h"

namespace features {

// Enables the tab dragging fallback when full window dragging is not supported
// by the platform (e.g. Wayland). See https://crbug.com/896640
const base::Feature kAllowWindowDragUsingSystemDragDrop{
    "AllowWindowDragUsingSystemDragDrop", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Chrome Labs menu in the toolbar. See https://crbug.com/1145666
const base::Feature kChromeLabs{"ChromeLabs",
                                base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Enables "Tips for Chrome" in Main Chrome Menu | Help.
const base::Feature kChromeTipsInMainMenu{"ChromeTipsInMainMenu",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables "Tips for Chrome" in Main Chrome Menu | Help.
const base::Feature kChromeTipsInMainMenuNewBadge{
    "ChromeTipsInMainMenuNewBadge", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables "Chrome What's New" UI.
const base::Feature kChromeWhatsNewUI {
  "ChromeWhatsNewUI",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(ANDROID) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS) && !BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Enables "new" badge for "Chrome What's New" in Main Chrome Menu | Help.
const base::Feature kChromeWhatsNewInMainMenuNewBadge{
    "ChromeWhatsNewInMainMenuNewBadge", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(ANDROID)
// Enables "Access Code Cast" UI.
const base::Feature kAccessCodeCastUI{"AccessCodeCastUI",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables displaying the submenu to open a link with a different profile
// even if there is no other profile opened in a separate window
const base::Feature kDisplayOpenLinkAsProfile{
    "DisplayOpenLinkAsProfile", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing the EV certificate details in the Page Info bubble.
const base::Feature kEvDetailsInPageInfo{"EvDetailsInPageInfo",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the reauth flow for authenticated profiles with invalid credentials
// when the force sign-in policy is enabled.
const base::Feature kForceSignInReauth{"ForceSignInReauth",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a more prominent active tab title in dark mode to aid with
// accessibility.
const base::Feature kProminentDarkModeActiveTabTitle{
    "ProminentDarkModeActiveTabTitle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the QuickCommands UI surface. See https://crbug.com/1014639
const base::Feature kQuickCommands{"QuickCommands",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the side search feature for Google Search. Presents recent Google
// search results in a browser side panel.
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable by default as the ChromeOS iteration of Side Search has launched (See
// crbug.com/1242730).
const base::Feature kSideSearch{"SideSearch", base::FEATURE_ENABLED_BY_DEFAULT};
#else
// Disable by default on remaining desktop platforms until desktop UX has
// launched (See crbug.com/1279696).
const base::Feature kSideSearch{"SideSearch",
                                base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const base::Feature kSideSearchFeedback{"SideSearchFeedback",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Side Search feature is configured to support any
// participating Chrome search engine. This should always be enabled with
// kSideSearch on non-ChromeOS platforms.
const base::Feature kSideSearchDSESupport{"SideSearchDSESupport",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the side search icon animates-in its label when the side
// panel is made available for the active tab.
const base::Feature kSideSearchPageActionLabelAnimation{
    "SideSearchPageActionLabelAnimation", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the frequency that the Side Search page action's label is shown. If
// enabled the label text is shown one per window.
const base::FeatureParam<kSideSearchLabelAnimationTypeOption>::Option
    kSideSearchPageActionLabelAnimationTypeParamOptions[] = {
        {kSideSearchLabelAnimationTypeOption::kProfile, "Profile"},
        {kSideSearchLabelAnimationTypeOption::kWindow, "Window"},
        {kSideSearchLabelAnimationTypeOption::kTab, "Tab"}};

const base::FeatureParam<kSideSearchLabelAnimationTypeOption>
    kSideSearchPageActionLabelAnimationType{
        &kSideSearchPageActionLabelAnimation,
        "SideSearchPageActionLabelAnimationType",
        kSideSearchLabelAnimationTypeOption::kWindow,
        &kSideSearchPageActionLabelAnimationTypeParamOptions};

const base::FeatureParam<int> kSideSearchPageActionLabelAnimationMaxCount{
    &kSideSearchPageActionLabelAnimation,
    "SideSearchPageActionLabelAnimationMaxCount", 1};

// Whether to clobber all side search side panels in the current browser window
// or only the side search in the current tab before read later or lens side
// panel is open.
const base::Feature kClobberAllSideSearchSidePanels{
    "ClobberAllSideSearchSidePanels", base::FEATURE_ENABLED_BY_DEFAULT};

// Adds improved support for handling multiple contextual and global RHS browser
// side panels. Designed specifically to handle the interim state before the v2
// side panel project launches.
const base::Feature kSidePanelImprovedClobbering{
    "SidePanelImprovedClobbering", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSidePanelJourneys{"SidePanelJourneys",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs to scroll in the tabstrip. https://crbug.com/951078
const base::Feature kScrollableTabStrip{"ScrollableTabStrip",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const char kMinimumTabWidthFeatureParameterName[] = "minTabWidth";

// Directly controls the "new" badge (as opposed to old "master switch"; see
// https://crbug.com/1169907 for master switch deprecation and
// https://crbug.com/968587 for the feature itself)
// https://crbug.com/1173792
const base::Feature kTabGroupsNewBadgePromo{"TabGroupsNewBadgePromo",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables users to explicitly save and recall tab groups.
// https://crbug.com/1223929
const base::Feature kTabGroupsSave{"TabGroupsSave",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables preview images in tab-hover cards.
// https://crbug.com/928954
const base::Feature kTabHoverCardImages{"TabHoverCardImages",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const char kTabHoverCardImagesNotReadyDelayParameterName[] =
    "page_not_ready_delay";
const char kTabHoverCardImagesLoadingDelayParameterName[] =
    "page_loading_delay";
const char kTabHoverCardImagesLoadedDelayParameterName[] = "page_loaded_delay";
const char kTabHoverCardImagesCrossfadePreviewAtParameterName[] =
    "crossfade_preview_at";
const char kTabHoverCardAdditionalMaxWidthDelay[] =
    "additional_max_width_delay";
const char kTabHoverCardAlternateFormat[] = "alternate_format";

// Enables tab outlines in additional situations for accessibility.
const base::Feature kTabOutlinesInLowContrastThemes{
    "TabOutlinesInLowContrastThemes", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabSearchChevronIcon{"TabSearchChevronIcon",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the tab search submit feedback button.
const base::Feature kTabSearchFeedback{"TabSearchFeedback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not to use fuzzy search for tab search.
const base::Feature kTabSearchFuzzySearch{"TabSearchFuzzySearch",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const char kTabSearchSearchThresholdName[] = "TabSearchSearchThreshold";

const base::FeatureParam<bool> kTabSearchSearchIgnoreLocation{
    &kTabSearchFuzzySearch, "TabSearchSearchIgnoreLocation", false};

const base::Feature kTabSearchMediaTabs{"TabSearchMediaTabs",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// If this feature parameter is enabled, show media tabs in both "Audio & Video"
// section and "Open Tabs" section.
const char kTabSearchAlsoShowMediaTabsinOpenTabsSectionParameterName[] =
    "Also show Media Tabs in Open Tabs Section";

const base::FeatureParam<int> kTabSearchSearchDistance{
    &kTabSearchFuzzySearch, "TabSearchSearchDistance", 200};

const base::FeatureParam<double> kTabSearchSearchThreshold{
    &kTabSearchFuzzySearch, kTabSearchSearchThresholdName, 0.6};

const base::FeatureParam<double> kTabSearchTitleWeight{
    &kTabSearchFuzzySearch, "TabSearchTitleWeight", 2.0};

const base::FeatureParam<double> kTabSearchHostnameWeight{
    &kTabSearchFuzzySearch, "TabSearchHostnameWeight", 1.0};

const base::FeatureParam<double> kTabSearchGroupTitleWeight{
    &kTabSearchFuzzySearch, "TabSearchGroupTitleWeight", 1.5};

const base::FeatureParam<bool> kTabSearchMoveActiveTabToBottom{
    &kTabSearchFuzzySearch, "TabSearchMoveActiveTabToBottom", true};

// Controls feature parameters for Tab Search's `Recently Closed` entries.
const base::Feature kTabSearchRecentlyClosed{"TabSearchRecentlyClosed",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<int> kTabSearchRecentlyClosedDefaultItemDisplayCount{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedDefaultItemDisplayCount",
    8};

const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedTabCountThreshold", 100};

const base::Feature kTabSearchUseMetricsReporter{
    "TabSearchUseMetricsReporter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kToolbarUseHardwareBitmapDraw{
    "ToolbarUseHardwareBitmapDraw", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUnifiedSidePanel{"UnifiedSidePanel",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// This enables enables persistence of a WebContents in a 1-to-1 association
// with the current Profile for WebUI bubbles. See https://crbug.com/1177048.
const base::Feature kWebUIBubblePerProfilePersistence{
    "WebUIBubblePerProfilePersistence", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
const base::Feature kWebUITabStrip {
  "WebUITabStrip",
#if BUILDFLAG(IS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

// The default value of this flag is aligned with platform behavior to handle
// context menu with touch.
// TODO(crbug.com/1257626): Enable this flag for all platforms after launch.
const base::Feature kWebUITabStripContextMenuAfterTap {
  "WebUITabStripContextMenuAfterTap",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_CHROMEOS)
const base::Feature kChromeOSTabSearchCaptionButton{
    "ChromeOSTabSearchCaptionButton", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_MAC)
// Enabled an experiment which increases the prominence to grant MacOS system
// location permission to Chrome when location permissions have already been
// approved. https://crbug.com/1211052
const base::Feature kLocationPermissionsExperiment{
    "LocationPermissionsExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<int>
    kLocationPermissionsExperimentBubblePromptLimit{
        &kLocationPermissionsExperiment, "bubble_prompt_count", 3};
constexpr base::FeatureParam<int>
    kLocationPermissionsExperimentLabelPromptLimit{
        &kLocationPermissionsExperiment, "label_prompt_count", 5};

const base::Feature kViewsFirstRunDialog{"ViewsFirstRunDialog",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kViewsTaskManager{"ViewsTaskManager",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kViewsJSAppModalDialog{"ViewsJSAppModalDialog",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

int GetLocationPermissionsExperimentBubblePromptLimit() {
  return kLocationPermissionsExperimentBubblePromptLimit.Get();
}
int GetLocationPermissionsExperimentLabelPromptLimit() {
  return kLocationPermissionsExperimentLabelPromptLimit.Get();
}
#endif

#if BUILDFLAG(IS_WIN)

// Moves the Tab Search button into the browser frame's caption button area on
// Windows 10 (crbug.com/1223847).
const base::Feature kWin10TabSearchCaptionButton{
    "Win10TabSearchCaptionButton", base::FEATURE_ENABLED_BY_DEFAULT};

#endif

}  // namespace features
