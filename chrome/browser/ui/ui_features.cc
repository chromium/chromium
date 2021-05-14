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

// Enables showing the EV certificate details in the Page Info bubble.
const base::Feature kEvDetailsInPageInfo{"EvDetailsInPageInfo",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables an extension menu in the toolbar. See https://crbug.com/943702
const base::Feature kExtensionsToolbarMenu{"ExtensionsToolbarMenu",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the new profile picker.
// https:://crbug.com/1063856
const base::Feature kNewProfilePicker{"NewProfilePicker",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enables tabs to scroll in the tabstrip. https://crbug.com/951078
const base::Feature kScrollableTabStrip{"ScrollableTabStrip",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const char kMinimumTabWidthFeatureParameterName[] = "minTabWidth";

// Enables buttons to permanently appear on the tabstrip when
// scrollable-tabstrip is enabled. https://crbug.com/1116118
const base::Feature kScrollableTabStripButtons{
    "ScrollableTabStripButtons", base::FEATURE_DISABLED_BY_DEFAULT};

// Hosts some content in a side panel. https://crbug.com/1149995
const base::Feature kSidePanel{"SidePanel", base::FEATURE_DISABLED_BY_DEFAULT};

// Displays a prototype of the side panel. https://crbug.com/1181931
const base::Feature kSidePanelPrototype{"SidePanelPrototype",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Updated managed profile sign-in popup. https://crbug.com/1141224
const base::Feature kSyncConfirmationUpdatedText{
    "SyncConfirmationUpdatedText", base::FEATURE_DISABLED_BY_DEFAULT};

// Sign-in functionality in the profile creation flow. https://crbug.com/1126913
const base::Feature kSignInProfileCreation{"SignInProfileCreation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Smoother enterprise experience in the sign-in profile creation flow.
// https://crbug.com/1178494
const base::Feature kSignInProfileCreationEnterprise{
    "SignInProfileCreationEnterprise", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enables popup cards containing tab information when hovering over a tab.
// https://crbug.com/910739
const base::Feature kTabHoverCards{"TabHoverCards",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Parameter name used for tab hover cards user study.
// TODO(corising): Removed this after tab hover cards user study.
const char kTabHoverCardsFeatureParameterName[] = "setting";

// Enables preview images in hover cards. See kTabHoverCards.
// https://crbug.com/928954
const base::Feature kTabHoverCardImages{"TabHoverCardImages",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const char kTabHoverCardImagesNotReadyDelayParameterName[] =
    "page_not_ready_delay";
const char kTabHoverCardImagesLoadingDelayParameterName[] =
    "page_loading_delay";
const char kTabHoverCardImagesLoadedDelayParameterName[] = "page_loaded_delay";

// Enables tab outlines in additional situations for accessibility.
const base::Feature kTabOutlinesInLowContrastThemes{
    "TabOutlinesInLowContrastThemes", base::FEATURE_DISABLED_BY_DEFAULT};

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

const base::FeatureParam<double> kTabSearchTitleToHostnameWeightRatio{
    &kTabSearchFuzzySearch, "TabSearchTitleToHostnameWeightRatio", 2.0};

const base::FeatureParam<bool> kTabSearchMoveActiveTabToBottom{
    &kTabSearchFuzzySearch, "TabSearchMoveActiveTabToBottom", true};

const base::FeatureParam<int> kTabSearchRecentlyClosedDefaultItemDisplayCount{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedDefaultItemDisplayCount",
    8};

const base::FeatureParam<int> kTabSearchRecentlyClosedMaxEntries{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedMaxEntries", 100};

const base::Feature kToolbarUseHardwareBitmapDraw{
    "ToolbarUseHardwareBitmapDraw", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based separator that's only used for performance testing. See
// https://crbug.com/993502.
const base::Feature kWebFooterExperiment{"WebFooterExperiment",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// This enables enables persistence of a WebContents in a 1-to-1 association
// with the current Profile for WebUI bubbles. See https://crbug.com/1177048.
const base::Feature kWebUIBubblePerProfilePersistence{
    "WebUIBubblePerProfilePersistence", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the WebUI Download Shelf instead of the Views framework Download
// Shelf. See https://crbug.com/1180372.
const base::Feature kWebUIDownloadShelf{"WebUIDownloadShelf",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
const base::Feature kWebUITabStrip{"WebUITabStrip",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a WebUI Feedback UI, as opposed to the Chrome App UI. See
// https://crbug.com/1167223.
const base::Feature kWebUIFeedback{"WebUIFeedback",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
const base::Feature kHiddenNetworkWarning{"HiddenNetworkWarning",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
