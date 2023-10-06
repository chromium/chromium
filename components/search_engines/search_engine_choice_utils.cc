// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"

namespace search_engines {
namespace {

// The choice screen should be shown if the `DefaultSearchProviderEnabled`
// policy is not set, or set to true and the
// `DefaultSearchProviderSearchURL` policy is not set.
bool IsSearchEngineChoiceScreenAllowedByPolicy(
    const policy::PolicyService& policy_service) {
  const auto& policies = policy_service.GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const auto* default_search_provider_enabled = policies.GetValue(
      policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  // Policy is not set.
  if (!default_search_provider_enabled) {
    return true;
  }

  if (default_search_provider_enabled->GetBool()) {
    const auto* default_search_provider_search_url =
        policies.GetValue(policy::key::kDefaultSearchProviderSearchURL,
                          base::Value::Type::STRING);
    if (!default_search_provider_search_url) {
      return true;
    }
  }
  return false;
}

const base::flat_set<int> GetEeaChoiceCountries() {
  using country_codes::CountryCharsToCountryID;

  // Google-internal reference: http://go/geoscope-comparisons.
  return base::flat_set<int>({
      CountryCharsToCountryID('A', 'T'),  // Austria
      CountryCharsToCountryID('A', 'X'),  // Åland Islands
      CountryCharsToCountryID('B', 'E'),  // Belgium
      CountryCharsToCountryID('B', 'G'),  // Bulgaria
      CountryCharsToCountryID('B', 'L'),  // St. Barthélemy
      CountryCharsToCountryID('C', 'Y'),  // Cyprus
      CountryCharsToCountryID('C', 'Z'),  // Czech Republic
      CountryCharsToCountryID('D', 'E'),  // Germany
      CountryCharsToCountryID('D', 'K'),  // Denmark
      CountryCharsToCountryID('E', 'A'),  // Ceuta & Melilla
      CountryCharsToCountryID('E', 'E'),  // Estonia
      CountryCharsToCountryID('E', 'S'),  // Spain
      CountryCharsToCountryID('F', 'I'),  // Finland
      CountryCharsToCountryID('F', 'R'),  // France
      CountryCharsToCountryID('G', 'F'),  // French Guiana
      CountryCharsToCountryID('G', 'P'),  // Guadeloupe
      CountryCharsToCountryID('G', 'R'),  // Greece
      CountryCharsToCountryID('H', 'R'),  // Croatia
      CountryCharsToCountryID('H', 'U'),  // Hungary
      CountryCharsToCountryID('I', 'C'),  // Canary Islands
      CountryCharsToCountryID('I', 'E'),  // Ireland
      CountryCharsToCountryID('I', 'S'),  // Iceland
      CountryCharsToCountryID('I', 'T'),  // Italy
      CountryCharsToCountryID('L', 'I'),  // Liechtenstein
      CountryCharsToCountryID('L', 'T'),  // Lithuania
      CountryCharsToCountryID('L', 'U'),  // Luxembourg
      CountryCharsToCountryID('L', 'V'),  // Latvia
      CountryCharsToCountryID('M', 'F'),  // St. Martin
      CountryCharsToCountryID('M', 'Q'),  // Martinique
      CountryCharsToCountryID('M', 'T'),  // Malta
      CountryCharsToCountryID('N', 'C'),  // New Caledonia
      CountryCharsToCountryID('N', 'L'),  // Netherlands
      CountryCharsToCountryID('N', 'O'),  // Norway
      CountryCharsToCountryID('P', 'F'),  // French Polynesia
      CountryCharsToCountryID('P', 'L'),  // Poland
      CountryCharsToCountryID('P', 'M'),  // St. Pierre & Miquelon
      CountryCharsToCountryID('P', 'T'),  // Portugal
      CountryCharsToCountryID('R', 'E'),  // Réunion
      CountryCharsToCountryID('R', 'O'),  // Romania
      CountryCharsToCountryID('S', 'E'),  // Sweden
      CountryCharsToCountryID('S', 'I'),  // Slovenia
      CountryCharsToCountryID('S', 'J'),  // Svalbard & Jan Mayen
      CountryCharsToCountryID('S', 'K'),  // Slovakia
      CountryCharsToCountryID('T', 'F'),  // French Southern Territories
      CountryCharsToCountryID('V', 'A'),  // Vatican City
      CountryCharsToCountryID('W', 'F'),  // Wallis & Futuna
      CountryCharsToCountryID('Y', 'T'),  // Mayotte
  });
}

}  // namespace

const char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";

const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
    "Search.ChoiceScreenProfileInitConditions";

const char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";

// Returns whether the choice screen flag is generally enabled for the specific
// user flow.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo) {
  switch (promo) {
    case ChoicePromo::kAny:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoice) ||
             base::FeatureList::IsEnabled(switches::kSearchEngineChoiceFre);
    case ChoicePromo::kDialog:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoice);
    case ChoicePromo::kFre:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoiceFre);
  }
}

