// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace ntp_features {

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
const base::Feature kConfirmSuggestionRemovals{
    "ConfirmNtpSuggestionRemovals", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the OneGooleBar cached response is sent back to NTP.
const base::Feature kCacheOneGoogleBar{"CacheOneGoogleBar",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissPromos{"DismissNtpPromos",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, queries that are frequently repeated by the user (and are
// expected to be issued again) are shown as most visited tiles.
const base::Feature kNtpRepeatableQueries{"NtpRepeatableQueries",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Depends on kRealbox being enabled. If enabled, the NTP "realbox" will be
// themed like the omnibox (same background/text/selected/hover colors).
const base::Feature kRealboxMatchOmniboxTheme{
    "NtpRealboxMatchOmniboxTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
const base::Feature kRealboxUseGoogleGIcon{"NtpRealboxUseGoogleGIcon",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shows Vasco suggestion chips in the NTP below fakebox/realbox
// despite other config except DisableSearchSuggestChips below.
const base::Feature kSearchSuggestChips{"SearchSuggestChips",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, hides Vasco suggestion chips in the NTP below fakebox/realbox
// despite other config.
const base::Feature kDisableSearchSuggestChips{
    "DisableSearchSuggestChips", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, handles navigations from the Most Visited tiles explicitly and
// overrides the navigation's transition type to bookmark navigation before the
// navigation is issued.
// TODO(crbug.com/1147589): When removing this flag, also remove the workaround
// in ChromeContentBrowserClient::OverrideNavigationParams.
extern const base::Feature kNtpHandleMostVisitedNavigationExplicitly{
    "HandleMostVisitedNavigationExplicitly", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, logo will be shown.
const base::Feature kNtpLogo{"NtpLogo", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, shortcuts will be shown.
const base::Feature kNtpShortcuts{"NtpShortcuts",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, middle slot promo will be shown.
const base::Feature kNtpMiddleSlotPromo{"NtpMiddleSlotPromo",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, modules will be shown.
const base::Feature kModules{"NtpModules", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, modules will be loaded even if kModules is disabled. This is
// useful to determine if a user would have seen modules in order to
// counterfactually log or trigger.
const base::Feature kNtpModulesLoad{"NtpModulesLoad",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, recipe tasks module will be shown.
const base::Feature kNtpRecipeTasksModule{"NtpRecipeTasksModule",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shopping tasks module will be shown.
const base::Feature kNtpShoppingTasksModule{"NtpShoppingTasksModule",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, chrome cart module will be shown.
const base::Feature kNtpChromeCartModule{"NtpChromeCartModule",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, redesigned modules will be shown.
const base::Feature kNtpModulesRedesigned{"NtpModulesRedesigned",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Google Drive module will be shown.
const base::Feature kNtpDriveModule{"NtpDriveModule",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Google Photos module will be shown.
const base::Feature kNtpPhotosModule{"NtpPhotosModule",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, modules will be able to be reordered via dragging and dropping
const base::Feature kNtpModulesDragAndDrop{"NtpModulesDragAndDrop",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const char kNtpModulesLoadTimeoutMillisecondsParam[] =
    "NtpModulesLoadTimeoutMillisecondsParam";
const char kNtpShoppingTasksModuleDataParam[] =
    "NtpShoppingTasksModuleDataParam";
const char kNtpRecipeTasksModuleDataParam[] = "NtpRecipeTasksModuleDataParam";
const char kNtpShoppingTasksModuleCacheMaxAgeSParam[] =
    "NtpShoppingTasksModuleCacheMaxAgeSParam";
const char kNtpRecipeTasksModuleCacheMaxAgeSParam[] =
    "NtpRecipeTasksModuleCacheMaxAgeSParam";
const char kNtpChromeCartModuleDataParam[] = "NtpChromeCartModuleDataParam";
const char kNtpChromeCartModuleAbandonedCartDiscountParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountParam";
const char NtpChromeCartModuleAbandonedCartDiscountUseUtmParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountUseUtmParam";
const char kNtpChromeCartModuleHeuristicsImprovementParam[] =
    "NtpChromeCartModuleHeuristicsImprovementParam";
const char kNtpDriveModuleDataParam[] = "NtpDriveModuleDataParam";
const char kNtpDriveModuleManagedUsersOnlyParam[] =
    "NtpDriveModuleManagedUsersOnlyParam";
const char kNtpDriveModuleCacheMaxAgeSParam[] =
    "NtpDriveModuleCacheMaxAgeSParam";
const char kNtpDriveModuleExperimentGroupParam[] =
    "NtpDriveModuleExperimentGroupParam";

base::TimeDelta GetModulesLoadTimeout() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kModules, kNtpModulesLoadTimeoutMillisecondsParam);
  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  unsigned int param_value_as_int = 0;
  if (!base::StringToUint(param_value, &param_value_as_int)) {
    return base::TimeDelta::FromSeconds(3);
  }
  return base::TimeDelta::FromMilliseconds(param_value_as_int);
}

}  // namespace ntp_features
