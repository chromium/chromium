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
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, shows an extension card within the Customize Chrome Side
// Panel for access to the Chrome Web Store extensions.
BASE_FEATURE(kCustomizeChromeSidePanelExtensionsCard,
             "CustomizeChromeSidePanelExtensionsCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows wallpaper search within the Cusotmize Chrome Side Panel.
BASE_FEATURE(kCustomizeChromeWallpaperSearch,
             "CustomizeChromeWallpaperSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows inspiration card in Customize Chrome Side Panel Wallpaper
// Search.
BASE_FEATURE(kCustomizeChromeWallpaperSearchInspirationCard,
             "CustomizeChromeWallpaperSearchInspirationCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces a dark Google logo for a specific subset of Chrome Web Store themes
// (see crbug.com/1329552). This is enabled by default to allow finch to disable
// this NTP treatment in the case of unexpected regressions.
BASE_FEATURE(kCwsDarkLogo, "CwsDarkLogo", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
BASE_FEATURE(kDismissPromos,
             "DismissNtpPromos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, all NTP "realbox" Chrome Refresh features will be enabled
BASE_FEATURE(kRealboxCr23All,
             "NtpRealboxCr23All",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP "realbox" will have consistent row height. Includes changing
// entity sizes and inlining subtitles.
BASE_FEATURE(kRealboxCr23ConsistentRowHeight,
             "NtpRealboxCr23ConsistentRowHeight",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP "realbox" expanded state icon CR23 updates will appear.
// Includes CR23 icons as well as backgrounds for AiS and pedal suggestions and
// updated entity corner radii.
BASE_FEATURE(kRealboxCr23ExpandedStateIcons,
             "NtpRealboxCr23ExpandedStateIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP "realbox" expanded state layout CR23 updates will
// appear.
BASE_FEATURE(kRealboxCr23ExpandedStateLayout,
             "NtpRealboxCr23ExpandedStateLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP "realbox" will be themed for CR23. Includes realbox
// matching omnibox theme and increased realbox shadow.
BASE_FEATURE(kRealboxCr23Theming,
             "NtpRealboxCr23Theming",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the NTP "realbox" will have same border/drop shadow in hover
// state as searchbox.
BASE_FEATURE(kRealboxMatchSearchboxTheme,
             "NtpRealboxMatchSearchboxTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Determines the behavior of the width of the realbox in relation to the width
// for secondary column.
BASE_FEATURE(kRealboxWidthBehavior,
             "NtpRealboxWidthBehavior",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the realbox will be taller.
BASE_FEATURE(kRealboxIsTall,
             "NtpRealboxIsTall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
BASE_FEATURE(kRealboxUseGoogleGIcon,
             "NtpRealboxUseGoogleGIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, alpha NTP backgrounds will show in Customize Chrome.
BASE_FEATURE(kNtpAlphaBackgroundCollections,
             "NtpAlphaBackgroundCollections",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP background image links will be checked for HTTP status
// errors."
BASE_FEATURE(kNtpBackgroundImageErrorDetection,
             "NtpBackgroundImageErrorDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, chrome cart module will be shown.
BASE_FEATURE(kNtpChromeCartModule,
             "NtpChromeCartModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, segmentation data will be collected to decide whether or not to
// show the Drive module.
BASE_FEATURE(kNtpDriveModuleSegmentation,
             "NtpDriveModuleSegmentation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, six files show in the NTP Drive module, instead of three.
BASE_FEATURE(kNtpDriveModuleShowSixFiles,
             "NtpDriveModuleShowSixFiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, logo will fill up less vertical space.
BASE_FEATURE(kNtpReducedLogoSpace,
             "NtpReducedLogoSpace",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If true, extends width of modules if space allows.
BASE_FEATURE(kNtpWideModules,
             "NtpWideModules",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesOrderParam".
BASE_FEATURE(kNtpModulesOrder,
             "NtpModulesOrder",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesMaxColumnCountParam".
BASE_FEATURE(kNtpModulesMaxColumnCount,
             "NtpModulesMaxColumnCount",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesLoadedWithOtherModulesMaxInstanceCount"
BASE_FEATURE(kNtpModulesLoadedWithOtherModulesMaxInstanceCount,
             "NtpModulesLoadedWithOtherModulesMaxInstanceCount",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, displays a horizontal scrollbar on overflowing modules.
BASE_FEATURE(kNtpModulesOverflowScrollbar,
             "NtpModulesOverflowScrollbar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, modules will be able to be reordered via dragging and dropping
BASE_FEATURE(kNtpModulesDragAndDrop,
             "NtpModulesDragAndDrop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the first run experience for Modular NTP Desktop v1 will show.
BASE_FEATURE(kNtpModulesFirstRunExperience,
             "NtpModulesFirstRunExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, MostVisited tiles will reflow when overflowing.
BASE_FEATURE(kNtpMostVisitedReflowOnOverflow,
             "NtpMostVisitedReflowOnOverflow",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Google Lens image search will call Lens v3 direct upload
// endpoint instead of uploading to Scotty.
BASE_FEATURE(kNtpLensDirectUpload,
             "NtpLensDirectUpload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, recipe tasks module will be shown.
BASE_FEATURE(kNtpRecipeTasksModule,
             "NtpRecipeTasksModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SafeBrowsing module will be shown to a target user.
BASE_FEATURE(kNtpSafeBrowsingModule,
             "NtpSafeBrowsingModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shortcuts will be shown.
BASE_FEATURE(kNtpShortcuts, "NtpShortcuts", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, shortcuts will be shown in a wide single row.
BASE_FEATURE(kNtpSingleRowShortcuts,
             "NtpSingleRowShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the History clusters module will be shown.
BASE_FEATURE(kNtpHistoryClustersModule,
             "NtpHistoryClustersModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Dummy feature to set kNtpHistoryClustersModuleBeginTimeDurationHoursParam.
BASE_FEATURE(kNtpHistoryClustersModuleBeginTimeDuration,
             "NtpHistoryClustersModuleBeginTimeDuration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set kNtpHistoryClustersModuleMinimumVisitsRequiredParam.
BASE_FEATURE(kNtpHistoryClustersModuleMinimumVisitsRequired,
             "NtpHistoryClustersModuleMinimumVisitsRequired",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set kNtpHistoryClustersModuleMinimumImagesRequiredParam.
BASE_FEATURE(kNtpHistoryClustersModuleMinimumImagesRequired,
             "NtpHistoryClustersModuleMinimumImagesRequired",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set kNtpHistoryClustersModuleCategoriesParam.
BASE_FEATURE(kNtpHistoryClustersModuleCategories,
             "NtpHistoryClustersModuleCategories",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the history clusters module will be loaded but not shown. This is
// useful to determine if a user would have seen modules in order to
// counterfactually log or trigger.
BASE_FEATURE(kNtpHistoryClustersModuleLoad,
             "NtpHistoryClustersModuleLoad",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set kNtpHistoryClustersModuleMaxClustersParam.
BASE_FEATURE(kNtpHistoryClustersModuleMaxClusters,
             "NtpHistoryClustersMaxClusters",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, module headers will display an associated icon.
BASE_FEATURE(kNtpModulesHeaderIcon,
             "NtpModulesHeaderIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled a suggestion chip will show in the header for Quests V2.
BASE_FEATURE(kNtpHistoryClustersModuleSuggestionChipHeader,
             "NtpHistoryClustersModuleSuggestionChipHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Discounts badge will show on the visit tile in the History
// clusters module when available.
BASE_FEATURE(kNtpHistoryClustersModuleDiscounts,
             "NtpHistoryClustersModuleDiscounts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, ChromeCart tile will show in the History clusters module when
// available.
BASE_FEATURE(kNtpChromeCartInHistoryClusterModule,
             "NtpChromeCartInHistoryClusterModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpHistoryClustersModuleUseModelRanking,
             "NtpHistoryClustersModuleUseModelRanking",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpHistoryClustersModuleTextOnly,
             "NtpHistoryClustersModuleTextOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, ChromeCart module will show together with ChromeCart+History
// cluster module when available.
BASE_FEATURE(kNtpChromeCartHistoryClusterCoexist,
             "NtpChromeCartHistoryClusterCoexist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the History Clusters module will attempt to fetch clusters until
// it has enough clusters for the module or the History Clusters service says
// that all visits have been exhausted.
BASE_FEATURE(kNtpHistoryClustersModuleFetchClustersUntilExhausted,
             "NtpHistoryClustersModuleFetchClustersUntilExhausted",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the History clusters module will contain visits from other
// devices.
BASE_FEATURE(kNtpHistoryClustersModuleIncludeSyncedVisits,
             "NtpHistoryClustersModuleIncludeSyncedVisits",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the History clsuters module will enable content clustering for
// the displayed clusters.
BASE_FEATURE(kNtpHistoryClustersModuleEnableContentClustering,
             "HistoryClustersModuleEnableContentClustering",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNtpTabResumptionModule,
             "NtpTabResumptionModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kNtpModuleIgnoredCriteriaThreshold[] =
    "NtpModuleIgnoredCriteriaThreshold";
const char kNtpModuleIgnoredHaTSDelayTimeParam[] =
    "NtpModuleIgnoredHaTSDelayTimeParam";
const char kNtpModulesEligibleForHappinessTrackingSurveyParam[] =
    "NtpModulesEligibleForHappinessTrackingSurveyParam";
const char kNtpModulesInteractionBasedSurveyEligibleIdsParam[] =
    "NtpModulesInteractionBasedSurveyEligibleIdsParam";
const char kNtpModulesLoadTimeoutMillisecondsParam[] =
    "NtpModulesLoadTimeoutMillisecondsParam";
const char kNtpModulesLoadedWithOtherModulesMaxInstanceCountParam[] =
    "NtpModulesLoadedWithOtherModulesMaxInstanceCountParam";
const char kNtpModulesMaxColumnCountParam[] = "NtpModulesMaxColumnCountParam";
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
const char kNtpHistoryClustersModuleDataParam[] =
    "NtpHistoryClustersModuleDataParam";
const char kNtpChromeCartInHistoryClustersModuleDataParam[] =
    "NtpChromeCartInHistoryClustersModuleDataParam";
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
const char kNtpRecipeTasksModuleDataParam[] = "NtpRecipeTasksModuleDataParam";
const char kNtpRecipeTasksModuleCacheMaxAgeSParam[] =
    "NtpRecipeTasksModuleCacheMaxAgeSParam";
const char kNtpRecipeTasksModuleExperimentGroupParam[] =
    "NtpRecipeTasksModuleExperimentGroupParam";
const char kNtpHistoryClustersModuleBeginTimeDurationHoursParam[] =
    "NtpHistoryClustersModuleBeginTimeDurationHoursParam";
const char kNtpHistoryClustersModuleMinimumVisitsRequiredParam[] =
    "NtpHistoryClustersModuleMinimumVisitsRequiredParam";
const char kNtpHistoryClustersModuleMinimumImagesRequiredParam[] =
    "NtpHistoryClustersModuleMinimumImagesRequiredParam";
const char kNtpHistoryClustersModuleCategoriesAllowlistParam[] =
    "NtpHistoryClustersModuleCategoriesParam";
const char kNtpHistoryClustersModuleCategoriesBlocklistParam[] =
    "NtpHistoryClustersModuleCategoriesBlocklistParam";
const char kNtpHistoryClustersModuleCategoriesBoostlistParam[] =
    "NtpHistoryClustersModuleCategoriesBoostlistParam";
const char kNtpHistoryClustersModuleMaxClustersParam[] =
    "NtpHistoryClustersModuleMaxClustersParam";
const char kNtpHistoryClustersModuleScoreThresholdParam[] =
    "NtpHistoryClustersModuleScoreThresholdParam";
const char kNtpRealboxWidthBehaviorParam[] = "NtpRealboxWidthBehaviorParam";
const char kNtpTabResumptionModuleDataParam[] =
    "NtpTabResumptionModuleDataParam";

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

int GetModulesMaxColumnCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kNtpModulesMaxColumnCount, kNtpModulesMaxColumnCountParam, 3);
}

int GetMultipleLoadedModulesMaxModuleInstanceCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kNtpModulesLoadedWithOtherModulesMaxInstanceCount,
      kNtpModulesLoadedWithOtherModulesMaxInstanceCountParam, 2);
}

std::vector<std::string> GetModulesOrder() {
  return base::SplitString(base::GetFieldTrialParamValueByFeature(
                               kNtpModulesOrder, kNtpModulesOrderParam),
                           ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

}  // namespace ntp_features
