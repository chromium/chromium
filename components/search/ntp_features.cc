// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace {

const char kMobilePromoQRCodeURL[] =
    "https://apps.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=desktop-chr-ntp&mt=8";

}  // namespace

namespace ntp_features {

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
BASE_FEATURE(kConfirmSuggestionRemovals,
             "ConfirmNtpSuggestionRemovals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows an extension card within the Customize Chrome Side
// Panel for access to the Chrome Web Store extensions.
BASE_FEATURE(kCustomizeChromeSidePanelExtensionsCard,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows wallpaper search within the Customize Chrome Side Panel.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kCustomizeChromeWallpaperSearch,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// If enabled, shows entry point on Customize Chrome Side Panel's Appearance
// page for Wallpaper Search.";
BASE_FEATURE(kCustomizeChromeWallpaperSearchButton,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows inspiration card in Customize Chrome Side Panel Wallpaper
// Search.
BASE_FEATURE(kCustomizeChromeWallpaperSearchInspirationCard,
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
BASE_FEATURE(kRealboxUseGoogleGIcon,
             "NtpRealboxUseGoogleGIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, alpha NTP backgrounds will show in Customize Chrome.
// This is a development switch. Keep indefinitely.
BASE_FEATURE(kNtpAlphaBackgroundCollections, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP background image links will be checked for HTTP status
// errors."
BASE_FEATURE(kNtpBackgroundImageErrorDetection,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calendar module will be shown.
BASE_FEATURE(kNtpCalendarModule, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, chrome cart module will be shown.
BASE_FEATURE(kNtpChromeCartModule, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, customization of Chrome will be promoted on the NTP.
BASE_FEATURE(kNtpCustomizeChromeAutoOpen, base::FEATURE_DISABLED_BY_DEFAULT);

#if !defined(OFFICIAL_BUILD)
// If enabled, dummy modules will be shown.
// This is a development switch. Keep indefinitely.
BASE_FEATURE(kNtpDummyModules, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// If enabled, Google Drive module will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpDriveModule, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the NTP Drive module does not require sync.
BASE_FEATURE(kNtpDriveModuleHistorySyncRequirement,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, segmentation data will be collected to decide whether or not to
// show the Drive module.
BASE_FEATURE(kNtpDriveModuleSegmentation, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, six files show in the NTP Drive module, instead of three.
BASE_FEATURE(kNtpDriveModuleShowSixFiles, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, logo will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpLogo, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, middle slot promo will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpMiddleSlotPromo, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, middle slot promo will be dismissed from New Tab Page until new
// promo message is populated
BASE_FEATURE(kNtpMiddleSlotPromoDismissal, base::FEATURE_ENABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesLoadTimeoutMillisecondsParam".
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpModulesLoadTimeoutMilliseconds,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesOrderParam".
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpModulesOrder, base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesMaxColumnCountParam".
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpModulesMaxColumnCount, base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpModulesLoadedWithOtherModulesMaxInstanceCount"
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpModulesLoadedWithOtherModulesMaxInstanceCount,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, modules will be able to be reordered via dragging and dropping
BASE_FEATURE(kNtpModulesDragAndDrop, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, modules will be loaded but not shown. This is useful to determine
// if a user would have seen modules in order to counterfactually log or
// trigger.
BASE_FEATURE(kNtpModulesLoad, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, makes browser sign-in requirement per-module, instead of a
// requirement for all modules.
BASE_FEATURE(kNtpModuleSignInRequirement, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, OneGoogleBar will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpOneGoogleBar, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, outlook calendar module will be shown.
BASE_FEATURE(kNtpOutlookCalendarModule, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Google Photos module will be shown.
BASE_FEATURE(kNtpPhotosModule, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a customized title will be shown on the opt-in card.
BASE_FEATURE(kNtpPhotosModuleCustomizedOptInTitle,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a customized art work will be shown on the opt-in card.
BASE_FEATURE(kNtpPhotosModuleCustomizedOptInArtWork,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Google Photos opt-in card will show a button to soft opt-out.
BASE_FEATURE(kNtpPhotosModuleSoftOptOut, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the single svg image show in Photos opt-in screen will be
// replaced by constituent images to support i18n.
BASE_FEATURE(kNtpPhotosModuleSplitSvgOptInArtWork,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Following Feed module will be shown.
BASE_FEATURE(kNtpFeedModule, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SafeBrowsing module will be shown to a target user.
BASE_FEATURE(kNtpSafeBrowsingModule, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, sharepoint module will be shown.
BASE_FEATURE(kNtpSharepointModule, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, shortcuts will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpShortcuts, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Tab Resumption module will be shown.
BASE_FEATURE(kNtpMostRelevantTabResumptionModule,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Tab Resumption module will be allowed to fallback to the
// favicon server when fetching favicons for displayed continuation suggestions.
BASE_FEATURE(kNtpMostRelevantTabResumptionAllowFaviconServerFallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Tab Resumption module will fallback to host url to find a
// favicon if there are none locally available.
BASE_FEATURE(kNtpMostRelevantTabResumptionModuleFallbackToHost,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpTabResumptionModuleCategories,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set how recent tabs must be to be shown.
BASE_FEATURE(kNtpTabResumptionModuleTimeLimit,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows entry point on New Tab Page for Customize Chrome Side Panel
// Wallpaper Search.
BASE_FEATURE(kNtpWallpaperSearchButton, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, animates New Tab Page's Wallpaper Search Button.
BASE_FEATURE(kNtpWallpaperSearchButtonAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpWallpaperSearchButtonHideCondition".
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpWallpaperSearchButtonHideCondition,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dummy feature to set param "NtpWallpaperSearchButtonAnimationShownThreshold".
// This is used for an emergency Finch param. Keep indefinitely.
BASE_FEATURE(kNtpWallpaperSearchButtonAnimationShownThreshold,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to control the display of a mobile promo on the NTP.
BASE_FEATURE(kNtpMobilePromo, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the Microsoft Authentication module will be shown.
BASE_FEATURE(kNtpMicrosoftAuthenticationModule,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the features of NTP Next (AI action chips etc.) will be shown.
BASE_FEATURE(kNtpNextFeatures, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the OGB loader will request for the async bar parts payload type.
BASE_FEATURE(kNtpOneGoogleBarAsyncBarParts, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a footer will show on the NTP.
BASE_FEATURE(kNtpFooter, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, tab groups module will be shown.
BASE_FEATURE(kNtpTabGroupsModule,
             "kNtpTabGroupsModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, zero tab group card will be shown if the user doesn't have any
// tab groups.
BASE_FEATURE(kNtpTabGroupsModuleZeroState,
             "kNtpTabGroupsModuleZeroState",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, stale features will be auto-removed from the NTP.
BASE_FEATURE(kNtpFeatureOptimization,
             "kNtpFeatureOptimization",
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
const char kNtpCalendarModuleDataParam[] = "NtpCalendarModuleDataParam";
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
const char kNtpOutlookCalendarModuleDataParam[] =
    "NtpOutlookCalendarModuleDataParam";
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
const char kNtpMostRelevantTabResumptionModuleDataParam[] =
    "NtpMostRelevantTabResumptionModuleDataParam";
const char kNtpMostRelevantTabResumptionModuleMaxVisitsParam[] =
    "NtpMostRelevantTabResumptionModuleMaxVisitsParam";
const char kNtpTabGroupsModuleDataParam[] = "NtpTabGroupsModuleDataParam";
const char kNtpTabResumptionModuleCategoriesBlocklistParam[] =
    "NtpTabResumptionModuleCategoriesBlocklistParam";
const char kNtpTabResumptionModuleDismissalDurationParam[] =
    "NtpMostRelevantTabResumptionModuleDismissalDurationParam";
const char kNtpTabResumptionModuleDataParam[] =
    "NtpTabResumptionModuleDataParam";
const char kNtpTabResumptionModuleResultTypesParam[] =
    "NtpTabResumptionModuleResultTypesParam";
const char kNtpTabResumptionModuleTimeLimitParam[] =
    "NtpTabResumptionModuleTimeLimitParam";
const char kNtpTabResumptionModuleVisibilityThresholdDataParam[] =
    "NtpTabResumptionModuleVisibilityThresholdDataParam";
const char kNtpWallpaperSearchButtonHideConditionParam[] =
    "NtpWallpaperSearchButtonHideConditionParam";
const char kNtpWallpaperSearchButtonAnimationShownThresholdParam[] =
    "NtpWallpaperSearchButtonAnimationShownThresholdParam";
const char kWallpaperSearchHatsDelayParam[] = "WallpaperSearchHatsDelayParam";
const char kNtpMobilePromoTargetUrlParam[] = "NtpMobilePromoTargetUrlParam";

const base::FeatureParam<bool> kNtpNextShowStaticTextParam(
    &ntp_features::kNtpNextFeatures,
    "NtpNextShowStaticTextParam",
    false);
const base::FeatureParam<bool> kNtpNextShowDeepDiveSuggestionsParam(
    &ntp_features::kNtpNextFeatures,
    "NtpNextShowDeepDiveSuggestionsParam",
    false);
const base::FeatureParam<bool>
    kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam(
        &ntp_features::kNtpNextFeatures,
        "NtpNextSuggestionsFromNewSearchSuggestionsEndpointParam",
        false);
const base::FeatureParam<bool> kNtpNextShowSimplificationUIParam(
    &ntp_features::kNtpNextFeatures,
    "NtpNextShowSimplificationUIParam",
    false);
const base::FeatureParam<int> kMaxTilesBeforeShowMore{
    &ntp_features::kNtpNextFeatures, "max_tiles_before_show_more", 5};
const base::FeatureParam<bool> kAddTabUploadDelayOnActionChipClick(
    &ntp_features::kNtpNextFeatures,
    "AddTabUploadDelayOnActionChipClick",
    true);

const base::FeatureParam<int> kNtpCustomizeChromeAutoShownMaxCount(
    &ntp_features::kNtpCustomizeChromeAutoOpen,
    "max_customize_chrome_auto_shown_count",
    5);
const base::FeatureParam<int> kNtpCustomizeChromeAutoShownSessionMaxCount(
    &ntp_features::kNtpCustomizeChromeAutoOpen,
    "max_customize_chrome_auto_shown_session_count",
    5);

const base::FeatureParam<std::string> kNtpCalendarModuleExperimentParam(
    &ntp_features::kNtpCalendarModule,
    "NtpCalendarModuleMaxExperimentParam",
    "ntp-calendar");
const base::FeatureParam<int> kNtpCalendarModuleMaxEventsParam(
    &ntp_features::kNtpCalendarModule,
    "NtpCalendarModuleMaxEventsParam",
    5);
const base::FeatureParam<base::TimeDelta> kNtpCalendarModuleWindowEndDeltaParam(
    &ntp_features::kNtpCalendarModule,
    "NtpCalendarModuleWindowEndDeltaParam",
    base::Hours(12));
const base::FeatureParam<base::TimeDelta>
    kNtpCalendarModuleWindowStartDeltaParam(
        &ntp_features::kNtpCalendarModule,
        "NtpCalendarModuleWindowStartDeltaParam",
        base::Minutes(-15));
const base::FeatureParam<bool> kNtpOutlookCalendarModuleAttachmentCheckParam(
    &ntp_features::kNtpOutlookCalendarModule,
    "NtpOutlookCalendarModuleAttachmentCheckParam",
    true);
extern const base::FeatureParam<bool>
    kNtpOutlookCalendarModuleDisableAttachmentsParam(
        &ntp_features::kNtpOutlookCalendarModule,
        "NtpOutlookCalendarModuleDisableAttachmentsParam",
        false);
const base::FeatureParam<int> kNtpOutlookCalendarModuleMaxEventsParam(
    &ntp_features::kNtpOutlookCalendarModule,
    "NtpOutlookCalendarModuleMaxEventsParam",
    5);
const base::FeatureParam<base::TimeDelta>
    kNtpOutlookCalendarModuleRetrievalWindowParam(
        &ntp_features::kNtpOutlookCalendarModule,
        "NtpOutlookCalendarModuleRetrievalWindowParam",
        base::Hours(12));
const base::FeatureParam<bool> kNtpRealboxCr23ExpandedStateBgMatchesOmnibox(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23ExpandedStateBgMatchesOmnibox",
    true);
const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23SteadyStateShadow",
    false);
const base::FeatureParam<int> kNtpMobilePromoImpressionLimit(
    &ntp_features::kNtpMobilePromo,
    "kNtpMobilePromoImpressionLimit",
    10);
const base::FeatureParam<bool>
    kNtpMostRelevantTabResumptionModuleFilterLocalTabsParam{
        &kNtpMostRelevantTabResumptionModule,
        /*name=*/"local_tab_filter",
        /*default_value=*/true};

const base::FeatureParam<NtpSharepointModuleDataType>::Option
    kNtpSharepointModuleDataTypeOptions[] = {
        {NtpSharepointModuleDataType::kTrendingInsights, "trending-insights"},
        {NtpSharepointModuleDataType::kNonInsights, "non-insights"},
        {NtpSharepointModuleDataType::kTrendingInsightsFakeData,
         "fake-trending"},
        {NtpSharepointModuleDataType::kNonInsightsFakeData,
         "fake-non-insights"},
        {NtpSharepointModuleDataType::kCombinedSuggestions, "combined"}};

const base::FeatureParam<NtpSharepointModuleDataType>
    kNtpSharepointModuleDataParam{&ntp_features::kNtpSharepointModule,
                                  "NtpSharepointModuleDataParam",
                                  NtpSharepointModuleDataType::kNonInsights,
                                  &kNtpSharepointModuleDataTypeOptions};

const base::FeatureParam<int> kNtpMicrosoftFilesModuleMaxFilesParam(
    &ntp_features::kNtpSharepointModule,
    "NtpMicrosoftFilesModuleMaxFilesParam",
    6);

const base::FeatureParam<int>
    kNtpMicrosoftFilesModuleMaxTrendingFilesForCombinedParam(
        &ntp_features::kNtpSharepointModule,
        "NtpMicrosoftFilesModuleMaxTrendingFilesForCombinedParam",
        2);

const base::FeatureParam<int>
    kNtpMicrosoftFilesModuleMaxNonInsightsFilesForCombinedParam(
        &ntp_features::kNtpSharepointModule,
        "NtpMicrosoftFilesModuleMaxNonInsightsFilesForCombinedParam",
        4);

const base::FeatureParam<base::TimeDelta>
    kNtpTabGroupsModuleWindowEndDeltaParam(
        &ntp_features::kNtpTabGroupsModule,
        "NtpTabGroupsModuleWindowEndDeltaParam",
        base::Hours(12));

const base::FeatureParam<size_t> kNtpTabGroupsModuleMaxGroupCountParam(
    &ntp_features::kNtpTabGroupsModule,
    "kNtpTabGroupsModuleMaxGroupCountParam",
    4);

const base::FeatureParam<bool> kEnableStaleShortcutsAutoRemoval(
    &ntp_features::kNtpFeatureOptimization,
    "EnableStaleShortcutsAutoRemoval",
    false);
const base::FeatureParam<bool> kEnableStaleModulesAutoRemoval(
    &ntp_features::kNtpFeatureOptimization,
    "EnableStaleModulesAutoRemoval",
    false);
const base::FeatureParam<base::TimeDelta> kMinStalenessUpdateTimeInterval(
    &ntp_features::kNtpFeatureOptimization,
    "MinStalenessUpdateTimeInterval",
    base::Days(1));
const base::FeatureParam<int> kStaleShortcutsCountThreshold(
    &ntp_features::kNtpFeatureOptimization,
    "StaleShortcutsCountThreshold",
    60);
const base::FeatureParam<int> kStaleModulesCountThreshold(
    &ntp_features::kNtpFeatureOptimization,
    "StaleModulesCountThreshold",
    14);
const base::FeatureParam<bool> kRemoveDismissModules(
    &ntp_features::kNtpFeatureOptimization,
    "RemoveDismissModules",
    false);

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

int GetWallpaperSearchButtonAnimationShownThreshold() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kNtpWallpaperSearchButtonAnimationShownThreshold,
      kNtpWallpaperSearchButtonAnimationShownThresholdParam, 15);
}

int GetWallpaperSearchButtonHideCondition() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kNtpWallpaperSearchButtonHideCondition,
      kNtpWallpaperSearchButtonHideConditionParam, 2);
}

std::string GetMobilePromoTargetURL() {
  std::string field_trial_url = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMobilePromo,
      ntp_features::kNtpMobilePromoTargetUrlParam);
  return (field_trial_url.empty()) ? kMobilePromoQRCodeURL : field_trial_url;
}

int GetMaxTilesBeforeShowMore() {
  return kMaxTilesBeforeShowMore.Get();
}

}  // namespace ntp_features
