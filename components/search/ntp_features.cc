// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace ntp_features {

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
BASE_FEATURE(kConfirmSuggestionRemovals,
             "ConfirmNtpSuggestionRemovals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the OneGooleBar cached response is sent back to NTP.
BASE_FEATURE(kCacheOneGoogleBar,
             "CacheOneGoogleBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chrome theme color will be set to match the NTP background
// on NTP Customize Chrome background change.
BASE_FEATURE(kCustomizeChromeColorExtraction,
             "CustomizeChromeColorExtraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Customize Chrome will be an option in the Unified Side Panel
// when on the New Tab Page.
BASE_FEATURE(kCustomizeChromeSidePanel,
             "CustomizeChromeSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the removal of the NTP background scrim and forced dark foreground
// colors for a specific subset of Chrome Web Store themes (see
// crbug.com/1329552). This is enabled by default to allow finch to disable this
// NTP treatment in the case of unexpected regressions.
BASE_FEATURE(kCwsScrimRemoval,
             "CwsScrimRemoval",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
BASE_FEATURE(kDismissPromos,
             "DismissNtpPromos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the NTP "realbox" will be themed like the omnibox
// (same background/text/selected/hover colors).
BASE_FEATURE(kRealboxMatchOmniboxTheme,
             "NtpRealboxMatchOmniboxTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the NTP "realbox" will have same border/drop shadow in hover
// state as searchbox.
BASE_FEATURE(kRealboxMatchSearchboxTheme,
             "NtpRealboxMatchSearchboxTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the NTP "realbox" will have the same rounded corners as
// searchbox.
BASE_FEATURE(kRealboxRoundedCorners,
             "NtpRealboxRoundedCorners",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
BASE_FEATURE(kRealboxUseGoogleGIcon,
             "NtpRealboxUseGoogleGIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, chrome cart module will be shown.
BASE_FEATURE(kNtpChromeCartModule,
             "NtpChromeCartModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Comprehensive Theming color changes will be applied to elements
// on the New Tab Page, excluding the Realbox.
BASE_FEATURE(kNtpComprehensiveTheming,
             "NtpComprehensiveTheming",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Comprehensive Theming color changes will be applied to the
// Realbox on the New Tab Page.
BASE_FEATURE(kNtpComprehensiveThemeRealbox,
             "NtpComprehensiveThemeRealbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !defined(OFFICIAL_BUILD)
// If enabled, dummy modules will be shown.
BASE_FEATURE(kNtpDummyModules,
             "NtpDummyModules",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// If enabled, Google Drive module will be shown.
BASE_FEATURE(kNtpDriveModule,
             "NtpDriveModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, handles navigations from the Most Visited tiles explicitly and
// overrides the navigation's transition type to bookmark navigation before the
// navigation is issued.
// TODO(crbug.com/1147589): When removing this flag, also remove the workaround
// in ChromeContentBrowserClient::OverrideNavigationParams.
BASE_FEATURE(kNtpHandleMostVisitedNavigationExplicitly,
             "HandleMostVisitedNavigationExplicitly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, logo will be shown.
BASE_FEATURE(kNtpLogo, "NtpLogo", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, middle slot promo will be shown.
BASE_FEATURE(kNtpMiddleSlotPromo,
             "NtpMiddleSlotPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, middle slot promo will be dismissed from New Tab Page until new
// promo message is populated
BASE_FEATURE(kNtpMiddleSlotPromoDismissal,
             "NtpMiddleSlotPromoDismissal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesLoadTimeoutMillisecondsParam".
BASE_FEATURE(kNtpModulesLoadTimeoutMilliseconds,
             "NtpModulesLoadTimeoutMilliseconds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesOrderParam".
BASE_FEATURE(kNtpModulesOrder,
             "NtpModulesOrder",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, modules will be able to be reordered via dragging and dropping
BASE_FEATURE(kNtpModulesDragAndDrop,
             "NtpModulesDragAndDrop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the first run experience for Modular NTP Desktop v1 will show.
BASE_FEATURE(kNtpModulesFirstRunExperience,
             "NtpModulesFirstRunExperience",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, modules will be loaded but not shown. This is useful to determine
// if a user would have seen modules in order to counterfactually log or
// trigger.
BASE_FEATURE(kNtpModulesLoad,
             "NtpModulesLoad",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, redesigned modules will be shown.
BASE_FEATURE(kNtpModulesRedesigned,
             "NtpModulesRedesigned",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a different module layout where modules are organized in rows and
// columns will be shown.
BASE_FEATURE(kNtpModulesRedesignedLayout,
             "NtpModulesRedesignedLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, OneGoogleBar will be shown.
BASE_FEATURE(kNtpOneGoogleBar,
             "NtpOneGoogleBar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Google Photos module will be shown.
BASE_FEATURE(kNtpPhotosModule,
             "NtpPhotosModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a customized title will be shown on the opt-in card.
BASE_FEATURE(kNtpPhotosModuleCustomizedOptInTitle,
             "NtpPhotosModuleCustomizedOptInTitle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a customized art work will be shown on the opt-in card.
BASE_FEATURE(kNtpPhotosModuleCustomizedOptInArtWork,
             "NtpPhotosModuleCustomizedOptInArtWork",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Google Photos opt-in card will show a button to soft opt-out.
BASE_FEATURE(kNtpPhotosModuleSoftOptOut,
             "NtpPhotosModuleSoftOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the single svg image show in Photos opt-in screen will be
// replaced by constituent images to support i18n.
BASE_FEATURE(kNtpPhotosModuleSplitSvgOptInArtWork,
             "NtpPhotosModuleSplitSvgOptInArtWork",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Following Feed module will be shown.
BASE_FEATURE(kNtpFeedModule,
             "NtpFeedModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Google Lens image search will be shown in the NTP Realbox.
BASE_FEATURE(kNtpRealboxLensSearch,
             "NtpRealboxLensSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, recipe tasks module will be shown.
BASE_FEATURE(kNtpRecipeTasksModule,
             "NtpRecipeTasksModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the scrim is removed.
BASE_FEATURE(kNtpRemoveScrim,
             "NtpRemoveScrim",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SafeBrowsing module will be shown to a target user.
BASE_FEATURE(kNtpSafeBrowsingModule,
             "NtpSafeBrowsingModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shortcuts will be shown.
BASE_FEATURE(kNtpShortcuts, "NtpShortcuts", base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double>
    kNtpElementLuminosityChangeForLightBackgroundParam{
        &kNtpComprehensiveTheming,
        "NtpElementLuminosityChangeForLightBackgroundParam", 0.1};

const base::FeatureParam<double>
    kNtpElementLuminosityChangeForDarkBackgroundParam{
        &kNtpComprehensiveTheming,
        "NtpElementLuminosityChangeForDarkBackgroundParam", 0.2};

const base::FeatureParam<std::string> kNtpOgbButtonSelectorParam{
    &kNtpRemoveScrim, "NtpOgbButtonSelectorParam", ".gb_A"};

const base::FeatureParam<std::string> kNtpOgbUnprotectedTextSelectorParam{
    &kNtpRemoveScrim, "NtpOgbUnprotectedTextSelectorParam", ".gb_d"};

const char kNtpModulesEligibleForHappinessTrackingSurveyParam[] =
    "NtpModulesEligibleForHappinessTrackingSurveyParam";
const char kNtpModulesLoadTimeoutMillisecondsParam[] =
    "NtpModulesLoadTimeoutMillisecondsParam";
const char kNtpModulesOrderParam[] = "NtpModulesOrderParam";
const char kNtpChromeCartModuleDataParam[] = "NtpChromeCartModuleDataParam";
const char kNtpChromeCartModuleAbandonedCartDiscountParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountParam";
const char kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountUseUtmParam";
const char kNtpChromeCartModuleHeuristicsImprovementParam[] =
    "NtpChromeCartModuleHeuristicsImprovementParam";
const char kNtpChromeCartModuleCouponParam[] = "NtpChromeCartModuleCouponParam";
const char kNtpDriveModuleDataParam[] = "NtpDriveModuleDataParam";
const char kNtpDriveModuleManagedUsersOnlyParam[] =
    "NtpDriveModuleManagedUsersOnlyParam";
const char kNtpDriveModuleCacheMaxAgeSParam[] =
    "NtpDriveModuleCacheMaxAgeSParam";
const char kNtpDriveModuleExperimentGroupParam[] =
    "NtpDriveModuleExperimentGroupParam";
const char kNtpMiddleSlotPromoDismissalParam[] =
    "NtpMiddleSlotPromoDismissalParam";
const char kNtpPhotosModuleDataParam[] = "NtpPhotosModuleDataParam";
const char kNtpPhotosModuleOptInTitleParam[] = "NtpPhotosModuleOptInTitleParam";
const char kNtpPhotosModuleOptInArtWorkParam[] =
    "NtpPhotosModuleOptInArtWorkParam";
const char kNtpSafeBrowsingModuleCooldownPeriodDaysParam[] =
    "NtpSafeBrowsingModuleCooldownPeriodDaysParam";
const char kNtpSafeBrowsingModuleCountMaxParam[] =
    "NtpSafeBrowsingModuleCountMaxParam";
const char kRealboxMatchOmniboxThemeVariantParam[] =
    "RealboxMatchOmniboxThemeVariantParam";
const char kNtpRecipeTasksModuleDataParam[] = "NtpRecipeTasksModuleDataParam";
const char kNtpRecipeTasksModuleCacheMaxAgeSParam[] =
    "NtpRecipeTasksModuleCacheMaxAgeSParam";
const char kNtpRecipeTasksModuleExperimentGroupParam[] =
    "NtpRecipeTasksModuleExperimentGroupParam";

base::TimeDelta GetModulesLoadTimeout() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kNtpModulesLoadTimeoutMilliseconds,
      kNtpModulesLoadTimeoutMillisecondsParam);
  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  unsigned int param_value_as_int = 0;
  if (!base::StringToUint(param_value, &param_value_as_int)) {
    return base::Seconds(3);
  }
  return base::Milliseconds(param_value_as_int);
}

std::vector<std::string> GetModulesOrder() {
  return base::SplitString(base::GetFieldTrialParamValueByFeature(
                               kNtpModulesOrder, kNtpModulesOrderParam),
                           ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

}  // namespace ntp_features
