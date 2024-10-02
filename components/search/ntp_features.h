// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_NTP_FEATURES_H_
#define COMPONENTS_SEARCH_NTP_FEATURES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

BASE_DECLARE_FEATURE(kConfirmSuggestionRemovals);
BASE_DECLARE_FEATURE(kCustomizeChromeColorExtraction);
BASE_DECLARE_FEATURE(kCustomizeChromeSidePanelExtensionsCard);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearch);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearchButton);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearchInspirationCard);
BASE_DECLARE_FEATURE(kIframeOneGoogleBar);
BASE_DECLARE_FEATURE(kOneGoogleBarModalOverlays);
BASE_DECLARE_FEATURE(kRealboxCr23Theming);
BASE_DECLARE_FEATURE(kRealboxMatchOmniboxTheme);
BASE_DECLARE_FEATURE(kRealboxMatchSearchboxTheme);
BASE_DECLARE_FEATURE(kRealboxUseGoogleGIcon);
BASE_DECLARE_FEATURE(kNtpAlphaBackgroundCollections);
BASE_DECLARE_FEATURE(kNtpBackgroundImageErrorDetection);
BASE_DECLARE_FEATURE(kNtpCalendarModule);
BASE_DECLARE_FEATURE(kNtpChromeCartModule);
BASE_DECLARE_FEATURE(kNtpDriveModule);
BASE_DECLARE_FEATURE(kNtpDriveModuleSegmentation);
BASE_DECLARE_FEATURE(kNtpDriveModuleShowSixFiles);
#if !defined(OFFICIAL_BUILD)
BASE_DECLARE_FEATURE(kNtpDummyModules);
#endif
BASE_DECLARE_FEATURE(kNtpComprehensiveTheming);
BASE_DECLARE_FEATURE(kNtpLogo);
BASE_DECLARE_FEATURE(kNtpMiddleSlotPromo);
BASE_DECLARE_FEATURE(kNtpMiddleSlotPromoDismissal);
BASE_DECLARE_FEATURE(kNtpModulesLoadTimeoutMilliseconds);
BASE_DECLARE_FEATURE(kNtpWideModules);
BASE_DECLARE_FEATURE(kNtpModulesOrder);
BASE_DECLARE_FEATURE(kNtpModulesOverflowScrollbar);
BASE_DECLARE_FEATURE(kNtpModulesDragAndDrop);
BASE_DECLARE_FEATURE(kNtpModulesLoad);
BASE_DECLARE_FEATURE(kNtpModulesRedesigned);
BASE_DECLARE_FEATURE(kNtpOutlookCalendarModule);
BASE_DECLARE_FEATURE(kNtpPhotosModule);
BASE_DECLARE_FEATURE(kNtpPhotosModuleSoftOptOut);
BASE_DECLARE_FEATURE(kNtpPhotosModuleCustomizedOptInTitle);
BASE_DECLARE_FEATURE(kNtpPhotosModuleCustomizedOptInArtWork);
BASE_DECLARE_FEATURE(kNtpPhotosModuleSplitSvgOptInArtWork);
BASE_DECLARE_FEATURE(kNtpFeedModule);
BASE_DECLARE_FEATURE(kNtpOneGoogleBar);
BASE_DECLARE_FEATURE(kNtpLensDirectUpload);
BASE_DECLARE_FEATURE(kNtpSafeBrowsingModule);
BASE_DECLARE_FEATURE(kNtpSharepointModule);
BASE_DECLARE_FEATURE(kNtpShortcuts);
BASE_DECLARE_FEATURE(kNtpHandleMostVisitedNavigationExplicitly);
BASE_DECLARE_FEATURE(kNtpModulesHeaderIcon);
BASE_DECLARE_FEATURE(kNtpMostRelevantTabResumptionModule);
BASE_DECLARE_FEATURE(kNtpMostRelevantTabResumptionModuleDeviceIcon);
BASE_DECLARE_FEATURE(kNtpTabResumptionModuleCategories);
BASE_DECLARE_FEATURE(kNtpTabResumptionModuleTimeLimit);
BASE_DECLARE_FEATURE(kNtpWallpaperSearchButton);
BASE_DECLARE_FEATURE(kNtpWallpaperSearchButtonAnimation);
BASE_DECLARE_FEATURE(kNtpWallpaperSearchButtonAnimationShownThreshold);
BASE_DECLARE_FEATURE(kNtpMobilePromo);

// Parameter for controlling the luminosity difference for NTP elements on light
// backgrounds.
extern const base::FeatureParam<double>
    kNtpElementLuminosityChangeForLightBackgroundParam;

// Parameter for controlling the luminosity difference for NTP elements on dark
// backgrounds.
extern const base::FeatureParam<double>
    kNtpElementLuminosityChangeForDarkBackgroundParam;

