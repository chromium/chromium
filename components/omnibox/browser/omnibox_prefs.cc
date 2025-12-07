// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"

#include <optional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace omnibox {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      kKeywordSpaceTriggeringEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kShowGoogleLensShortcut, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kShowAiModeOmniboxButton, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kShowSearchTools, true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(omnibox::kDismissedGeminiIph, false);
  registry->RegisterBooleanPref(
      omnibox::kDismissedEnterpriseSearchAggregatorIphPrefName, false);
  registry->RegisterBooleanPref(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName, false);
  registry->RegisterBooleanPref(
      omnibox::kDismissedHistoryEmbeddingsSettingsPromo, false);
  registry->RegisterBooleanPref(omnibox::kDismissedHistoryScopePromo, false);
  registry->RegisterBooleanPref(omnibox::kDismissedHistoryEmbeddingsScopePromo,
                                false);
  registry->RegisterBooleanPref(kBottomOmniboxEverUsed, false);

  registry->RegisterIntegerPref(kShownCountGeminiIph, 0);
  registry->RegisterIntegerPref(kShownCountEnterpriseSearchAggregatorIph, 0);
  registry->RegisterIntegerPref(kShownCountFeaturedEnterpriseSiteSearchIph, 0);
  registry->RegisterIntegerPref(kShownCountHistoryEmbeddingsSettingsPromo, 0);
  registry->RegisterIntegerPref(kShownCountHistoryScopePromo, 0);
  registry->RegisterIntegerPref(kShownCountHistoryEmbeddingsScopePromo, 0);
  registry->RegisterIntegerPref(kFocusedSrpWebCount, 0);

  registry->RegisterIntegerPref(kAimHintLastImpressionDay, 0);
  registry->RegisterIntegerPref(kAimHintDailyImpressionsCount, 0);
  registry->RegisterIntegerPref(kAimHintTotalImpressions, 0);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kIsOmniboxInBottomPosition, false);
}

void SetUserPreferenceForZeroSuggestCachedResponse(
    PrefService* prefs,
    const std::string& page_url,
    const std::string& response) {
  DCHECK(prefs);

  if (page_url.empty()) {
    prefs->SetString(kZeroSuggestCachedResults, response);
  } else {
    // Constrain the cache to a single entry by overwriting the existing value.
    base::Value::Dict new_dict;
    new_dict.Set(page_url, response);
    prefs->SetDict(kZeroSuggestCachedResultsWithURL, std::move(new_dict));
  }
}

std::string GetUserPreferenceForZeroSuggestCachedResponse(
    PrefService* prefs,
    const std::string& page_url) {
  DCHECK(prefs);

  if (page_url.empty()) {
    return prefs->GetString(omnibox::kZeroSuggestCachedResults);
  }

  const base::Value::Dict& dictionary =
      prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL);
  auto* value_ptr = dictionary.FindString(page_url);
  return value_ptr ? *value_ptr : std::string();
}

}  // namespace omnibox
