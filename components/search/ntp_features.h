// Copyright 2018 The Chromium Authors. All rights reserved.
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

extern const base::Feature kConfirmSuggestionRemovals;
extern const base::Feature kCacheOneGoogleBar;
extern const base::Feature kCwsScrimRemoval;
extern const base::Feature kDismissPromos;
extern const base::Feature kIframeOneGoogleBar;
extern const base::Feature kOneGoogleBarModalOverlays;
extern const base::Feature kRealboxMatchOmniboxTheme;
extern const base::Feature kRealboxMatchSearchboxTheme;
extern const base::Feature kRealboxUseGoogleGIcon;
extern const base::Feature kNtpChromeCartModule;
extern const base::Feature kNtpDriveModule;
#if !defined(OFFICIAL_BUILD)
extern const base::Feature kNtpDummyModules;
#endif
extern const base::Feature kNtpLogo;
extern const base::Feature kNtpMiddleSlotPromo;
extern const base::Feature kNtpMiddleSlotPromoDismissal;
extern const base::Feature kNtpModulesLoadTimeoutMilliseconds;
extern const base::Feature kNtpModulesOrder;
extern const base::Feature kNtpModulesDragAndDrop;
extern const base::Feature kNtpModulesFirstRunExperience;
extern const base::Feature kNtpModulesLoad;
extern const base::Feature kNtpModulesRedesigned;
extern const base::Feature kNtpModulesRedesignedLayout;
extern const base::Feature kNtpPhotosModule;
extern const base::Feature kNtpPhotosModuleSoftOptOut;
extern const base::Feature kNtpPhotosModuleCustomizedOptInTitle;
extern const base::Feature kNtpPhotosModuleCustomizedOptInArtWork;
extern const base::Feature kNtpPhotosModuleSplitSvgOptInArtWork;
extern const base::Feature kNtpFeedModule;
extern const base::Feature kNtpOneGoogleBar;
extern const base::Feature kNtpRecipeTasksModule;
extern const base::Feature kNtpSafeBrowsingModule;
extern const base::Feature kNtpShortcuts;

extern const base::Feature kNtpHandleMostVisitedNavigationExplicitly;

// Parameter determining the module load timeout.
extern const char kNtpModulesLoadTimeoutMillisecondsParam[];
// Parameter determining the module order.
extern const char kNtpModulesOrderParam[];
// Parameter determining the type of recipe data to request.
extern const char kNtpRecipeTasksModuleDataParam[];
// Parameter determining the max age in seconds of the cache for recipe tasks
// data.
extern const char kNtpRecipeTasksModuleCacheMaxAgeSParam[];
// Parameter for communicating the experiment group of the recipe tasks module
// experiment.
extern const char kNtpRecipeTasksModuleExperimentGroupParam[];
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
// Parameter determining the variations of searchbox theme matching.
extern const char kRealboxMatchSearchboxThemeParam[];

// Returns the timeout after which the load of a module should be aborted.
base::TimeDelta GetModulesLoadTimeout();

// Returns a list of module IDs ordered by how they should appear on the NTP.
std::vector<std::string> GetModulesOrder();
}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
