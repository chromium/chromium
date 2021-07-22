// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"

namespace features {

// Enables Chrome Labs menu in the toolbar. See https://crbug.com/1145666
const base::Feature kChromeLabs{"ChromeLabs",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Commander UI surface. See https://crbug.com/1014639
const base::Feature kCommander{"Commander", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Enables "Tips for Chrome" in Main Chrome Menu | Help.
const base::Feature kChromeTipsInMainMenu{"ChromeTipsInMainMenu",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables "Tips for Chrome" in Main Chrome Menu | Help.
const base::Feature kChromeTipsInMainMenuNewBadge{
    "ChromeTipsInMainMenuNewBadge", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables "Chrome What's New" UI.
const base::Feature kChromeWhatsNewUI{"ChromeWhatsNewUI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Enables "new" badge for "Chrome What's New" in Main Chrome Menu | Help.
const base::Feature kChromeWhatsNewInMainMenuNewBadge{
    "ChromeWhatsNewInMainMenuNewBadge", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Enables "Enterprise Casting" UI.
const base::Feature kEnterpriseCastingUI{"EnterpriseCastingUI",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables showing the EV certificate details in the Page Info bubble.
const base::Feature kEvDetailsInPageInfo{"EvDetailsInPageInfo",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the hosting of an extension in the left aligned side panel of the
// browser window. Currently used for a hosted extension experiment.
const base::Feature kExtensionsSidePanel{"ExtensionsSidePanel",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kExtensionsSidePanelId{
    &kExtensionsSidePanel, "ExtensionsSidePanelId", ""};

// Enables the reauth flow for authenticated profiles with invalid credentials
// when the force sign-in policy is enabled.
const base::Feature kForceSignInReauth{"ForceSignInReauth",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables updated tabstrip animations, required for a scrollable tabstrip.
// https://crbug.com/958173
const base::Feature kNewTabstripAnimation{"NewTabstripAnimation",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a more prominent active tab title in dark mode to aid with
// accessibility.
const base::Feature kProminentDarkModeActiveTabTitle{
    "ProminentDarkModeActiveTabTitle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a 'new' badge on the option to add to the reading list in the tab
// context menu.
const base::Feature kReadLaterNewBadgePromo{"ReadLaterNewBadgePromo",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReadLaterAddFromDialog{"ReadLaterAddFromDialog",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs to scroll in the tabstrip. https://crbug.com/951078
const base::Feature kScrollableTabStrip{"ScrollableTabStrip",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const char kMinimumTabWidthFeatureParameterName[] = "minTabWidth";

// Enables buttons to permanently appear on the tabstrip when
// scrollable-tabstrip is enabled. https://crbug.com/1116118
const base::Feature kScrollableTabStripButtons{
    "ScrollableTabStripButtons", base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(ANDROID)
// Changes the layout of the chrome://settings page to only show one section at
// a time, crbug.com/1204457.
const base::Feature kSettingsLandingPageRedesign{
    "SettingsLandingPageRedesign", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Updated managed profile sign-in popup. https://crbug.com/1141224
const base::Feature kSyncConfirmationUpdatedText{
    "SyncConfirmationUpdatedText", base::FEATURE_DISABLED_BY_DEFAULT};

// Sign-in functionality in the profile creation flow. https://crbug.com/1126913
const base::Feature kSignInProfileCreation{"SignInProfileCreation",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Smoother enterprise experience in the sign-in profile creation flow.
// https://crbug.com/1178494
const base::Feature kSignInProfileCreationEnterprise{
    "SignInProfileCreationEnterprise", base::FEATURE_ENABLED_BY_DEFAULT};

// Automatically create groups for users based on domain.
// https://crbug.com/1128703
const base::Feature kTabGroupsAutoCreate{"TabGroupsAutoCreate",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs to be frozen when collapsed. https://crbug.com/1110108
const base::Feature kTabGroupsCollapseFreezing{
    "TabGroupsCollapseFreezing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the feedback through the tab group editor bubble.
// https://crbug.com/1067062
const base::Feature kTabGroupsFeedback{"TabGroupsFeedback",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables tab outlines in additional situations for accessibility.
const base::Feature kTabOutlinesInLowContrastThemes{
    "TabOutlinesInLowContrastThemes", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables submenus under each tab group or window within the app menu history.
const base::Feature kTabRestoreSubMenus{"TabRestoreSubMenus",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabSearchChevronIcon{"TabSearchChevronIcon",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not feature parameters should be used for Tab Search's
// fuzzy search or if default values should be used.
const base::Feature kTabSearchFuzzySearch{"kTabSearchFuzzySearch",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the tab search submit feedback button.
const base::Feature kTabSearchFeedback{"TabSearchFeedback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls feature parameters for Tab Search's `Recently Closed` entries.
const base::Feature kTabSearchRecentlyClosed{"TabSearchRecentlyClosed",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<bool> kTabSearchSearchIgnoreLocation{
    &kTabSearchFuzzySearch, "TabSearchSearchIgnoreLocation", true};

const base::FeatureParam<int> kTabSearchSearchDistance{
    &kTabSearchFuzzySearch, "TabSearchSearchDistance", 200};

const base::FeatureParam<double> kTabSearchSearchThreshold{
    &kTabSearchFuzzySearch, "TabSearchSearchThreshold", 0.0};

const base::FeatureParam<double> kTabSearchTitleWeight{
    &kTabSearchFuzzySearch, "TabSearchTitleWeight", 2.0};

const base::FeatureParam<double> kTabSearchHostnameWeight{
    &kTabSearchFuzzySearch, "TabSearchHostnameWeight", 1.0};

const base::FeatureParam<double> kTabSearchGroupTitleWeight{
    &kTabSearchFuzzySearch, "TabSearchGroupTitleWeight", 1.5};

const base::FeatureParam<bool> kTabSearchMoveActiveTabToBottom{
    &kTabSearchFuzzySearch, "TabSearchMoveActiveTabToBottom", true};

const base::FeatureParam<int> kTabSearchRecentlyClosedDefaultItemDisplayCount{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedDefaultItemDisplayCount",
    8};

const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedTabCountThreshold", 100};

const base::Feature kToolbarUseHardwareBitmapDraw{
    "ToolbarUseHardwareBitmapDraw", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseRelaunchToUpdateString{
    "UseRelaunchToUpdateString", base::FEATURE_DISABLED_BY_DEFAULT};

// This enables enables persistence of a WebContents in a 1-to-1 association
// with the current Profile for WebUI bubbles. See https://crbug.com/1177048.
const base::Feature kWebUIBubblePerProfilePersistence{
    "WebUIBubblePerProfilePersistence", base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(ANDROID)
const base::Feature kWebUIBrandingUpdate{"WebUIBrandingUpdate",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the WebUI Download Shelf instead of the Views framework Download
// Shelf. See https://crbug.com/1180372.
const base::Feature kWebUIDownloadShelf{"WebUIDownloadShelf",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
const base::Feature kWebUITabStrip{"WebUITabStrip",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the new tab button is in the tab strip or the toolbar when WebUI tab
// strip is enabled.
const base::Feature kWebUITabStripNewTabButtonInTabStrip{
    "WebUITabStripNewTabButtonInTabStrip", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a WebUI Feedback UI, as opposed to the Chrome App UI. See
// https://crbug.com/1167223.
const base::Feature kWebUIFeedback{"WebUIFeedback",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
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

#if defined(OS_WIN)

// Moves the Tab Search button into the browser frame's caption button area on
// Windows 10 (crbug.com/1223847).
const base::Feature kWin10TabSearchCaptionButton{
    "Win10TabSearchCaptionButton", base::FEATURE_DISABLED_BY_DEFAULT};

#endif

}  // namespace features
