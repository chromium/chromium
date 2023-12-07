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

SearchEngineType GetDefaultSearchEngineType(
    TemplateURLService& template_url_service) {
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();

  return default_search_engine ? default_search_engine->GetEngineType(
                                     template_url_service.search_terms_data())
                               : SEARCH_ENGINE_OTHER;
}

}  // namespace

const char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";

const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
    "Search.ChoiceScreenProfileInitConditions";

const char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";

const char kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
    "Search.ChoiceScreenDefaultSearchEngineType";

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
  auto condition = GetStaticChoiceScreenConditions(
      policy_service, profile_properties, CHECK_DEREF(template_url_service));

  if (condition == SearchEngineChoiceScreenConditions::kEligible) {
    condition = GetDynamicChoiceScreenConditions(
        *profile_properties.pref_service, *template_url_service);
  }

  RecordChoiceScreenProfileInitCondition(condition);
  return condition == SearchEngineChoiceScreenConditions::kEligible;
}

SearchEngineChoiceScreenConditions GetStaticChoiceScreenConditions(
    const policy::PolicyService& policy_service,
    const ProfileProperties& profile_properties,
    const TemplateURLService& template_url_service) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

  if (!profile_properties.is_regular_profile) {
    // Naming not exactly accurate, but still reflect the fact that incognito,
    // kiosk, etc. are not supported and belongs in this bucked more than in
    // `kProfileOutOfScope` for example.
    return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  // A command line argument with the option for disabling the choice screen for
  // testing and automation environments.
  if (command_line->HasSwitch(switches::kDisableSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

  // Force triggering the choice screen for testing the screen itself.
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());

  // The timestamp indicates that the user has already made a search engine
  // choice in the choice screen.
  if (prefs.HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  if (!IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(&prefs))) {
    return SearchEngineChoiceScreenConditions::kNotInRegionalScope;
  }

  // Initially exclude users with this type of override. Consult b/302675777 for
  // next steps.
  if (prefs.HasPrefPath(prefs::kSearchProviderOverrides)) {
    return SearchEngineChoiceScreenConditions::kSearchProviderOverride;
  }

  if (!IsSearchEngineChoiceScreenAllowedByPolicy(policy_service)) {
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
}

SearchEngineChoiceScreenConditions GetDynamicChoiceScreenConditions(
    const PrefService& profile_prefs,
    const TemplateURLService& template_url_service) {
  // Don't show the dialog if the default search engine is set by an extension.
  if (template_url_service.IsExtensionControlledDefaultSearch()) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kExtensionControlled;
  }

  // Don't show the dialog if the user has a custom search engine set as
  // default.
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();
  if (default_search_engine &&
      !template_url_service.IsPrepopulatedOrCreatedByPolicy(
          default_search_engine)) {
    return SearchEngineChoiceScreenConditions::kHasCustomSearchEngine;
  }

  // Force triggering the choice screen for testing the screen itself.
  // Deliberately checked after the conditions overriding the default search
  // engine with some custom one because they would put the choice screens in
  // some unstable state and they are rather easy to change if we want to
  // re-enable the triggering.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  if (profile_prefs.HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kAlreadyCompleted;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
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

void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram, engine_type,
      SEARCH_ENGINE_MAX);
}

void RecordChoiceMade(PrefService* profile_prefs,
                      ChoiceMadeLocation choice_location,
                      TemplateURLService* template_url_service) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return;
  }

  // Don't modify the pref if the user is not in the EEA region.
  if (!search_engines::IsEeaChoiceCountry(
          search_engines::GetSearchEngineChoiceCountryId(profile_prefs))) {
    return;
  }

  // Don't modify the pref if it was already set.
  if (profile_prefs->HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    return;
  }

  // TODO(b/307713013): Remove the check for `template_url_service` when the
  // function is used on the iOS side.
  if (template_url_service) {
    search_engines::RecordChoiceScreenDefaultSearchProviderType(
        GetDefaultSearchEngineType(*template_url_service));
  }

  profile_prefs->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
}

}  // namespace search_engines