// Parameter determining the ignore based survey launch delay time.
extern const char kNtpModuleIgnoredHaTSDelayTimeParam[];
// Parameter determining the number of times a module must have loaded with no
// interaction by the user before it's considered as ignored.
extern const char kNtpModuleIgnoredCriteriaThreshold[];
// Parameter determining the module load timeout.
extern const char kNtpModulesLoadTimeoutMillisecondsParam[];
// Parameter determining the module order.
extern const char kNtpModulesOrderParam[];
// Parameter determining the type of calendar data used to render module.
extern const char kNtpCalendarModuleDataParam[];
// Parameter determining the type of cart data used to render module.
extern const char kNtpChromeCartModuleDataParam[];
// Parameter for enabling the abandoned cart discount.
extern const char kNtpChromeCartModuleAbandonedCartDiscountParam[];
// Parameter for enabling the abandoned cart discount with utm_source tag to
// indicate the feature state.
extern const char kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam[];
// Parameter for enabling the cart heuristics improvement.
extern const char kNtpChromeCartModuleHeuristicsImprovementParam[];
// Parameter for enabling coupons on the Cart module.
extern const char kNtpChromeCartModuleCouponParam[];
// Parameter determining the type of Drive data to render.
extern const char kNtpDriveModuleDataParam[];
// Parameter for enabling the Drive module for managed users only.
extern const char kNtpDriveModuleManagedUsersOnlyParam[];
// Parameter determining the max age in seconds of the cache for drive data.
extern const char kNtpDriveModuleCacheMaxAgeSParam[];
// Parameter for communicating the experiment group of the Drive module
// experiment.
extern const char kNtpDriveModuleExperimentGroupParam[];
// Parameter determining the type of calendar data to render.
extern const char kNtpOutlookCalendarModuleDataParam[];
// Parameter determining the type of middle slot promo data to render.
extern const char kNtpMiddleSlotPromoDismissalParam[];
// Parameter determining the modules that are eligigle for HaTS.
extern const char kNtpModulesEligibleForHappinessTrackingSurveyParam[];
// Parameter determining module trigger ids for HaTS for eligible module ids for
// a given module interaction type.
extern const char kNtpModulesInteractionBasedSurveyEligibleIdsParam[];
// Parameter determining the type of Photos data to render.
extern const char kNtpPhotosModuleDataParam[];
// Parameter determining the art work in opt-in card.
extern const char kNtpPhotosModuleOptInArtWorkParam[];
// Parameter determining the title for the opt-in card.
extern const char kNtpPhotosModuleOptInTitleParam[];
// Parameter determining the number of times a module is shown to a user
// before cooldown starts.
extern const char kNtpSafeBrowsingModuleCountMaxParam[];
// Parameter determining the cooldown period (in days) for a target user.
extern const char kNtpSafeBrowsingModuleCooldownPeriodDaysParam[];
// Parameter determining the variation of the omnibox theme matching.
extern const char kRealboxMatchOmniboxThemeVariantParam[];
extern const char kNtpMostRelevantTabResumptionModuleDataParam[];
// Parameter determining the max visits to show.
extern const char kNtpMostRelevantTabResumptionModuleMaxVisitsParam[];
extern const char kNtpRealboxWidthBehaviorParam[];
// Parameter for determining the categories a tab must not fall into
// to be shown.
extern const char kNtpTabResumptionModuleCategoriesBlocklistParam[];
extern const char kNtpTabResumptionModuleDataParam[];
// Parameter determining for how long a dismissed tab should be discarded
// from the module's displayed visit resumption suggestions.
extern const char kNtpTabResumptionModuleDismissalDurationParam[];
// Parameter determining what types result types to request when fetching URL
// visit aggregate data.
extern const char kNtpTabResumptionModuleResultTypesParam[];
// Parameter determining the recency of tabs in the Tab Resumption module.
extern const char kNtpTabResumptionModuleTimeLimitParam[];
extern const char kNtpTabResumptionModuleVisibilityThresholdDataParam[];
// Parameter determining the number of times to animate the NTP Wallpaper Search
// button.
extern const char kNtpWallpaperSearchButtonAnimationShownThresholdParam[];
// Parameter determining what condition to use to hide the wallpaper search
// button.
extern const char kNtpWallpaperSearchButtonHideConditionParam[];
// Parameter determining the trigger delay of the Wallpaper Search HaTS survey.
extern const char kWallpaperSearchHatsDelayParam[];

// Parameter determining the experiment name to pass to the Google Calendar
// API.
extern const base::FeatureParam<std::string> kNtpCalendarModuleExperimentParam;
// Parameter determining the number of events to show on the calendar module.
extern const base::FeatureParam<int> kNtpCalendarModuleMaxEventsParam;
// Parameter determining the time delta from now for the end of the event
// window.
extern const base::FeatureParam<base::TimeDelta>
    kNtpCalendarModuleWindowEndDeltaParam;
// Parameter determining the time delta from now for the start of the event
// window.
extern const base::FeatureParam<base::TimeDelta>
    kNtpCalendarModuleWindowStartDeltaParam;
// Parameter determining the background color of the expanded state realbox.
extern const base::FeatureParam<bool>
    kNtpRealboxCr23ExpandedStateBgMatchesOmnibox;
// Parameter determining the whether the steady state realbox has a shadow.
extern const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow;
// Parameter determining the impression limit for the NTP mobile promo. The
// promo will not be shown again after the impression limit is reached.
extern const base::FeatureParam<int> kNtpMobilePromoImpressionLimit;

// Returns the timeout after which the load of a module should be aborted.
base::TimeDelta GetModulesLoadTimeout();

// Returns the maximum number of columns to show on the redesigned modules UI
// experience.
int GetModulesMaxColumnCount();

// Returns the maximum number of instances to render for a given module when the
// module has loaded with other modules. A sentinel value of -1 implies there is
// no limit.
int GetMultipleLoadedModulesMaxModuleInstanceCount();

// Returns a list of module IDs ordered by how they should appear on the NTP.
std::vector<std::string> GetModulesOrder();

// Returns the maximum number of times to show animation for NTP wallpaper
// search button.
int GetWallpaperSearchButtonAnimationShownThreshold();

// Returns the condition to use to hide the wallpaper search button.
int GetWallpaperSearchButtonHideCondition();
}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
