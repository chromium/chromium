// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_NTP_FEATURES_H_
#define COMPONENTS_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace base {
class Time;
}  // namespace base

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kConfirmSuggestionRemovals;
extern const base::Feature kCacheOneGoogleBar;
extern const base::Feature kDismissPromos;
extern const base::Feature kIframeOneGoogleBar;
extern const base::Feature kNtpRepeatableQueries;
extern const base::Feature kOneGoogleBarModalOverlays;
extern const base::Feature kRealboxMatchOmniboxTheme;
extern const base::Feature kRealboxUseGoogleGIcon;
extern const base::Feature kWebUI;
extern const base::Feature kNtpLogo;
extern const base::Feature kNtpShortcuts;
extern const base::Feature kNtpMiddleSlotPromo;
extern const base::Feature kModules;
extern const base::Feature kNtpRecipeTasksModule;
extern const base::Feature kNtpShoppingTasksModule;
extern const base::Feature kNtpChromeCartModule;
extern const base::Feature kNtpDriveModule;
extern const base::Feature kSearchSuggestChips;
extern const base::Feature kDisableSearchSuggestChips;

extern const base::Feature kNtpHandleMostVisitedNavigationExplicitly;

// Parameter name determining the age threshold in days for local history
// repeatable queries.
// The value of this parameter should be parsable as an unsigned integer.
extern const char kNtpRepeatableQueriesAgeThresholdDaysParam[];
// Parameter name determining the number of seconds until the recency component
// of the frecency score for local history repeatable queries decays to half.
// The value of this parameter should be parsable as an unsigned integer.
extern const char kNtpRepeatableQueriesRecencyHalfLifeSecondsParam[];
// Parameter name determining the factor by which the frequency component of the
// frecency score for local history repeatable queries is exponentiated.
// The value of this parameter should be parsable as a double.
extern const char kNtpRepeatableQueriesFrequencyExponentParam[];
// Parameter name determining the position, with respect to the MV tiles, in
// which the repeatable queries should be inserted.
extern const char kNtpRepeatableQueriesInsertPositionParam[];
// The available positions, with respect to the MV tiles, in which the
// repeatable queries can be inserted.
enum class RepeatableQueriesInsertPosition {
  kStart = 0,  // At the start of MV tiles.
  kEnd,        // At the end of MV tiles.
};

// Parameter determining the module load timeout.
extern const char kNtpModulesLoadTimeoutMillisecondsParam[];
// Parameter determining the type of stateful data to request.
extern const char kNtpStatefulTasksModuleDataParam[];
// Parameter determining the type of cart data used to render module.
extern const char kNtpChromeCartModuleDataParam[];

// Returns the age threshold for local history repeatable queries.
base::Time GetLocalHistoryRepeatableQueriesAgeThreshold();
// Returns the number of seconds until the recency component of the frecency
// score for local history repeatable queries decays to half.
int GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds();
// Returns the factor by which the frequency component of the frecency score for
// local history repeatable queries is exponentiated.
double GetLocalHistoryRepeatableQueriesFrequencyExponent();
// Returns the position, with respect to the MV tiles, in which the repeatable
// queries should be inserted.
RepeatableQueriesInsertPosition GetRepeatableQueriesInsertPosition();

// Returns the timeout after which the load of a module should be aborted.
base::TimeDelta GetModulesLoadTimeout();
}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
