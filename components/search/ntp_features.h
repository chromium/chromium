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
BASE_DECLARE_FEATURE(kCacheOneGoogleBar);
BASE_DECLARE_FEATURE(kCustomizeChromeColorExtraction);
BASE_DECLARE_FEATURE(kCustomizeChromeSidePanelExtensionsCard);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearch);
BASE_DECLARE_FEATURE(kCwsDarkLogo);
BASE_DECLARE_FEATURE(kDismissPromos);
BASE_DECLARE_FEATURE(kIframeOneGoogleBar);
BASE_DECLARE_FEATURE(kOneGoogleBarModalOverlays);
BASE_DECLARE_FEATURE(kRealboxMatchOmniboxTheme);
BASE_DECLARE_FEATURE(kRealboxMatchSearchboxTheme);
BASE_DECLARE_FEATURE(kRealboxUseGoogleGIcon);
BASE_DECLARE_FEATURE(kRealboxWidthBehavior);
BASE_DECLARE_FEATURE(kRealboxIsTall);
BASE_DECLARE_FEATURE(kNtpAlphaBackgroundCollections);
BASE_DECLARE_FEATURE(kNtpBackgroundImageErrorDetection);
BASE_DECLARE_FEATURE(kNtpChromeCartModule);
BASE_DECLARE_FEATURE(kNtpDriveModule);
BASE_DECLARE_FEATURE(kNtpDriveModuleSegmentation);
BASE_DECLARE_FEATURE(kNtpDriveModuleShowSixFiles);
#if !defined(OFFICIAL_BUILD)
BASE_DECLARE_FEATURE(kNtpDummyModules);
#endif
BASE_DECLARE_FEATURE(kNtpComprehensiveTheming);
BASE_DECLARE_FEATURE(kNtpComprehensiveThemeRealbox);
BASE_DECLARE_FEATURE(kNtpLogo);
BASE_DECLARE_FEATURE(kNtpReducedLogoSpace);
BASE_DECLARE_FEATURE(kNtpMiddleSlotPromo);
BASE_DECLARE_FEATURE(kNtpMiddleSlotPromoDismissal);
BASE_DECLARE_FEATURE(kNtpModulesLoadTimeoutMilliseconds);
BASE_DECLARE_FEATURE(kNtpWideModules);
BASE_DECLARE_FEATURE(kNtpModulesOrder);
BASE_DECLARE_FEATURE(kNtpModulesOverflowScrollbar);
BASE_DECLARE_FEATURE(kNtpModulesDragAndDrop);
BASE_DECLARE_FEATURE(kNtpModulesFirstRunExperience);
BASE_DECLARE_FEATURE(kNtpModulesLoad);
BASE_DECLARE_FEATURE(kNtpModulesRedesigned);
BASE_DECLARE_FEATURE(kNtpMostVisitedReflowOnOverflow);
BASE_DECLARE_FEATURE(kNtpPhotosModule);
BASE_DECLARE_FEATURE(kNtpPhotosModuleSoftOptOut);
BASE_DECLARE_FEATURE(kNtpPhotosModuleCustomizedOptInTitle);
BASE_DECLARE_FEATURE(kNtpPhotosModuleCustomizedOptInArtWork);
BASE_DECLARE_FEATURE(kNtpPhotosModuleSplitSvgOptInArtWork);
BASE_DECLARE_FEATURE(kNtpFeedModule);
BASE_DECLARE_FEATURE(kNtpOneGoogleBar);
BASE_DECLARE_FEATURE(kNtpRealboxLensSearch);
BASE_DECLARE_FEATURE(kNtpLensDirectUpload);
BASE_DECLARE_FEATURE(kNtpRecipeTasksModule);
BASE_DECLARE_FEATURE(kNtpSafeBrowsingModule);
BASE_DECLARE_FEATURE(kNtpShortcuts);
BASE_DECLARE_FEATURE(kNtpSingleRowShortcuts);
BASE_DECLARE_FEATURE(kNtpHandleMostVisitedNavigationExplicitly);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModule);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleBeginTimeDuration);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleMinimumVisitsRequired);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleMinimumImagesRequired);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleCategories);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleLoad);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleMaxClusters);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleSuggestionChipHeader);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleTextOnly);
BASE_DECLARE_FEATURE(kNtpModulesHeaderIcon);
BASE_DECLARE_FEATURE(kNtpChromeCartInHistoryClusterModule);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleUseModelRanking);
BASE_DECLARE_FEATURE(kNtpChromeCartHistoryClusterCoexist);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleFetchClustersUntilExhausted);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleIncludeSyncedVisits);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleEnableContentClustering);
BASE_DECLARE_FEATURE(kNtpHistoryClustersModuleDiscounts);

// Parameter for controlling the luminosity difference for NTP elements on light
// backgrounds.
extern const base::FeatureParam<double>
    kNtpElementLuminosityChangeForLightBackgroundParam;

// Parameter for controlling the luminosity difference for NTP elements on dark
// backgrounds.
extern const base::FeatureParam<double>
    kNtpElementLuminosityChangeForDarkBackgroundParam;

// Parameter determining the module load timeout.
extern const char kNtpModulesLoadTimeoutMillisecondsParam[];
// Parameter determining the module order.
extern const char kNtpModulesOrderParam[];
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
// Parameter determining the fake data to request to surface a particular
// visual layout.
extern const char kNtpHistoryClustersModuleDataParam[];
// Parameter determining the fake data to request to surface a particular
// cart tile layout in history cluster module.
extern const char kNtpChromeCartInHistoryClustersModuleDataParam[];
// Parameter determining the type of middle slot promo data to render.
extern const char kNtpMiddleSlotPromoDismissalParam[];
// Parameter determining the modules that are eligigle for HATS.
extern const char kNtpModulesEligibleForHappinessTrackingSurveyParam[];
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
// Parameter determining the type of recipe data to request.
extern const char kNtpRecipeTasksModuleDataParam[];
// Parameter determining the max age in seconds of the cache for recipe tasks
// data.
extern const char kNtpRecipeTasksModuleCacheMaxAgeSParam[];
// Parameter for communicating the experiment group of the recipe tasks module
// experiment.
extern const char kNtpRecipeTasksModuleExperimentGroupParam[];
// Parameter for determining the maximum number of hours to look back to show a
// history cluster.
extern const char kNtpHistoryClustersModuleBeginTimeDurationHoursParam[];
// Parameter for determining the minimum number of visits that are required in
// order to show a history cluster.
extern const char kNtpHistoryClustersModuleMinimumVisitsRequiredParam[];
// Parameter for determining the minimum number of visits with an image that are
// required in order to show a history cluster.
extern const char kNtpHistoryClustersModuleMinimumImagesRequiredParam[];
// Parameter for determining the categories a history cluster must fall into to
// be shown.
extern const char kNtpHistoryClustersModuleCategoriesAllowlistParam[];
// Parameter for determining the categories a history cluster must not fall into
// to be shown.
extern const char kNtpHistoryClustersModuleCategoriesBlocklistParam[];
// Parameter for determining the categories a history cluster falls into to be
// boosted for showing on the New Tab Page.
extern const char kNtpHistoryClustersModuleCategoriesBoostlistParam[];
// Parameter for setting the maximum number of candidate clusters for the
// History Clusters Service to return.
extern const char kNtpHistoryClustersModuleMaxClustersParam[];
extern const char kNtpRealboxWidthBehaviorParam[];

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
}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
