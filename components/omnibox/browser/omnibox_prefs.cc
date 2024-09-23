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
namespace {

// Returns an equivalent omnibox::UMAGroupId value for omnibox::GroupId.
constexpr UMAGroupId ToUMAGroupId(GroupId group_id) {
  switch (group_id) {
    case GROUP_INVALID:
      return UMAGroupId::kInvalid;
    case GROUP_PREVIOUS_SEARCH_RELATED:
      return UMAGroupId::kPreviousSearchRelated;
    case GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS:
      return UMAGroupId::kPreviousSearchRelatedEntityChips;
    case GROUP_TRENDS:
      return UMAGroupId::kTrends;
    case GROUP_TRENDS_ENTITY_CHIPS:
      return UMAGroupId::kTrendsEntityChips;
    case GROUP_RELATED_QUERIES:
      return UMAGroupId::kRelatedQueries;
    case GROUP_VISITED_DOC_RELATED:
      return UMAGroupId::kVisitedDocRelated;
    default:
      return UMAGroupId::kUnknown;
  }
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSuggestionGroupVisibility);
  registry->RegisterBooleanPref(
      kKeywordSpaceTriggeringEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kShowGoogleLensShortcut, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(omnibox::kDismissedGeminiIph, false);
  registry->RegisterBooleanPref(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName, false);
  registry->RegisterBooleanPref(
      omnibox::kDismissedHistoryEmbeddingsSettingsPromo, false);
  registry->RegisterBooleanPref(omnibox::kDismissedHistoryScopePromo, false);
  registry->RegisterBooleanPref(omnibox::kDismissedHistoryEmbeddingsScopePromo,
                                false);

  registry->RegisterIntegerPref(kShownCountGeminiIph, 0);
  registry->RegisterIntegerPref(kShownCountFeaturedEnterpriseSiteSearchIph, 0);
  registry->RegisterIntegerPref(kShownCountHistoryEmbeddingsSettingsPromo, 0);
  registry->RegisterIntegerPref(kShownCountHistoryScopePromo, 0);
  registry->RegisterIntegerPref(kShownCountHistoryEmbeddingsScopePromo, 0);
}

SuggestionGroupVisibility GetUserPreferenceForSuggestionGroupVisibility(
    const PrefService* prefs,
    int suggestion_group_id) {
  DCHECK(prefs);

  const base::Value::Dict& dictionary =
      prefs->GetDict(kSuggestionGroupVisibility);

  std::optional<int> value =
      dictionary.FindInt(base::NumberToString(suggestion_group_id));

  if (value == SuggestionGroupVisibility::HIDDEN ||
      value == SuggestionGroupVisibility::SHOWN) {
    return static_cast<SuggestionGroupVisibility>(*value);
  }

  return SuggestionGroupVisibility::DEFAULT;
}

void SetUserPreferenceForSuggestionGroupVisibility(
    PrefService* prefs,
    int suggestion_group_id,
    SuggestionGroupVisibility visibility) {
  DCHECK(prefs);

  ScopedDictPrefUpdate update(prefs, kSuggestionGroupVisibility);
  update->Set(base::NumberToString(suggestion_group_id), visibility);

  base::UmaHistogramEnumeration(
      visibility == SuggestionGroupVisibility::SHOWN
          ? kGroupIdToggledOnHistogram
          : kGroupIdToggledOffHistogram,
      ToUMAGroupId(GroupIdForNumber(suggestion_group_id)));
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
