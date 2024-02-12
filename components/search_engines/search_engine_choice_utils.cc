// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include <string>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/grit/components_scaled_resources.h"  // nogncheck
#include "ui/resources/grit/ui_resources.h"               // nogncheck
#endif

namespace search_engines {

namespace {
#if !BUILDFLAG(IS_ANDROID)
// Defines `kSearchEngineResourceIdMap`.
#include "components/search_engines/generated_search_engine_resource_ids-inc.cc"
#endif
}  // namespace

const char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";

const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
    "Search.ChoiceScreenProfileInitConditions";

const char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";

const char kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
    "Search.ChoiceScreenDefaultSearchEngineType";

const char kSearchEngineChoiceWipeReasonHistogram[] = "Search.ChoiceWipeReason";

const char kSearchEngineChoiceRepromptHistogram[] = "Search.ChoiceReprompt";

const char kSearchEngineChoiceRepromptWildcardHistogram[] =
    "Search.ChoiceReprompt.Wildcard";

const char kSearchEngineChoiceRepromptSpecificCountryHistogram[] =
    "Search.ChoiceReprompt.SpecificCountry";

const char kSearchEngineChoiceUnexpectedIdHistogram[] =
    "Search.ChoiceDebug.UnexpectedSearchEngineId";

const char kSearchEngineChoiceIsDefaultProviderAddedToChoicesHistogram[] =
    "Search.ChoiceDebug.IsDefaultProviderAddedToChoices";

// Returns whether the choice screen flag is generally enabled for the specific
// user flow.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo) {
  if (!base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)) {
    return false;
  }

#if BUILDFLAG(IS_IOS)
  // Chrome on iOS does not tag profiles, so this param instead determines
  // whether we show the choice screen outside of the FRE or not.
  if (switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get() &&
      promo == ChoicePromo::kDialog) {
    return false;
  }
#endif

  return true;
}

bool IsEeaChoiceCountry(int country_id) {
  return kEeaChoiceCountriesIds.contains(country_id);
}

void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenProfileInitConditionsHistogram, condition);
}

void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event) {
  base::UmaHistogramEnumeration(kSearchEngineChoiceScreenEventsHistogram,
                                event);

  if (event == SearchEngineChoiceScreenEvents::kChoiceScreenWasDisplayed ||
      event == SearchEngineChoiceScreenEvents::kFreChoiceScreenWasDisplayed ||
      event == SearchEngineChoiceScreenEvents::
                   kProfileCreationChoiceScreenWasDisplayed) {
    base::RecordAction(
        base::UserMetricsAction("SearchEngineChoiceScreenShown"));
  }
}

void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram, engine_type,
      SEARCH_ENGINE_MAX);
}

void RecordUnexpectedSearchProvider(const TemplateURLData& data) {
  base::UmaHistogramSparse(kSearchEngineChoiceUnexpectedIdHistogram,
                           data.prepopulate_id);
}

void RecordIsDefaultProviderAddedToChoices(bool inserted_default) {
  base::UmaHistogramBoolean(
      kSearchEngineChoiceIsDefaultProviderAddedToChoicesHistogram,
      inserted_default);
}

void WipeSearchEngineChoicePrefs(PrefService& profile_prefs,
                                 WipeSearchEngineChoiceReason reason) {
  if (IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceWipeReasonHistogram,
                                  reason);
    profile_prefs.ClearPref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
    profile_prefs.ClearPref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion);
  }
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetMarketingSnippetString(
    const TemplateURLData& template_url_data) {
  int snippet_resource_id =
      GetMarketingSnippetResourceId(template_url_data.keyword());

  return snippet_resource_id == -1
             ? l10n_util::GetStringFUTF16(
                   IDS_SEARCH_ENGINE_FALLBACK_MARKETING_SNIPPET,
                   template_url_data.short_name())
             : l10n_util::GetStringUTF16(snippet_resource_id);
}

int GetIconResourceId(const std::u16string& engine_keyword) {
  // `kSearchEngineResourceIdMap` is defined in
  // `components/search_engines/generated_search_engine_resource_ids-inc.cc`
  const base::fixed_flat_map<std::u16string_view, int,
                             kSearchEngineResourceIdMap.size()>::const_iterator
      iterator = kSearchEngineResourceIdMap.find(engine_keyword);
  return iterator == kSearchEngineResourceIdMap.cend() ? -1 : iterator->second;
}

#endif

}  // namespace search_engines
