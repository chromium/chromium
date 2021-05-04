// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"

#include "base/check.h"
#include "base/metrics/sparse_histogram.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace omnibox {

const char kToggleSuggestionGroupIdOffHistogram[] =
    "Omnibox.ToggleSuggestionGroupId.Off";
const char kToggleSuggestionGroupIdOnHistogram[] =
    "Omnibox.ToggleSuggestionGroupId.On";

// A client-side toggle for document (Drive) suggestions.
// Also gated by a feature and server-side Admin Panel controls.
const char kDocumentSuggestEnabled[] = "documentsuggest.enabled";

// Enum specifying the active behavior for the intranet redirect detector.
// The browser pref kDNSInterceptionChecksEnabled also impacts the redirector.
// Values are defined in omnibox::IntranetRedirectorBehavior.
const char kIntranetRedirectBehavior[] = "browser.intranet_redirect_behavior";

// A dictionary of visibility preferences for suggestion groups. The key is the
// suggestion group ID serialized as a string, and the value is
// SuggestionGroupVisibility serialized as an integer.
const char kSuggestionGroupVisibility[] = "omnibox.suggestionGroupVisibility";

// Boolean that specifies whether to always show full URLs in the omnibox.
const char kPreventUrlElisionsInOmnibox[] = "omnibox.prevent_url_elisions";

// A cache of zero suggest results using JSON serialized into a string.
const char kZeroSuggestCachedResults[] = "zerosuggest.cachedresults";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSuggestionGroupVisibility);
}

SuggestionGroupVisibility GetUserPreferenceForSuggestionGroupVisibility(
    PrefService* prefs,
    int suggestion_group_id) {
  DCHECK(prefs);

  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(kSuggestionGroupVisibility);
  DCHECK(dictionary);

  base::Optional<int> value =
      dictionary->FindIntKey(base::NumberToString(suggestion_group_id));

  if (value == SuggestionGroupVisibility::HIDDEN ||
      value == SuggestionGroupVisibility::SHOWN) {
    return static_cast<SuggestionGroupVisibility>(*value);
  }

  return SuggestionGroupVisibility::DEFAULT;
}

void SetSuggestionGroupVisibility(PrefService* prefs,
                                  int suggestion_group_id,
                                  SuggestionGroupVisibility new_value) {
  DCHECK(prefs);

  DictionaryPrefUpdate update(prefs, kSuggestionGroupVisibility);
  update->SetIntKey(base::NumberToString(suggestion_group_id), new_value);

  base::SparseHistogram::FactoryGet(
      new_value == SuggestionGroupVisibility::SHOWN
          ? kToggleSuggestionGroupIdOnHistogram
          : kToggleSuggestionGroupIdOffHistogram,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(suggestion_group_id);
}

}  // namespace omnibox
