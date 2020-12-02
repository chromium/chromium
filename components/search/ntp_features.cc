// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace ntp_features {

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
const base::Feature kConfirmSuggestionRemovals{
    "ConfirmNtpSuggestionRemovals", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissPromos{"DismissNtpPromos",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the OneGooleBar is loaded in an iframe. Otherwise, it is inlined.
const base::Feature kIframeOneGoogleBar{"IframeOneGoogleBar",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, queries that are frequently repeated by the user (and are
// expected to be issued again) are shown as most visited tiles.
const base::Feature kNtpRepeatableQueries{"NtpRepeatableQueries",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the iframed OneGooleBar shows the overlays modally with a
// backdrop.
const base::Feature kOneGoogleBarModalOverlays{
    "OneGoogleBarModalOverlays", base::FEATURE_DISABLED_BY_DEFAULT};

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

// If enabled, the WebUI new tab page will load when a new tab is created
// instead of the local NTP.
const base::Feature kWebUI{"NtpWebUI", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the Doodle will be shown on themed and dark mode NTPs.
const base::Feature kWebUIThemeModeDoodles{"WebUIThemeModeDoodles",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, modules will be shown.
const base::Feature kModules{"NtpModules", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, recipe tasks module will be shown.
const base::Feature kNtpRecipeTasksModule{"NtpRecipeTasksModule",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shopping tasks module will be shown.
const base::Feature kNtpShoppingTasksModule{"NtpShoppingTasksModule",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const char kNtpRepeatableQueriesAgeThresholdDaysParam[] =
    "NtpRepeatableQueriesAgeThresholdDays";
const char kNtpRepeatableQueriesRecencyHalfLifeSecondsParam[] =
    "NtpRepeatableQueriesRecencyHalfLifeSeconds";
const char kNtpRepeatableQueriesFrequencyExponentParam[] =
    "NtpRepeatableQueriesFrequencyExponent";
const char kNtpRepeatableQueriesInsertPositionParam[] =
    "NtpRepeatableQueriesInsertPosition";

const char kNtpStatefulTasksModuleDataParam[] =
    "NtpStatefulTasksModuleDataParam";

base::Time GetLocalHistoryRepeatableQueriesAgeThreshold() {
  const base::TimeDelta kLocalHistoryRepeatableQueriesAgeThreshold =
      base::TimeDelta::FromDays(180);  // Six months.
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kNtpRepeatableQueries, kNtpRepeatableQueriesAgeThresholdDaysParam);

  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  unsigned int param_value_as_int = 0;
  if (!base::StringToUint(param_value, &param_value_as_int)) {
    return base::Time::Now() - kLocalHistoryRepeatableQueriesAgeThreshold;
  }

  return (base::Time::Now() - base::TimeDelta::FromDays(param_value_as_int));
}

int GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds() {
  const base::TimeDelta kLocalHistoryRepeatableQueriesRecencyHalfLife =
      base::TimeDelta::FromDays(7);  // One week.
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kNtpRepeatableQueries, kNtpRepeatableQueriesRecencyHalfLifeSecondsParam);

  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  unsigned int param_value_as_int = 0;
  if (!base::StringToUint(param_value, &param_value_as_int)) {
    return kLocalHistoryRepeatableQueriesRecencyHalfLife.InSeconds();
  }

  return param_value_as_int;
}

double GetLocalHistoryRepeatableQueriesFrequencyExponent() {
  const double kLocalHistoryRepeatableQueriesFrequencyExponent = 2.0;
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kNtpRepeatableQueries, kNtpRepeatableQueriesFrequencyExponentParam);

  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  double param_value_as_double = 0;
  if (!base::StringToDouble(param_value, &param_value_as_double)) {
    return kLocalHistoryRepeatableQueriesFrequencyExponent;
  }

  return param_value_as_double;
}

RepeatableQueriesInsertPosition GetRepeatableQueriesInsertPosition() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kNtpRepeatableQueries, kNtpRepeatableQueriesInsertPositionParam);
  return param_value == "end" ? RepeatableQueriesInsertPosition::kEnd
                              : RepeatableQueriesInsertPosition::kStart;
}

}  // namespace ntp_features