bool ShouldShowUpdatedSettings(PrefService& profile_prefs) {
  return IsChoiceScreenFlagEnabled(ChoicePromo::kAny) &&
         IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(&profile_prefs));
}

bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties,
                            TemplateURLService* template_url_service) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return false;
  }

  if (!profile_properties.is_regular_profile) {
    return false;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  // A command line argument with the option for disabling the choice screen for
  // testing and automation environments.
  if (command_line->HasSwitch(switches::kDisableSearchEngineChoiceScreen)) {
    return false;
  }
  // Force-enable the choice screen for testing the screen itself.
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return true;
  }

  // TODO(b/302687046): Change `template_url_service` to a reference once the
  // code is updated on iOS side.
  if (template_url_service) {
    // A custom search engine will have a `prepopulate_id` of 0.
    const int kCustomSearchEnginePrepopulateId = 0;
    const TemplateURL* default_search_engine =
        template_url_service->GetDefaultSearchProvider();
    // Don't show the dialog if the user as a custom search engine set a
    // default.
    if (default_search_engine->prepopulate_id() ==
        kCustomSearchEnginePrepopulateId) {
      RecordChoiceScreenProfileInitCondition(
          SearchEngineChoiceScreenConditions::kHasCustomSearchEngine);
      return false;
    }
  }

  PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());

  // The timestamp indicates that the user has already made a search engine
  // choice in the choice screen.
  if (prefs.GetInt64(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    RecordChoiceScreenProfileInitCondition(
        SearchEngineChoiceScreenConditions::kAlreadyCompleted);
    return false;
  }

  if (!IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(&prefs))) {
    RecordChoiceScreenProfileInitCondition(
        SearchEngineChoiceScreenConditions::kNotInRegionalScope);
    return false;
  }

  // Initially exclude users with this type of override. Consult b/302675777 for
  // next steps.
  if (prefs.HasPrefPath(prefs::kSearchProviderOverrides)) {
    RecordChoiceScreenProfileInitCondition(
        SearchEngineChoiceScreenConditions::kSearchProviderOverride);
    return false;
  }

  if (!IsSearchEngineChoiceScreenAllowedByPolicy(policy_service)) {
    RecordChoiceScreenProfileInitCondition(
        SearchEngineChoiceScreenConditions::kControlledByPolicy);
    return false;
  }

  RecordChoiceScreenProfileInitCondition(
      SearchEngineChoiceScreenConditions::kEligible);
  return true;
}

int GetSearchEngineChoiceCountryId(PrefService* profile_prefs) {
  int command_line_country = country_codes::CountryStringToCountryID(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSearchEngineChoiceCountry));
  if (command_line_country != country_codes::kCountryIDUnknown) {
    return command_line_country;
  }

  return country_codes::GetCountryIDFromPrefs(profile_prefs);
}

bool IsEeaChoiceCountry(int country_id) {
  return GetEeaChoiceCountries().contains(country_id);
}

void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      condition);
}

void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event) {
  base::UmaHistogramEnumeration(
      search_engines::kSearchEngineChoiceScreenEventsHistogram, event);
}

}  // namespace search_engines
