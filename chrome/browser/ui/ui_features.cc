// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/feature_entry.h"
#include "ui/base/ui_base_features.h"

namespace features {

// Enables the tab dragging fallback when full window dragging is not supported
// by the platform (e.g. Wayland). See https://crbug.com/896640
BASE_FEATURE(kAllowWindowDragUsingSystemDragDrop,
             "AllowWindowDragUsingSystemDragDrop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of WGC for the Eye Dropper screen capture.
BASE_FEATURE(kAllowEyeDropperWGCScreenCapture,
             "AllowEyeDropperWGCScreenCapture",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);

// Enables icon in titlebar for web apps.
BASE_FEATURE(kWebAppIconInTitlebar,
             "WebAppIconInTitlebar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Chrome Labs menu in the toolbar. See https://crbug.com/1145666
BASE_FEATURE(kChromeLabs, "ChromeLabs", base::FEATURE_ENABLED_BY_DEFAULT);
const char kChromeLabsActivationParameterName[] =
    "chrome_labs_activation_percentage";
const base::FeatureParam<int> kChromeLabsActivationPercentage{
    &kChromeLabs, kChromeLabsActivationParameterName, 99};

// When enabled, clicks outside the omnibox and its popup will close an open
// omnibox popup.
BASE_FEATURE(kCloseOmniboxPopupOnInactiveAreaClick,
             "CloseOmniboxPopupOnInactiveAreaClick",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables updated copy and modified behavior for the default browser prompt.
BASE_FEATURE(kDefaultBrowserPromptRefresh,
             "DefaultBrowserPromptRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Parallel feature to track the group name for the synthetic trial.
BASE_FEATURE(kDefaultBrowserPromptRefreshTrial,
             "DefaultBrowserPromptRefreshTrial",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kDefaultBrowserPromptRefreshStudyGroup{
    &kDefaultBrowserPromptRefreshTrial, "group_name", ""};

const base::FeatureParam<bool> kShowDefaultBrowserInfoBar{
    &kDefaultBrowserPromptRefresh, "show_info_bar", true};

const base::FeatureParam<bool> kShowDefaultBrowserAppMenuChip{
    &kDefaultBrowserPromptRefresh, "show_app_menu_chip", false};

const base::FeatureParam<bool> kShowDefaultBrowserAppMenuItem{
    &kDefaultBrowserPromptRefresh, "show_app_menu_item", false};

const base::FeatureParam<bool> kUpdatedInfoBarCopy{
    &kDefaultBrowserPromptRefresh, "updated_info_bar_copy", true};

const base::FeatureParam<base::TimeDelta> kRepromptDuration{
    &kDefaultBrowserPromptRefresh, "reprompt_duration", base::Days(28)};

const base::FeatureParam<int> kMaxPromptCount{&kDefaultBrowserPromptRefresh,
                                              "max_prompt_count", -1};

const base::FeatureParam<int> kRepromptDurationMultiplier{
    &kDefaultBrowserPromptRefresh, "reprompt_duration_multiplier", 2};

const base::FeatureParam<base::TimeDelta> kDefaultBrowserAppMenuDuration{
    &kDefaultBrowserPromptRefresh, "app_menu_duration", base::Days(3)};

const base::FeatureParam<bool> kAppMenuChipColorPrimary{
    &kDefaultBrowserPromptRefresh, "app_menu_chip_color_primary", false};

// Create new Extensions app menu option (removing "More Tools -> Extensions")
// with submenu to manage extensions and visit chrome web store.
BASE_FEATURE(kExtensionsMenuInAppMenu,
             "ExtensionsMenuInAppMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsExtensionMenuInRootAppMenu() {
  return base::FeatureList::IsEnabled(kExtensionsMenuInAppMenu);
}

#if !defined(ANDROID)
// Enables "Access Code Cast" UI.
BASE_FEATURE(kAccessCodeCastUI,
             "AccessCodeCastUI",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables the feature to remove the last confirmation dialog when relaunching
// to update Chrome.
BASE_FEATURE(kFewerUpdateConfirmations,
             "FewerUpdateConfirmations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Enables showing the "Get the most out of Chrome" section in settings.
BASE_FEATURE(kGetTheMostOutOfChrome,
             "GetTheMostOutOfChrome",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature controls whether the user can be shown the Chrome for iOS promo
// when saving or updating passwords.
BASE_FEATURE(kIOSPromoRefreshedPasswordBubble,
             "IOSPromoRefreshedPasswordBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature controls whether the user can be shown the Chrome for iOS promo
// when saving or updating addresses.
BASE_FEATURE(kIOSPromoAddressBubble,
             "IOSPromoAddressBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature controls whether the user can be shown the Chrome for iOS promo
// when adding to the bookmarks.
BASE_FEATURE(kIOSPromoBookmarkBubble,
             "IOSPromoBookmarkBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature controls whether the user can be shown the Chrome for iOS promo
// when saving or updating payments.
BASE_FEATURE(kIOSPromoPaymentBubble,
             "IOSPromoPaymentBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This array lists the different activation params that can be passed in the
// experiment config, with their corresponding string.
constexpr base::FeatureParam<IOSPromoBookmarkBubbleActivation>::Option
    kIOSPromoBookmarkBubbleActivationOptions[] = {
        {IOSPromoBookmarkBubbleActivation::kContextual, "contextual"},
        {IOSPromoBookmarkBubbleActivation::kAlwaysShowWithBookmarkBubble,
         "always-show"},
};
constexpr base::FeatureParam<IOSPromoBookmarkBubbleActivation>
    kIOSPromoBookmarkBubbleActivationParam{
        &kIOSPromoBookmarkBubble, "activation",
        IOSPromoBookmarkBubbleActivation::kContextual,
        &kIOSPromoBookmarkBubbleActivationOptions};
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Happiness Tracking Surveys being delivered via chrome
// webui, rather than a separate static website.
BASE_FEATURE(kHaTSWebUI, "HaTSWebUI", base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, requesting to use the keyboard or pointer lock API causes a
// permission prompt to be shown.
BASE_FEATURE(kKeyboardAndPointerLockPrompt,
             "KeyboardAndPointerLockPrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Controls whether we use a different UX for simple extensions overriding
// settings.
BASE_FEATURE(kLightweightExtensionOverrideConfirmations,
             "LightweightExtensionOverrideConfirmations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Preloads a WebContents with a Top Chrome WebUI on BrowserView initialization,
// so that it can be shown instantly at a later time when necessary.
BASE_FEATURE(kPreloadTopChromeWebUI,
             "PreloadTopChromeWebUI",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kPreloadTopChromeWebUIModeName[] = "preload-mode";
const char kPreloadTopChromeWebUIModePreloadOnWarmupName[] =
    "preload-on-warmup";
const char kPreloadTopChromeWebUIModePreloadOnMakeContentsName[] =
    "preload-on-make-contents";
constexpr base::FeatureParam<PreloadTopChromeWebUIMode>::Option
    kPreloadTopChromeWebUIModeOptions[] = {
        {PreloadTopChromeWebUIMode::kPreloadOnWarmup,
         kPreloadTopChromeWebUIModePreloadOnWarmupName},
        {PreloadTopChromeWebUIMode::kPreloadOnMakeContents,
         kPreloadTopChromeWebUIModePreloadOnMakeContentsName},
};
const base::FeatureParam<PreloadTopChromeWebUIMode> kPreloadTopChromeWebUIMode{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUIModeName,
    PreloadTopChromeWebUIMode::kPreloadOnMakeContents,
    &kPreloadTopChromeWebUIModeOptions};
const char kPreloadTopChromeWebUISmartPreloadName[] = "smart-preload";
const base::FeatureParam<bool> kPreloadTopChromeWebUISmartPreload{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUISmartPreloadName, false};
const char kPreloadTopChromeWebUIDelayPreloadName[] = "delay-preload";
const base::FeatureParam<bool> kPreloadTopChromeWebUIDelayPreload{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUIDelayPreloadName, false};

// Enables exiting browser fullscreen (users putting the browser itself into the
// fullscreen mode via the browser UI or shortcuts) with press-and-hold Esc.
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen,
             "PressAndHoldEscToExitBrowserFullscreen",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable responsive toolbar. Toolbar buttons overflow to a chevron button when
// the browser width is resized smaller than normal.
BASE_FEATURE(kResponsiveToolbar,
             "ResponsiveToolbar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the side search feature for Google Search. Presents recent Google
// search results in a browser side panel.
BASE_FEATURE(kSideSearch, "SideSearch", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSideSearchFeedback,
             "SideSearchFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Displays right-click search results of a highlighted text in side panel,
// So users are not forced to switch to a new tab to view the search results
BASE_FEATURE(kSearchWebInSidePanel,
             "SearchWebInSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature that controls whether or not feature engagement configurations can be
// used to control automatic triggering for side search.
BASE_FEATURE(kSideSearchAutoTriggering,
             "SideSearchAutoTriggering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature param that determines how many times a user has to return to a given
// SRP before we automatically trigger the side search side panel for that SRP
// on a subsequent navigation.
const base::FeatureParam<int> kSideSearchAutoTriggeringReturnCount{
    &kSideSearchAutoTriggering, "SideSearchAutoTriggeringReturnCount", 2};

BASE_FEATURE(kSidePanelWebView,
             "SidePanelWebView",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSidePanelJourneysQueryless,
             "SidePanelJourneysQueryless",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !defined(ANDROID)
BASE_FEATURE(kSidePanelCompanionDefaultPinned,
             "SidePanelCompanionDefaultPinned",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSidePanelResizing,
             "SidePanelResizing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables buttons when scrolling the tabstrip https://crbug.com/951078
BASE_FEATURE(kTabScrollingButtonPosition,
             "TabScrollingButtonPosition",
             base::FEATURE_ENABLED_BY_DEFAULT);
const char kTabScrollingButtonPositionParameterName[] = "buttonPosition";

// Enables tabs to be frozen when collapsed.
// https://crbug.com/1110108
BASE_FEATURE(kTabGroupsCollapseFreezing,
             "TabGroupsCollapseFreezing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables preview images in tab-hover cards.
// https://crbug.com/928954
BASE_FEATURE(kTabHoverCardImages,
             "TabHoverCardImages",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

const char kTabHoverCardImagesNotReadyDelayParameterName[] =
    "page_not_ready_delay";
const char kTabHoverCardImagesLoadingDelayParameterName[] =
    "page_loading_delay";
const char kTabHoverCardImagesLoadedDelayParameterName[] = "page_loaded_delay";
const char kTabHoverCardImagesCrossfadePreviewAtParameterName[] =
    "crossfade_preview_at";
const char kTabHoverCardAdditionalMaxWidthDelay[] =
    "additional_max_width_delay";

BASE_FEATURE(kTabOrganization,
             "TabOrganization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabOrganization() {
  return base::FeatureList::IsEnabled(features::kTabOrganization);
}

BASE_FEATURE(kTabstripDeclutter,
             "TabstripDeclutter",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabstripDeclutter() {
  return base::FeatureList::IsEnabled(features::kTabstripDeclutter);
}

BASE_FEATURE(kMultiTabOrganization,
             "MultiTabOrganization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationAppMenuItem,
             "TabOrganizationAppMenuItem",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabReorganization,
             "TabReorganization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabReorganizationDivider,
             "TabReorganizationDivider",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationModelStrategy,
             "TabOrganizationModelStrategy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kTabOrganizationTriggerPeriod{
    &kTabOrganization, "trigger_period", base::Hours(6)};

const base::FeatureParam<double> kTabOrganizationTriggerBackoffBase{
    &kTabOrganization, "backoff_base", 2.0};

const base::FeatureParam<double> kTabOrganizationTriggerThreshold{
    &kTabOrganization, "trigger_threshold", 7.0};

const base::FeatureParam<double> kTabOrganizationTriggerSensitivityThreshold{
    &kTabOrganization, "trigger_sensitivity_threshold", 0.5};

const base::FeatureParam<bool> KTabOrganizationTriggerDemoMode{
    &kTabOrganization, "trigger_demo_mode", false};

BASE_FEATURE(kTabSearchChevronIcon,
             "TabSearchChevronIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the tab search submit feedback button.
BASE_FEATURE(kTabSearchFeedback,
             "TabSearchFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);



// Controls feature parameters for Tab Search's `Recently Closed` entries.
BASE_FEATURE(kTabSearchRecentlyClosed,
             "TabSearchRecentlyClosed",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kTabSearchRecentlyClosedDefaultItemDisplayCount{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedDefaultItemDisplayCount",
    8};

const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold{
    &kTabSearchRecentlyClosed, "TabSearchRecentlyClosedTabCountThreshold", 100};

// Enables creating a web app window when tearing off a tab with a url
// controlled by a web app.
BASE_FEATURE(kTearOffWebAppTabOpensWebAppWindow,
             "TearOffWebAppTabOpensWebAppWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !defined(ANDROID)
BASE_FEATURE(kToolbarPinning,
             "ToolbarPinning",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsToolbarPinningEnabled() {
  return base::FeatureList::IsEnabled(kToolbarPinning);
}
#endif

// Controls whether top chrome pages will use the spare renderer if no top
// chrome renderers are present.
BASE_FEATURE(kTopChromeWebUIUsesSpareRenderer,
             "TopChromeWebUIUsesSpareRenderer",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables alternate update-related text to be displayed in browser app menu
// button, menu item and confirmation dialog.
BASE_FEATURE(kUpdateTextOptions,
             "UpdateTextOptions",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Used to present different flavors of update strings in browser app menu
// button.
const base::FeatureParam<int> kUpdateTextOptionNumber{
    &kUpdateTextOptions, "UpdateTextOptionNumber", 2};
#endif

// Enables enterprise profile badging on the toolbar avatar and in the profile
// menu. This will act as a kill switch.
BASE_FEATURE(kEnterpriseProfileBadging,
             "EnterpriseProfileBadging",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the management button on the toolbar for all managed browsers.
BASE_FEATURE(kManagementToolbarButton,
             "ManagementToolbarButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the management button on the toolbar by default for browser managed
// by trusted sources.
BASE_FEATURE(kManagementToolbarButtonForTrustedManagementSources,
             "ManagementToolbarButtonForTrustedManagementSources",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseUpdatedProfileCreationScreen,
             "EnterpriseUpdatedProfileCreationScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables enables persistence of a WebContents in a 1-to-1 association
// with the current Profile for WebUI bubbles. See https://crbug.com/1177048.
BASE_FEATURE(kWebUIBubblePerProfilePersistence,
             "WebUIBubblePerProfilePersistence",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
BASE_FEATURE(kWebUITabStrip,
             "WebUITabStrip",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// The default value of this flag is aligned with platform behavior to handle
// context menu with touch.
// TODO(crbug.com/40796475): Enable this flag for all platforms after launch.
BASE_FEATURE(kWebUITabStripContextMenuAfterTap,
             "WebUITabStripContextMenuAfterTap",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kViewsFirstRunDialog,
             "ViewsFirstRunDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kViewsJSAppModalDialog,
             "ViewsJSAppModalDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Reduce resource usage when view is hidden by not rendering loading animation.
// TODO(crbug.com/40224168): Clean up the feature in M117.
BASE_FEATURE(kStopLoadingAnimationForHiddenWindow,
             "StopLoadingAnimationForHiddenWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUsePortalAccentColor,
             "UsePortalAccentColor",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kCompactMode, "CompactMode", base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
