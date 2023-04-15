// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/register_prefs.h"

#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets::prefs {

void RegisterProfilePrefsForMigrationApril2023(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedCategories);
  registry->RegisterDoublePref(kUserClassifierAverageNTPOpenedPerHour, 0.0);
  registry->RegisterDoublePref(kUserClassifierAverageSuggestionsShownPerHour,
                               0.0);
  registry->RegisterDoublePref(kUserClassifierAverageSuggestionsUsedPerHour,
                               0.0);
  registry->RegisterInt64Pref(kUserClassifierLastTimeToOpenNTP, 0);
  registry->RegisterInt64Pref(kUserClassifierLastTimeToShowSuggestions, 0);
  registry->RegisterInt64Pref(kUserClassifierLastTimeToUseSuggestions, 0);
  registry->RegisterIntegerPref(kSnippetFetcherInteractiveRequestCount, 0);
  registry->RegisterIntegerPref(kSnippetFetcherRequestCount, 0);
  registry->RegisterIntegerPref(kSnippetFetcherRequestsDay, 0);
  registry->RegisterIntegerPref(kSnippetThumbnailsInteractiveRequestCount, 0);
  registry->RegisterIntegerPref(kSnippetThumbnailsRequestCount, 0);
  registry->RegisterIntegerPref(kSnippetThumbnailsRequestsDay, 0);
  registry->RegisterTimeDeltaPref(kSnippetPersistentFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(kSnippetPersistentFetchingIntervalFallback,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(kSnippetStartupFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(kSnippetStartupFetchingIntervalFallback,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(kSnippetShownFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(kSnippetShownFetchingIntervalFallback,
                                  base::TimeDelta());
  registry->RegisterTimePref(kSnippetLastFetchAttemptTime, base::Time());
  registry->RegisterTimePref(kSnippetLastSuccessfulFetchTime, base::Time());

  registry->RegisterListPref(kRemoteSuggestionCategories);
  registry->RegisterInt64Pref(kLastSuccessfulBackgroundFetchTime, 0);

  registry->RegisterListPref(kClickBasedCategoryRankerOrderWithClicks);
  registry->RegisterInt64Pref(kClickBasedCategoryRankerLastDecayTime,
                              /*default_value=*/0);
}

void MigrateObsoleteProfilePrefsApril2023(PrefService* prefs) {
  prefs->ClearPref(kDismissedCategories);
  prefs->ClearPref(kUserClassifierAverageNTPOpenedPerHour);
  prefs->ClearPref(kUserClassifierAverageSuggestionsShownPerHour);
  prefs->ClearPref(kUserClassifierAverageSuggestionsUsedPerHour);
  prefs->ClearPref(kUserClassifierLastTimeToOpenNTP);
  prefs->ClearPref(kUserClassifierLastTimeToShowSuggestions);
  prefs->ClearPref(kUserClassifierLastTimeToUseSuggestions);
  prefs->ClearPref(kSnippetFetcherInteractiveRequestCount);
  prefs->ClearPref(kSnippetFetcherRequestCount);
  prefs->ClearPref(kSnippetFetcherRequestsDay);
  prefs->ClearPref(kSnippetThumbnailsInteractiveRequestCount);
  prefs->ClearPref(kSnippetThumbnailsRequestCount);
  prefs->ClearPref(kSnippetThumbnailsRequestsDay);
  prefs->ClearPref(kSnippetPersistentFetchingIntervalWifi);
  prefs->ClearPref(kSnippetPersistentFetchingIntervalFallback);
  prefs->ClearPref(kSnippetStartupFetchingIntervalWifi);
  prefs->ClearPref(kSnippetStartupFetchingIntervalFallback);
  prefs->ClearPref(kSnippetShownFetchingIntervalWifi);
  prefs->ClearPref(kSnippetShownFetchingIntervalFallback);
  prefs->ClearPref(kSnippetLastFetchAttemptTime);
  prefs->ClearPref(kSnippetLastSuccessfulFetchTime);
  prefs->ClearPref(kRemoteSuggestionCategories);
  prefs->ClearPref(kLastSuccessfulBackgroundFetchTime);
  prefs->ClearPref(kClickBasedCategoryRankerOrderWithClicks);
  prefs->ClearPref(kClickBasedCategoryRankerLastDecayTime);
}

}  // namespace ntp_snippets::prefs
